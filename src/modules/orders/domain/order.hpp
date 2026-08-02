#pragma once

// An order is what was agreed to be done. Several jobs may hang off one order,
// and a job may exist with no order at all, so nothing here reaches towards
// jobs.
//
// Two rules shape this file.
//
// The first is the price snapshot. When a line is added, the price resolved at
// that moment is copied onto the line and never looked up again. Editing a
// rate next week must not silently re-price work that was agreed last week,
// and the only way to guarantee that is to stop asking. The line also keeps
// why it cost what it cost, because "500, the agreed rate" and "500, the
// standard price" are different answers when a customer queries a bill.
//
// The second is that nothing here is ever negative. This shop issues no
// refunds and no credit notes; a negative quantity or a negative price would
// be a refund wearing a disguise, arriving through a door nobody guarded.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/records/money.hpp"
#include "engine/records/quantity.hpp"
#include "engine/storage/store.hpp"
#include "modules/pricing/domain/rate.hpp"

namespace squiflow::modules::orders {

// Orders deliberately do not use engine::DocumentState. That lifecycle names
// the three things that share it - quotations, invoices and agreements - and
// its Draft/Issued split exists because those are documents a customer ends up
// holding. An order is not issued to anybody; it is agreed, and from that
// moment it is live. There is no order_issue operation on the wire, so
// borrowing that enum would leave Issued unreachable and Draft meaningless.
enum class OrderState : std::uint8_t {
    Open,       // agreed and live; may be edited and may take new lines
    Cancelled,  // called off, with a recorded reason; never deleted
};

const char* to_string(OrderState state) noexcept;

// An order may only be changed while it is open. A cancelled order is a
// record of something that was called off, and editing it would rewrite what
// the shop and the customer agreed.
constexpr bool can_change(OrderState state) noexcept {
    return state == OrderState::Open;
}

struct Order {
    std::string id{};
    std::string party_id{};
    OrderState state{OrderState::Open};

    // What the customer was told, as an instant. Zero means nothing was
    // promised, which is honest; a default of "today" would be a promise the
    // shop never made.
    std::int64_t promised_at{0};

    std::string note{};
    std::int64_t created_at{0};
    std::string created_by{};

    std::int64_t cancelled_at{0};
    std::string cancelled_by{};
    std::string cancel_reason{};
};

// One agreed thing, at the price it was agreed at.
struct OrderLine {
    std::string id{};
    std::string order_id{};

    // Where the line sits on the order. Held explicitly so the printed order
    // reads in the order the counter entered it, rather than in whatever order
    // the rows came back.
    std::int64_t position{0};

    // Empty for an off-catalog line: a one-off job the shop does not keep a
    // product record for. Such a line must carry its own description, and its
    // price can only have come from an override.
    std::string product_id{};
    std::string description{};

    // Thousandths, matching engine::Quantity.
    std::int64_t quantity_scaled{0};

    // The snapshot. Minor units, per one unit of quantity.
    std::int64_t unit_price_minor{0};

    // Why it cost that, frozen at the same moment as the price itself.
    pricing::RateSource price_source{pricing::RateSource::None};
    bool price_overridden{false};
    std::string price_reason{};

    std::int64_t added_at{0};
    std::string added_by{};
};

// quantity x unit price, checked. Returns ok=false on overflow rather than
// wrapping, because a wrapped total is a wrong bill that looks like a right
// one.
engine::MoneyResult line_amount(const OrderLine& line) noexcept;

// Every line added up, checked at each step. An order with no lines totals
// zero, which is correct: nothing has been agreed yet.
engine::MoneyResult order_total(const std::vector<OrderLine>& lines) noexcept;

// Validation: throws RuleViolation on the first problem found.
void validate(const Order& order);
void validate(const OrderLine& line);

// Row mapping.
engine::Row to_row(const Order& order);
Order order_from_row(const engine::Row& row);

engine::Row to_row(const OrderLine& line);
OrderLine line_from_row(const engine::Row& row);

}  // namespace squiflow::modules::orders
