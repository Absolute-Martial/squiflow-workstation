#include "modules/orders/domain/order.hpp"

#include <limits>

#include "modules/context.hpp"

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

// A rate source read back from a row. A value outside the enumeration means
// the row was written by a build that is not this one, or was corrupted; it
// becomes None rather than a cast to whatever bit pattern happens to be there.
pricing::RateSource source_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 1: return pricing::RateSource::PartyRate;
        case 2: return pricing::RateSource::CatchAllRate;
        case 3: return pricing::RateSource::Default;
        default: return pricing::RateSource::None;
    }
}

}  // namespace

const char* to_string(OrderState state) noexcept {
    switch (state) {
        case OrderState::Open:      return "open";
        case OrderState::Cancelled: return "cancelled";
    }
    return "?";
}

engine::MoneyResult line_amount(const OrderLine& line) noexcept {
    const engine::Money rate{line.unit_price_minor};
    const engine::Quantity quantity{line.quantity_scaled};
    return engine::money_multiply(rate, quantity);
}

engine::MoneyResult order_total(const std::vector<OrderLine>& lines) noexcept {
    engine::Money running{0};
    for (const OrderLine& line : lines) {
        const engine::MoneyResult amount = line_amount(line);
        if (!amount.ok) {
            return {false, {}};
        }
        const engine::MoneyResult sum = engine::money_add(running, amount.value);
        if (!sum.ok) {
            return {false, {}};
        }
        running = sum.value;
    }
    return {true, running};
}

void validate(const Order& order) {
    if (order.id.empty()) {
        throw RuleViolation("This order has no record to be saved under.");
    }
    // party_id may be empty. That is a walk-in: somebody who paid at the
    // counter and will never be looked up again. Forcing a customer record
    // onto every order would mean inventing one for every passer-by.
    if (order.created_at <= 0 || order.promised_at < 0) {
        throw RuleViolation("That order's dates are not a time this shop has existed.");
    }
    if (blank(order.created_by)) {
        throw RuleViolation("An order must record who created it.");
    }
    const bool direct = order.source_quotation_id.empty() &&
                        order.source_revision_id.empty() && order.source_revision == 0;
    const bool converted = !order.source_quotation_id.empty() &&
                           !order.source_revision_id.empty() && order.source_revision > 0;
    if (!direct && !converted) {
        throw RuleViolation(
            "A quotation source must identify one quotation and one exact revision.");
    }

    switch (order.state) {
        case OrderState::Open:
            // Cancellation evidence on an open order is contradictory. If it
            // were tolerated, one screen could show "open" while an audit
            // screen showed who cancelled it.
            if (order.cancelled_at != 0 || !blank(order.cancelled_by) ||
                !blank(order.cancel_reason)) {
                throw RuleViolation("An open order cannot carry cancellation details.");
            }
            break;
        case OrderState::Cancelled:
            // All three facts travel together. A reason without a person or a
            // time is not an audit trail, and filling either with a default
            // would invent evidence.
            if (order.cancelled_at <= 0 || blank(order.cancelled_by) ||
                blank(order.cancel_reason)) {
                throw RuleViolation(
                    "A cancelled order must record when, by whom, and why it was cancelled.");
            }
            break;
        default:
            throw RuleViolation("That order has a state this build does not understand.");
    }
}

void validate(const OrderLine& line) {
    if (line.id.empty()) {
        throw RuleViolation("This order line has no record to be saved under.");
    }
    if (line.order_id.empty()) {
        throw RuleViolation("An order line must belong to an order.");
    }
    // A line has to say what it is for. A catalog line is identified by its
    // product; an off-catalog line has no product record, so its description is
    // the only thing telling anybody what was sold.
    if (line.product_id.empty() && blank(line.description)) {
        throw RuleViolation("A line with no product must describe what is being supplied.");
    }
    if (line.position < 0) {
        throw RuleViolation("An order line cannot sit at a negative position.");
    }
    if (line.position == std::numeric_limits<std::int64_t>::max()) {
        throw RuleViolation("That order has no position left for another line.");
    }
    if (line.quantity_scaled <= 0) {
        // Zero is meaningless and negative is a refund by another name. This
        // shop issues no refunds, so the door is closed here rather than left
        // for the invoice to notice.
        throw RuleViolation("An order line must be for a quantity greater than zero.");
    }
    if (line.unit_price_minor < 0) {
        throw RuleViolation("An order line cannot have a negative price.");
    }
    if (line.unit_price_minor > pricing::kMaxAmountMinor) {
        throw RuleViolation("That price is too large to be real. Check the decimal point.");
    }
    if (line.added_at <= 0) {
        throw RuleViolation("That line's date is not a time this shop has existed.");
    }
    if (blank(line.added_by)) {
        throw RuleViolation("An order line must record who added it.");
    }

    switch (line.price_source) {
        case pricing::RateSource::None:
        case pricing::RateSource::PartyRate:
        case pricing::RateSource::CatchAllRate:
        case pricing::RateSource::Default:
            break;
        default:
            throw RuleViolation("That line has a price source this build does not understand.");
    }
    if (line.price_source == pricing::RateSource::None && !line.price_overridden) {
        throw RuleViolation("That line has a price but no record of where it came from.");
    }
    if (line.price_overridden && blank(line.price_reason)) {
        throw RuleViolation("A changed price must keep the reason it was changed.");
    }
    if (!line.price_overridden && !blank(line.price_reason)) {
        throw RuleViolation("A price-change reason cannot exist without a price change.");
    }

    // Quantity and price are each sane on their own, and their product still
    // has to fit. Refusing here beats printing a total that has wrapped.
    if (!line_amount(line).ok) {
        throw RuleViolation("That quantity and price multiply to more than this shop can record.");
    }
}

engine::Row to_row(const Order& order) {
    engine::Row row;
    row.set("id", engine::Value::text(order.id));
    row.set("party_id", engine::Value::text(order.party_id));
    row.set("source_quotation_id", engine::Value::text(order.source_quotation_id));
    row.set("source_revision_id", engine::Value::text(order.source_revision_id));
    row.set("source_revision", engine::Value::integer(order.source_revision));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(order.state)));
    row.set("promised_at", engine::Value::integer(order.promised_at));
    row.set("note", engine::Value::text(order.note));
    row.set("created_at", engine::Value::integer(order.created_at));
    row.set("created_by", engine::Value::text(order.created_by));
    row.set("cancelled_at", engine::Value::integer(order.cancelled_at));
    row.set("cancelled_by", engine::Value::text(order.cancelled_by));
    row.set("cancel_reason", engine::Value::text(order.cancel_reason));
    return row;
}

Order order_from_row(const engine::Row& row) {
    Order order;
    order.id = row.get("id").text_or({});
    order.party_id = row.get("party_id").text_or({});
    order.source_quotation_id = row.get("source_quotation_id").text_or({});
    order.source_revision_id = row.get("source_revision_id").text_or({});
    order.source_revision = row.get("source_revision").integer_or(0);
    // Fail closed. Only the exact value for Open is editable; an unknown value
    // from a newer build or damaged row is treated as cancelled so this older
    // build cannot rewrite evidence it does not understand.
    order.state = (row.get("state").integer_or(1) == 0) ? OrderState::Open
                                                        : OrderState::Cancelled;
    order.promised_at = row.get("promised_at").integer_or(0);
    order.note = row.get("note").text_or({});
    order.created_at = row.get("created_at").integer_or(0);
    order.created_by = row.get("created_by").text_or({});
    order.cancelled_at = row.get("cancelled_at").integer_or(0);
    order.cancelled_by = row.get("cancelled_by").text_or({});
    order.cancel_reason = row.get("cancel_reason").text_or({});
    return order;
}

engine::Row to_row(const OrderLine& line) {
    engine::Row row;
    row.set("id", engine::Value::text(line.id));
    row.set("order_id", engine::Value::text(line.order_id));
    row.set("position", engine::Value::integer(line.position));
    row.set("product_id", engine::Value::text(line.product_id));
    row.set("description", engine::Value::text(line.description));
    row.set("quantity_scaled", engine::Value::integer(line.quantity_scaled));
    row.set("unit_price_minor", engine::Value::integer(line.unit_price_minor));
    row.set("price_source", engine::Value::integer(static_cast<std::int64_t>(line.price_source)));
    row.set("price_overridden", engine::Value::boolean(line.price_overridden));
    row.set("price_reason", engine::Value::text(line.price_reason));
    row.set("added_at", engine::Value::integer(line.added_at));
    row.set("added_by", engine::Value::text(line.added_by));
    return row;
}

OrderLine line_from_row(const engine::Row& row) {
    OrderLine line;
    line.id = row.get("id").text_or({});
    line.order_id = row.get("order_id").text_or({});
    line.position = row.get("position").integer_or(0);
    line.product_id = row.get("product_id").text_or({});
    line.description = row.get("description").text_or({});
    line.quantity_scaled = row.get("quantity_scaled").integer_or(0);
    line.unit_price_minor = row.get("unit_price_minor").integer_or(0);
    line.price_source = source_from(row.get("price_source").integer_or(0));
    line.price_overridden = row.get("price_overridden").boolean_or(false);
    line.price_reason = row.get("price_reason").text_or({});
    line.added_at = row.get("added_at").integer_or(0);
    line.added_by = row.get("added_by").text_or({});
    return line;
}

}  // namespace squiflow::modules::orders
