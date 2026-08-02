#include "workflows/take_payment.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/domain/payment.hpp"
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
            "This payment request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const modules::Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("take_payment: request has no session");
    }
    return *call.actor;
}

void reject_unknown_fields(const engine::Row& row) {
    static const std::set<std::string> allowed{
        "party_id", "amount_minor", "paid_at", "method",
        "external_reference", "note", "receipt_series", "receipt_number"};
    for (const auto& field : row.fields()) {
        if (!allowed.contains(field.first)) {
            throw modules::RuleViolation(
                "The payment request contains an unknown field: " +
                field.first + ".");
        }
    }
}

std::string required_text(const engine::Row& row, const char* name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) throw modules::RuleViolation(complaint);
    return *value;
}

std::string optional_text(const engine::Row& row, const char* name) {
    const engine::Value& value = row.get(name);
    if (value.is_null()) return {};
    const std::string* text = value.as_text();
    if (text == nullptr) {
        throw modules::RuleViolation(std::string("Payment field '") + name +
                                     "' must be text when supplied.");
    }
    return *text;
}

std::int64_t required_positive(const engine::Row& row, const char* name,
                               const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value || *value <= 0) throw modules::RuleViolation(complaint);
    return *value;
}

WorkflowResult take(engine::Transaction& transaction,
                    const modules::Call& call,
                    const TakePaymentClock& clock) {
    const engine::Row request = fields(call);
    reject_unknown_fields(request);
    const engine::RecordId payment_record =
        engine::record_id_from_string(call.record_id);
    if (!payment_record.is_valid()) {
        throw modules::RuleViolation("The payment identity is invalid.");
    }
    if (modules::receivables::data::find_payment(transaction, call.record_id)) {
        throw modules::RuleViolation("That payment identity is already in use.");
    }

    const std::string party_id = required_text(
        request, "party_id", "Recording customer credit requires a customer.");
    if (!engine::record_id_from_string(party_id).is_valid()) {
        throw modules::RuleViolation("The payment customer identity is invalid.");
    }
    const auto party = modules::parties::data::find_party(transaction, party_id);
    if (!party) throw modules::RuleViolation("That customer is not on file.");
    modules::parties::validate(*party);
    if (!party->is_customer || party->archived) {
        throw modules::RuleViolation("Payments require an active customer.");
    }

    const std::int64_t recorded_at = clock();
    if (recorded_at <= 0) {
        throw modules::RuleViolation("The payment recording time is invalid.");
    }
    modules::receivables::Payment payment;
    payment.id = call.record_id;
    payment.party_id = party_id;
    payment.amount_minor = required_positive(
        request, "amount_minor", "A payment amount must be greater than zero.");
    payment.paid_at = required_positive(
        request, "paid_at", "A payment must record when it was paid.");
    payment.method = required_text(
        request, "method", "A payment must record a simple method such as cash, bank, or cheque.");
    payment.external_reference = optional_text(request, "external_reference");
    payment.note = optional_text(request, "note");
    payment.receipt_series = optional_text(request, "receipt_series");
    const engine::Value& receipt_number = request.get("receipt_number");
    if (!receipt_number.is_null()) {
        const auto value = receipt_number.as_integer();
        if (!value || *value <= 0) {
            throw modules::RuleViolation(
                "An optional receipt number must be a positive number.");
        }
        payment.receipt_number = static_cast<std::uint64_t>(*value);
    }
    payment.recorded_at = recorded_at;
    payment.recorded_by = engine::to_string(actor(call).person);
    modules::receivables::validate(payment);
    modules::receivables::data::save_payment(transaction, payment);

    std::string detail = "Recorded " + payment.method + " payment of " +
                         std::to_string(payment.amount_minor) + " minor units";
    if (!blank(payment.external_reference)) {
        detail += " with optional reference " + payment.external_reference;
    }
    detail += "; all money remains unallocated.";
    return {{protocol::ModuleId::receivables, payment_record}, detail};
}

}  // namespace

WorkflowDefinition make_take_payment(TakePaymentClock clock) {
    if (!clock) throw modules::RegistryError("take_payment needs a clock");
    WorkflowDefinition definition;
    definition.operation = protocol::OperationId::take_payment;
    definition.requirements = {protocol::ModuleId::receivables,
                               protocol::ModuleId::parties};
    definition.handler = [clock = std::move(clock)](
        engine::Transaction& transaction, const modules::Call& call) {
        return take(transaction, call, clock);
    };
    return definition;
}

}  // namespace squiflow::workflows
