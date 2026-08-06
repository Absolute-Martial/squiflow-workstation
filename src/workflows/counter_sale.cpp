#include "workflows/counter_sale.hpp"

#include <algorithm>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/domain/order.hpp"
#include "modules/pricing/service/pricing_service.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/domain/payment.hpp"
#include "modules/registry.hpp"

namespace squiflow::workflows {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](char character) {
        return static_cast<unsigned char>(character) <=
               static_cast<unsigned char>(' ');
    });
}

engine::Row fields(const modules::Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw modules::RuleViolation(
            "This counter sale could not be read. Please try it again.");
    }
}

const engine::Session& actor(const modules::Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("counter_sale: request has no session");
    }
    return *call.actor;
}

void reject_unknown_fields(const engine::Row& row) {
    static const std::set<std::string> allowed{
        "line_id", "payment_id", "party_id", "product_id", "description",
        "quantity_scaled", "unit_price_minor", "price_reason", "note",
        "paid_at", "method", "external_reference", "receipt_series",
        "receipt_number"};
    for (const auto& field : row.fields()) {
        if (!allowed.contains(field.first)) {
            throw modules::RuleViolation(
                "The counter sale contains an unknown field: " +
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

std::string optional_text(const engine::Row& row, const char* name) {
    const engine::Value& value = row.get(name);
    if (value.is_null()) {
        return {};
    }
    const std::string* text = value.as_text();
    if (text == nullptr) {
        throw modules::RuleViolation(std::string("Counter-sale field '") + name +
                                     "' must be text when supplied.");
    }
    return *text;
}

std::int64_t required_positive(const engine::Row& row, const char* name,
                               const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value || *value <= 0) {
        throw modules::RuleViolation(complaint);
    }
    return *value;
}

std::optional<std::int64_t> optional_nonnegative(const engine::Row& row,
                                                 const char* name) {
    const engine::Value& value = row.get(name);
    if (value.is_null()) {
        return std::nullopt;
    }
    const auto amount = value.as_integer();
    if (!amount || *amount < 0) {
        throw modules::RuleViolation(std::string("Counter-sale field '") + name +
                                     "' must be a nonnegative amount.");
    }
    return amount;
}

void require_record_id(const std::string& value, const char* complaint) {
    if (!engine::record_id_from_string(value).is_valid()) {
        throw modules::RuleViolation(complaint);
    }
}

WorkflowResult take_sale(engine::Transaction& transaction,
                         const modules::Call& call,
                         const CounterSaleClock& clock) {
    const engine::Row request = fields(call);
    reject_unknown_fields(request);
    require_record_id(call.record_id, "The counter-sale identity is invalid.");
    const std::string line_id = required_text(
        request, "line_id", "A counter sale must identify its line.");
    const std::string payment_id = required_text(
        request, "payment_id", "A counter sale must identify its payment.");
    require_record_id(line_id, "The counter-sale line identity is invalid.");
    require_record_id(payment_id, "The counter-sale payment identity is invalid.");
    if (modules::orders::data::find_order(transaction, call.record_id) ||
        modules::orders::data::find_line(transaction, line_id) ||
        modules::receivables::data::find_payment(transaction, payment_id)) {
        throw modules::RuleViolation("That counter-sale identity is already in use.");
    }

    const std::int64_t recorded_at = clock();
    if (recorded_at <= 0) {
        throw modules::RuleViolation("The counter-sale recording time is invalid.");
    }
    const std::string person = engine::to_string(actor(call).person);
    const std::string party_id = optional_text(request, "party_id");
    if (!party_id.empty()) {
        require_record_id(party_id, "The counter-sale customer identity is invalid.");
    }
    const std::string product_id = optional_text(request, "product_id");
    const std::string description = optional_text(request, "description");
    const std::int64_t quantity = required_positive(
        request, "quantity_scaled", "A counter-sale quantity must be greater than zero.");

    const auto resolved = modules::pricing::effective_price(
        transaction, line_id, product_id, party_id, recorded_at);
    const auto entered_price = optional_nonnegative(request, "unit_price_minor");
    const std::string price_reason = optional_text(request, "price_reason");
    if (!entered_price && !resolved.found) {
        throw modules::RuleViolation(
            "That counter-sale line has no remembered price. Enter one explicitly.");
    }
    if (entered_price && blank(price_reason)) {
        throw modules::RuleViolation(
            "An explicitly entered counter-sale price needs a reason.");
    }
    if (!entered_price && !blank(price_reason)) {
        throw modules::RuleViolation(
            "A price reason cannot exist without an explicitly entered price.");
    }

    modules::orders::Order order;
    order.id = call.record_id;
    order.party_id = party_id;
    order.note = optional_text(request, "note");
    order.created_at = recorded_at;
    order.created_by = person;
    modules::orders::data::save_order(transaction, order);

    modules::orders::OrderLine line;
    line.id = line_id;
    line.order_id = order.id;
    line.product_id = product_id;
    line.description = description;
    line.quantity_scaled = quantity;
    line.unit_price_minor = entered_price.value_or(resolved.amount_minor);
    line.price_source = resolved.source;
    line.price_overridden = entered_price.has_value();
    line.price_reason = price_reason;
    line.added_at = recorded_at;
    line.added_by = person;
    modules::orders::data::save_line(transaction, line);
    const engine::MoneyResult total = modules::orders::line_amount(line);
    if (!total.ok || total.value.minor <= 0) {
        throw modules::RuleViolation(
            "The counter-sale total must be positive and fit in the register.");
    }

    modules::receivables::Payment payment;
    payment.id = payment_id;
    payment.party_id = party_id;
    payment.amount_minor = total.value.minor;
    payment.paid_at = required_positive(
        request, "paid_at", "A counter sale must record when it was paid.");
    payment.method = required_text(
        request, "method", "A counter sale must record how it was paid.");
    payment.external_reference = optional_text(request, "external_reference");
    payment.note = order.note;
    payment.receipt_series = optional_text(request, "receipt_series");
    const engine::Value& receipt_number = request.get("receipt_number");
    if (!receipt_number.is_null()) {
        const auto number = receipt_number.as_integer();
        if (!number || *number <= 0) {
            throw modules::RuleViolation(
                "A counter-sale receipt number must be positive when supplied.");
        }
        payment.receipt_number = static_cast<std::uint64_t>(*number);
    }
    payment.recorded_at = recorded_at;
    payment.recorded_by = person;
    modules::receivables::data::save_payment(transaction, payment);

    return {{protocol::ModuleId::orders,
             engine::record_id_from_string(order.id)},
            "Recorded counter sale and payment of " +
                std::to_string(payment.amount_minor) + " minor units."};
}

}  // namespace

WorkflowDefinition make_counter_sale(CounterSaleClock clock) {
    if (!clock) {
        throw modules::RegistryError("counter_sale needs a clock");
    }
    WorkflowDefinition definition;
    definition.operation = protocol::OperationId::counter_sale;
    definition.requirements = {protocol::ModuleId::orders,
                               protocol::ModuleId::pricing,
                               protocol::ModuleId::receivables};
    definition.handler = [clock = std::move(clock)](
        engine::Transaction& transaction, const modules::Call& call) {
        return take_sale(transaction, call, clock);
    };
    return definition;
}

}  // namespace squiflow::workflows
