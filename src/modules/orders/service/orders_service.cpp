#include "modules/orders/service/orders_service.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"

namespace squiflow::modules::orders {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

std::string required_text(const engine::Row& fields, const char* field, const char* complaint) {
    const std::string* value = fields.get(field).as_text();
    if (value == nullptr || blank(*value)) {
        throw RuleViolation(complaint);
    }
    return *value;
}

// Optional means the field may be absent. It does not mean a value of the
// wrong type is silently treated as absent: that would turn a malformed
// customer into a walk-in or clear a note without anybody asking.
std::string optional_text(const engine::Row& fields,
                          const char* field,
                          const char* complaint) {
    if (!fields.has(field)) {
        return {};
    }
    const std::string* value = fields.get(field).as_text();
    if (value == nullptr) {
        throw RuleViolation(complaint);
    }
    return *value;
}

// A quantity must arrive as a number. integer_or would quietly turn a quantity
// sent as text into the fallback, and a fallback of zero is a line for nothing
// at all, so a non-integer is refused instead of defaulted.
std::int64_t required_number(const engine::Row& fields, const char* field, const char* complaint) {
    const auto value = fields.get(field).as_integer();
    if (!value) {
        throw RuleViolation(complaint);
    }
    return *value;
}

std::int64_t optional_number(const engine::Row& fields,
                             const char* field,
                             const char* complaint) {
    if (!fields.has(field)) {
        return 0;
    }
    return required_number(fields, field, complaint);
}

std::string subject(const Call& call) {
    if (call.record_id.empty()) {
        throw RuleViolation("This request does not say which record it is about.");
    }
    return call.record_id;
}

// The order this call is about, or a refusal naming the problem. Every handler
// but create begins here, so the message is written once.
template <typename Reader>
Order existing_order(const Reader& reader, const std::string& id) {
    const auto found = data::find_order(reader, id);
    if (!found) {
        throw RuleViolation("That order is not on file.");
    }
    return *found;
}

}  // namespace

engine::Row read_fields(const Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        // The bytes did not decode. The person cannot fix a wire format, but
        // they can be told the request did not survive the trip rather than
        // being shown a storage-layer exception.
        throw RuleViolation("This request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("orders: a write arrived with no session");
    }
    return *call.actor;
}

void OrdersService::create(engine::Transaction& transaction, const Call& call) const {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    // Creating over an order that already exists would silently discard
    // whatever was agreed before, including its lines, which stay behind
    // pointing at an order that no longer says what they were priced under.
    if (data::find_order(transaction, id)) {
        throw RuleViolation("That order has already been recorded.");
    }

    Order order;
    order.id = id;
    // Absent is a walk-in, not a mistake. Present with the wrong type is a
    // malformed request, not permission to turn a customer order into one.
    order.party_id = optional_text(
        fields, "party_id", "That customer could not be read as a customer record.");
    order.state = OrderState::Open;
    order.promised_at = optional_number(
        fields, "promised_at", "That promised date could not be read as a date.");
    order.note = optional_text(fields, "note", "That order note could not be read as text.");
    order.created_at = clock_();
    order.created_by = engine::to_string(who.person);

    data::save_order(transaction, order);
}

void OrdersService::update(engine::Transaction& transaction, const Call& call) const {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);
    static_cast<void>(actor(call));

    Order order = existing_order(transaction, id);
    if (!can_change(order.state)) {
        throw RuleViolation("A cancelled order cannot be changed.");
    }

    // The customer is not an editable field. Moving an order to a different
    // customer would move its money too, and the lines were priced against the
    // original customer's agreed rates.
    if (fields.has("party_id")) {
        const std::string proposed = optional_text(
            fields, "party_id", "That customer could not be read as a customer record.");
        if (proposed != order.party_id) {
            throw RuleViolation(
                "An order cannot be moved to a different customer. Record a new order instead.");
        }
    }

    // Only the fields actually sent are touched. Absent means unchanged, not
    // cleared, so a screen that edits the note cannot wipe the promised date it
    // never showed.
    if (fields.has("promised_at")) {
        order.promised_at = required_number(
            fields, "promised_at", "That promised date could not be read as a date.");
    }
    if (fields.has("note")) {
        order.note = optional_text(fields, "note", "That order note could not be read as text.");
    }

    data::save_order(transaction, order);
}

void OrdersService::add_line(engine::Transaction& transaction, const Call& call) const {
    const engine::Row fields = read_fields(call);
    const std::string line_id = subject(call);
    const engine::Session& who = actor(call);

    const std::string order_id =
        required_text(fields, "order_id", "An order line must say which order it belongs to.");

    const Order order = existing_order(transaction, order_id);
    if (!can_change(order.state)) {
        throw RuleViolation("A cancelled order cannot take new lines.");
    }
    if (data::find_line(transaction, line_id)) {
        throw RuleViolation("That order line has already been recorded.");
    }

    OrderLine line;
    line.id = line_id;
    line.order_id = order_id;
    line.product_id = optional_text(
        fields, "product_id", "That product could not be read as a product record.");
    line.description = optional_text(
        fields, "description", "That line description could not be read as text.");
    line.quantity_scaled = required_number(
        fields, "quantity_scaled", "That quantity could not be read as a number.");
    line.added_at = clock_();
    line.added_by = engine::to_string(who.person);
    const auto position = data::next_position(transaction, order_id);
    if (!position) {
        throw RuleViolation("That order has no position left for another line.");
    }
    line.position = *position;

    // The snapshot. Asked once, here, against this order's customer and this
    // moment, and then never asked again for the life of the line.
    const pricing::EffectivePrice price = pricing::effective_price(
        transaction, line_id, line.product_id, order.party_id, line.added_at);
    if (!price.found) {
        // No rate, no standard price, no override. Guessing zero would put a
        // free item on the order and nobody would notice until the invoice.
        throw RuleViolation(
            "There is no price for this item. Set a price for it, or record a price change "
            "for this line, before adding it.");
    }
    line.unit_price_minor = price.amount_minor;
    line.price_source = price.source;
    line.price_overridden = price.overridden;
    line.price_reason = price.reason;

    data::save_line(transaction, line);
}

void OrdersService::cancel(engine::Transaction& transaction, const Call& call) const {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Order order = existing_order(transaction, id);
    if (order.state == OrderState::Cancelled) {
        // Not harmless: a second cancellation would overwrite the first
        // reason, and the first reason is the one that explains what happened.
        throw RuleViolation("That order has already been cancelled.");
    }

    order.cancel_reason =
        required_text(fields, "reason", "A cancelled order must say why it was cancelled.");
    order.state = OrderState::Cancelled;
    order.cancelled_at = clock_();
    order.cancelled_by = engine::to_string(who.person);

    data::save_order(transaction, order);
}

}  // namespace squiflow::modules::orders
