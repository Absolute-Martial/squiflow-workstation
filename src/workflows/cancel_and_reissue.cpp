#include "workflows/cancel_and_reissue.hpp"

#include <algorithm>
#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/agreements/data/repository.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/domain/invoice.hpp"
#include "modules/receivables/domain/payment.hpp"
#include "modules/registry.hpp"

namespace squiflow::workflows {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
    });
}

int nibble(const char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

void require_record_id(const std::string& value, const char* complaint) {
    if (value.size() != 32U ||
        std::any_of(value.begin(), value.end(),
                    [](const char c) { return nibble(c) < 0; }) ||
        !engine::record_id_from_string(value).is_valid()) {
        throw modules::RuleViolation(complaint);
    }
}

engine::Row fields(const modules::Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw modules::RuleViolation(
            "This cancel-and-reissue request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const modules::Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("cancel_and_reissue: request has no session");
    }
    return *call.actor;
}

void reject_unknown_fields(const engine::Row& row) {
    static const std::set<std::string> allowed{
        "replacement_invoice_id", "reason", "expected_series",
        "expected_number", "expected_line_count", "expected_total_minor"};
    for (const auto& field : row.fields()) {
        if (!allowed.contains(field.first)) {
            throw modules::RuleViolation(
                "The cancel-and-reissue request contains an unknown field: " +
                field.first + ".");
        }
    }
}

std::string required_text(const engine::Row& row, const char* name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) {
        throw modules::RuleViolation(complaint);
    }
    return *value;
}

std::int64_t required_positive(const engine::Row& row, const char* name,
                               const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value || *value <= 0) throw modules::RuleViolation(complaint);
    return *value;
}

bool active_replacement(const modules::receivables::Invoice& invoice) noexcept {
    return invoice.state != engine::DocumentState::Discarded;
}

WorkflowResult cancel(engine::Transaction& transaction,
                      const modules::Call& call,
                      const CancelAndReissueClock& clock) {
    const engine::Row request = fields(call);
    reject_unknown_fields(request);
    require_record_id(call.record_id, "The source invoice identity is invalid.");
    const std::string replacement_id = required_text(
        request, "replacement_invoice_id",
        "Cancel and reissue requires a replacement invoice identity.");
    require_record_id(replacement_id,
                      "The replacement invoice identity is invalid.");
    if (replacement_id == call.record_id) {
        throw modules::RuleViolation("An invoice cannot replace itself.");
    }
    const std::string reason = required_text(
        request, "reason", "Cancelling an invoice requires a reason.");
    const std::string expected_series = required_text(
        request, "expected_series",
        "Cancellation requires the confirmed invoice series.");
    const std::int64_t expected_number = required_positive(
        request, "expected_number",
        "Cancellation requires the confirmed invoice number.");
    const std::int64_t expected_count = required_positive(
        request, "expected_line_count",
        "Cancellation requires a positive confirmed line count.");
    const std::int64_t expected_total = required_positive(
        request, "expected_total_minor",
        "Cancellation requires a positive confirmed total.");

    auto stored = modules::receivables::data::find_invoice(
        transaction, call.record_id);
    if (!stored) throw modules::RuleViolation("That invoice is not on file.");
    modules::receivables::Invoice source = *stored;
    modules::receivables::validate(source);
    const bool first_cancellation =
        source.state == engine::DocumentState::Issued;
    if (!first_cancellation &&
        source.state != engine::DocumentState::Cancelled) {
        throw modules::RuleViolation(
            "Only an issued invoice, or a cancelled invoice whose replacement was discarded, can be reissued.");
    }
    if (!first_cancellation && source.cancel_reason != reason) {
        throw modules::RuleViolation(
            "A permanent cancellation reason cannot be changed during a later replacement attempt.");
    }
    if (source.number_series != expected_series ||
        source.number != static_cast<std::uint64_t>(expected_number)) {
        throw modules::RuleViolation(
            "The invoice number changed after cancellation was confirmed.");
    }

    const std::vector<modules::receivables::InvoiceLine> source_lines =
        modules::receivables::data::lines_for_invoice(transaction, source.id);
    if (source_lines.empty()) {
        throw modules::RuleViolation("An empty invoice cannot be reissued.");
    }
    for (const auto& line : source_lines) {
        modules::receivables::validate(line);
        if (line.invoice_id != source.id) {
            throw modules::RuleViolation(
                "An invoice line does not belong to the source invoice.");
        }
    }
    if (static_cast<std::uint64_t>(expected_count) !=
        static_cast<std::uint64_t>(source_lines.size())) {
        throw modules::RuleViolation(
            "The invoice line count changed after cancellation was confirmed.");
    }
    const engine::MoneyResult source_total =
        modules::receivables::invoice_total(source_lines);
    if (!source_total.ok || source_total.value.minor <= 0) {
        throw modules::RuleViolation(
            "The source invoice does not have a usable positive total.");
    }
    if (source_total.value.minor != expected_total) {
        throw modules::RuleViolation(
            "The invoice total changed after cancellation was confirmed.");
    }

    for (const auto& prior :
         modules::receivables::data::replacements_for_invoice(
             transaction, source.id)) {
        modules::receivables::validate(prior);
        if (active_replacement(prior)) {
            throw modules::RuleViolation(
                "That invoice already has an active replacement.");
        }
    }
    if (modules::receivables::data::find_invoice(transaction, replacement_id)) {
        throw modules::RuleViolation(
            "The replacement invoice identity is already in use.");
    }

    const std::int64_t at = clock();
    if (at <= 0 || at < source.issued_at) {
        throw modules::RuleViolation("The invoice cancellation time is invalid.");
    }
    const std::string person = engine::to_string(actor(call).person);

    std::vector<modules::receivables::InvoiceLine> copied_lines;
    copied_lines.reserve(source_lines.size());
    std::set<std::string> copied_ids;
    for (const auto& original : source_lines) {
        modules::receivables::InvoiceLine copied = original;
        copied.id = replacement_invoice_line_id(replacement_id, original.id);
        copied.invoice_id = replacement_id;
        copied.added_at = at;
        copied.added_by = person;
        if (!copied_ids.insert(copied.id).second) {
            throw modules::RuleViolation(
                "Two source lines derive the same replacement identity.");
        }
        if (modules::receivables::data::find_invoice_line(
                transaction, copied.id)) {
            throw modules::RuleViolation(
                "A replacement invoice-line identity is already in use.");
        }
        modules::receivables::validate(copied);
        copied_lines.push_back(std::move(copied));
    }

    std::vector<modules::receivables::PaymentAllocation> allocations =
        modules::receivables::data::allocations_for_target(
            transaction, protocol::ModuleId::receivables, source.id);
    for (const auto& allocation : allocations) {
        modules::receivables::validate(allocation);
        if (!first_cancellation &&
            allocation.state == modules::receivables::AllocationState::Active) {
            throw modules::RuleViolation(
                "A cancelled invoice unexpectedly has active allocated money.");
        }
    }

    if (first_cancellation) {
        for (auto usage : modules::agreements::data::consumptions_for_invoice(
                 transaction, source.id)) {
            modules::agreements::validate(usage);
            if (usage.state == modules::agreements::ConsumptionState::Released) continue;
            auto agreement_line = modules::agreements::data::find_line(
                transaction, usage.agreement_line_id);
            if (!agreement_line || agreement_line->agreement_id != usage.agreement_id) {
                throw modules::RuleViolation(
                    "Agreement consumption cannot find the quantity it must release.");
            }
            modules::agreements::validate(*agreement_line);
            const auto released = modules::agreements::release_quantity(
                *agreement_line, usage.quantity_scaled);
            if (!released.ok) {
                throw modules::RuleViolation(
                    "Agreement consumption cannot release more than was consumed.");
            }
            agreement_line->consumed_scaled = released.consumed_scaled;
            modules::agreements::data::save_line(transaction, *agreement_line);
            usage.state = modules::agreements::ConsumptionState::Released;
            usage.released_at = at;
            usage.released_by = person;
            usage.release_reason = "Invoice " + source.number_series + "-" +
                std::to_string(source.number) + " cancelled: " + reason;
            modules::agreements::data::save_consumption(transaction, usage);
        }
        source.state = engine::DocumentState::Cancelled;
        source.cancelled_at = at;
        source.cancelled_by = person;
        source.cancel_reason = reason;
        modules::receivables::data::save_invoice(transaction, source);

        for (auto& allocation : allocations) {
            if (allocation.state ==
                modules::receivables::AllocationState::Released) {
                continue;
            }
            allocation.state = modules::receivables::AllocationState::Released;
            allocation.released_at = at;
            allocation.released_by = person;
            allocation.release_reason = "Invoice " + source.number_series +
                "-" + std::to_string(source.number) + " cancelled: " + reason;
            modules::receivables::data::save_allocation(transaction, allocation);
        }
    }

    modules::receivables::Invoice replacement;
    replacement.id = replacement_id;
    replacement.party_id = source.party_id;
    replacement.due_at = source.due_at;
    replacement.note = source.note;
    replacement.created_at = at;
    replacement.created_by = person;
    replacement.replaces_invoice_id = source.id;
    modules::receivables::data::save_invoice(transaction, replacement);
    for (const auto& copied : copied_lines) {
        modules::receivables::data::save_invoice_line(transaction, copied);
    }

    return {{protocol::ModuleId::receivables,
             engine::record_id_from_string(source.id)},
            "Cancelled invoice " + source.number_series + "-" +
                std::to_string(source.number) + " and created replacement draft " +
                replacement.id + " with " +
                std::to_string(copied_lines.size()) + " line(s)."};
}

}  // namespace

std::string replacement_invoice_line_id(
    const std::string& replacement_invoice_id,
    const std::string& source_invoice_line_id) {
    require_record_id(replacement_invoice_id,
                      "The replacement invoice identity is invalid.");
    require_record_id(source_invoice_line_id,
                      "A source invoice-line identity is invalid.");
    const std::string input = replacement_invoice_id + ":" +
                              source_invoice_line_id;
    auto hash = [&](std::uint64_t value, const std::uint64_t salt) {
        value ^= salt;
        for (const char character : input) {
            const auto byte = static_cast<unsigned char>(character);
            value ^= static_cast<std::uint64_t>(byte);
            value *= 1099511628211ULL;
        }
        return value;
    };
    const std::uint64_t high = hash(14695981039346656037ULL,
                                    0xa0761d6478bd642fULL);
    const std::uint64_t low = hash(1099511628211ULL,
                                   0xe7037ed1a0b428dbULL);
    static constexpr char hex[] = "0123456789abcdef";
    std::string result(32U, '0');
    const auto write_half = [&](const std::uint64_t value,
                                const std::size_t offset) {
        for (std::size_t i = 0; i < 16U; ++i) {
            const std::size_t shift = (15U - i) * 4U;
            result[offset + i] =
                hex[static_cast<std::size_t>((value >> shift) & 0x0fU)];
        }
    };
    write_half(high, 0U);
    write_half(low, 16U);
    if (high == 0U && low == 0U) result.back() = '1';
    return result;
}

WorkflowDefinition make_cancel_and_reissue(CancelAndReissueClock clock) {
    if (!clock) {
        throw modules::RegistryError("cancel_and_reissue needs a clock");
    }
    WorkflowDefinition definition;
    definition.operation = protocol::OperationId::cancel_and_reissue;
    definition.requirements = {protocol::ModuleId::agreements,
                               protocol::ModuleId::receivables};
    definition.handler = [clock = std::move(clock)](
        engine::Transaction& transaction, const modules::Call& call) {
        return cancel(transaction, call, clock);
    };
    return definition;
}

}  // namespace squiflow::workflows
