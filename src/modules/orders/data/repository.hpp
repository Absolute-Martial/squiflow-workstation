#pragma once

// Reads are templates over the reader so that the same function serves a plain
// Store and an open Transaction. A handler mid-write must see its own
// uncommitted rows - adding a line has to see the lines added a moment ago to
// know where the next one sits - while a screen listing orders must not need a
// transaction to do it. Both call these.

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/orders/data/tables.hpp"
#include "modules/orders/domain/order.hpp"

namespace squiflow::modules::orders::data {

template <typename Reader>
std::optional<Order> find_order(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kOrder, id);
    if (!row) {
        return std::nullopt;
    }
    return order_from_row(*row);
}

template <typename Reader>
std::optional<OrderLine> find_line(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kOrderLine, id);
    if (!row) {
        return std::nullopt;
    }
    return line_from_row(*row);
}

// Every line on one order, in the order the counter entered them. Sorted by
// position and then by id, so two devices that added a line at the same
// position still print the same order rather than disagreeing quietly.
template <typename Reader>
std::vector<OrderLine> lines_for_order(const Reader& reader, const std::string& order_id) {
    engine::Query query{tables::kOrderLine};
    query.where_equals("order_id", engine::Value::text(order_id));
    query.order_by("position");
    query.order_by("id");
    std::vector<OrderLine> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(line_from_row(row));
    }
    return result;
}

// Where the next line goes. Derived from what is actually stored rather than
// from a counter held on the order, because a counter and the rows it counts
// are two things that can disagree, and only one of them is the truth.
template <typename Reader>
std::optional<std::int64_t> next_position(const Reader& reader,
                                          const std::string& order_id) {
    std::int64_t highest = -1;
    for (const OrderLine& line : lines_for_order(reader, order_id)) {
        if (line.position > highest) {
            highest = line.position;
        }
    }
    if (highest == std::numeric_limits<std::int64_t>::max()) {
        return std::nullopt;
    }
    return highest + 1;
}

// One customer's orders, newest last. A walk-in order carries no party and so
// appears in no customer's list, which is correct: there is nobody to list it
// against.
template <typename Reader>
std::vector<Order> orders_for_party(const Reader& reader, const std::string& party_id) {
    engine::Query query{tables::kOrder};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.order_by("created_at");
    query.order_by("id");
    std::vector<Order> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(order_from_row(row));
    }
    return result;
}

// The total of an order as it currently stands, computed from its lines every
// time rather than stored on the order. A stored total is a second copy of a
// fact, and the two copies drift the first time anything writes one without
// the other.
template <typename Reader>
engine::MoneyResult total_for_order(const Reader& reader, const std::string& order_id) {
    return order_total(lines_for_order(reader, order_id));
}

// Writes. A transaction is required, and the caller cannot open one, so a
// write outside the gate above is not expressible here.
void save_order(engine::Transaction& transaction, const Order& order);
void save_line(engine::Transaction& transaction, const OrderLine& line);

}  // namespace squiflow::modules::orders::data
