#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::orders::tables {

// Not "order". ORDER is a reserved word in SQL, and the SQLite store is a real
// backend here, not a hypothetical one. A table name that has to be quoted
// forever is a trap laid for whoever writes the next query by hand.
inline constexpr const char* kOrder = "customer_order";

// Lines are keyed by their own id rather than by order and position. Position
// can change, and a key that changes is not a key.
inline constexpr const char* kOrderLine = "order_line";

// Migration numbers are global, not per module: engine 1, administration 10,
// parties 11, catalog 12, pricing 13, orders 14.
inline constexpr int kFirstMigration = 14;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::orders::tables
