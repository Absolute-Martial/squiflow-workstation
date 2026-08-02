#include "workflows/issue_invoice.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/domain/credit_account.hpp"
#include "modules/receivables/domain/document_number_block.hpp"
#include "modules/receivables/domain/invoice.hpp"
#include "modules/registry.hpp"

namespace squiflow::workflows {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
    });
}

engine::Row fields(const modules::Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw modules::RuleViolation(
            "This invoice-issue request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const modules::Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("issue_invoice: request has no session");
    }
    return *call.actor;
}

void reject_unknown_fields(const engine::Row& row) {
    static const std::set<std::string> allowed{
        "series", "expected_line_count", "expected_total_minor"};
    for (const auto& field : row.fields()) {
        if (!allowed.contains(field.first)) {
            throw modules::RuleViolation(
                "The invoice-issue request contains an unknown field: " +
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

WorkflowResult issue(engine::Transaction& transaction,
                     const modules::Call& call,
                     const IssueInvoiceClock& clock) {
    const engine::Row request = fields(call);
    reject_unknown_fields(request);
    const std::string series = required_text(
        request, "series", "Issuing an invoice requires a number series.");
    const std::int64_t expected_count = required_positive(
        request, "expected_line_count",
        "Issuing an invoice requires a positive expected line count.");
    const std::int64_t expected_total = required_positive(
        request, "expected_total_minor",
        "Issuing an invoice requires a positive expected total.");

    const engine::RecordId invoice_record =
        engine::record_id_from_string(call.record_id);
    if (!invoice_record.is_valid()) {
        throw modules::RuleViolation("The invoice identity is invalid.");
    }

    auto stored = modules::receivables::data::find_invoice(
        transaction, call.record_id);
    if (!stored) throw modules::RuleViolation("That invoice is not on file.");
    modules::receivables::Invoice invoice = *stored;
    modules::receivables::validate(invoice);
    if (invoice.state != engine::DocumentState::Draft) {
        throw modules::RuleViolation("Only a draft invoice can be issued.");
    }

    std::optional<modules::receivables::Invoice> replacement_source;
    if (!invoice.replaces_invoice_id.empty()) {
        const engine::RecordId source_record =
            engine::record_id_from_string(invoice.replaces_invoice_id);
        if (!source_record.is_valid() ||
            invoice.replaces_invoice_id == invoice.id) {
            throw modules::RuleViolation(
                "That replacement draft has an invalid source invoice.");
        }
        replacement_source = modules::receivables::data::find_invoice(
            transaction, invoice.replaces_invoice_id);
        if (!replacement_source) {
            throw modules::RuleViolation(
                "The source invoice for this replacement is not on file.");
        }
        modules::receivables::validate(*replacement_source);
        if (replacement_source->state != engine::DocumentState::Cancelled ||
            !replacement_source->replacement_invoice_id.empty()) {
            throw modules::RuleViolation(
                "The source invoice is not waiting for this replacement.");
        }
        if (replacement_source->party_id != invoice.party_id) {
            throw modules::RuleViolation(
                "A replacement invoice cannot move the charge to another customer.");
        }
        std::size_t active_replacements = 0;
        for (const auto& candidate :
             modules::receivables::data::replacements_for_invoice(
                 transaction, replacement_source->id)) {
            modules::receivables::validate(candidate);
            if (candidate.state != engine::DocumentState::Discarded) {
                ++active_replacements;
                if (candidate.id != invoice.id) {
                    throw modules::RuleViolation(
                        "The source invoice has another active replacement.");
                }
            }
        }
        if (active_replacements != 1U) {
            throw modules::RuleViolation(
                "The replacement relationship is incomplete or contradictory.");
        }
    }

    const std::vector<modules::receivables::InvoiceLine> lines =
        modules::receivables::data::lines_for_invoice(transaction, invoice.id);
    if (lines.empty()) {
        throw modules::RuleViolation("An empty invoice cannot be issued.");
    }
    for (const auto& line : lines) {
        modules::receivables::validate(line);
        if (line.invoice_id != invoice.id) {
            throw modules::RuleViolation(
                "An invoice line does not belong to this draft.");
        }
    }
    if (static_cast<std::uint64_t>(expected_count) !=
        static_cast<std::uint64_t>(lines.size())) {
        throw modules::RuleViolation(
            "The invoice line count changed after it was confirmed.");
    }
    const engine::MoneyResult total =
        modules::receivables::invoice_total(lines);
    if (!total.ok || total.value.minor <= 0) {
        throw modules::RuleViolation(
            "The invoice does not have a usable positive total.");
    }
    if (total.value.minor != expected_total) {
        throw modules::RuleViolation(
            "The invoice total changed after it was confirmed.");
    }

    const std::int64_t issued_at = clock();
    if (issued_at <= 0) {
        throw modules::RuleViolation("The invoice issue time is invalid.");
    }
    if (!invoice.party_id.empty()) {
        const auto account = modules::receivables::data::find_credit_account(
            transaction, invoice.party_id);
        if (account) {
            modules::receivables::validate(*account);
            const modules::receivables::DueAtResult due =
                modules::receivables::credit_due_at(
                    issued_at, account->credit_period_days);
            if (!due.ok) {
                throw modules::RuleViolation(
                    "The customer credit due date cannot be calculated safely.");
            }
            invoice.due_at = due.value;
        }
    }
    if (invoice.due_at != 0 && invoice.due_at < issued_at) {
        throw modules::RuleViolation(
            "The invoice due date is already before its issue time.");
    }

    const engine::Session& who = actor(call);
    const std::string device_id = engine::to_string(who.device);
    auto block = modules::receivables::data::available_number_block(
        transaction, modules::receivables::NumberedDocumentKind::Invoice,
        series, device_id);
    if (!block) {
        throw modules::RuleViolation(
            "This device has no reserved invoice number left in that series.");
    }
    const auto number = modules::receivables::allocate(*block);
    if (!number) {
        throw modules::RuleViolation(
            "This device's reserved invoice number block is exhausted.");
    }

    // Persist consumption before the defensive duplicate check on purpose.
    // Both writes are still inside the registry transaction, so damaged data
    // that reveals a duplicate proves the consumed number rolls back rather
    // than merely relying on validation order to leave the row untouched.
    modules::receivables::data::save_number_block(transaction, *block);

    // A server must never issue overlapping ranges, but this final local check
    // fails closed if damaged or imported data contradicts that promise.
    if (modules::receivables::data::invoice_by_number(
            transaction, series, *number)) {
        throw modules::RuleViolation(
            "That final invoice number is already in use. No number was consumed.");
    }

    invoice.state = engine::DocumentState::Issued;
    invoice.number_series = series;
    invoice.number = *number;
    invoice.issued_at = issued_at;
    invoice.issued_by = engine::to_string(who.person);
    modules::receivables::data::save_invoice(transaction, invoice);
    if (replacement_source) {
        replacement_source->state = engine::DocumentState::Replaced;
        replacement_source->replacement_invoice_id = invoice.id;
        modules::receivables::data::save_invoice(
            transaction, *replacement_source);
    }

    return {{protocol::ModuleId::receivables, invoice_record},
            "Issued invoice " + series + "-" + std::to_string(*number) +
                " with " + std::to_string(lines.size()) + " line(s), total " +
                std::to_string(total.value.minor) + " minor units."};
}

}  // namespace

WorkflowDefinition make_issue_invoice(IssueInvoiceClock clock) {
    if (!clock) throw modules::RegistryError("issue_invoice needs a clock");
    WorkflowDefinition definition;
    definition.operation = protocol::OperationId::issue_invoice;
    definition.requirements = {protocol::ModuleId::receivables,
                               protocol::ModuleId::orders,
                               protocol::ModuleId::pricing};
    definition.handler = [clock = std::move(clock)](
        engine::Transaction& transaction, const modules::Call& call) {
        return issue(transaction, call, clock);
    };
    return definition;
}

}  // namespace squiflow::workflows
