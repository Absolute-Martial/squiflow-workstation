#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::pricing::tables {

// Rates are keyed by their own id, because a product can have many rates at
// once: one per party, and more than one over time.
inline constexpr const char* kRate = "rate";

// The standard price is keyed by product_id, so the schema itself makes a
// second default for the same product impossible rather than leaving it to a
// rule somebody has to remember.
inline constexpr const char* kDefaultRate = "rate_default";

// Overrides are keyed by their own id. A line can be re-priced more than once,
// and each attempt is kept, because the history of a price change is the point
// of recording it.
inline constexpr const char* kRateOverride = "rate_override";

// Migration numbers are global, not per module: engine 1, administration 10,
// parties 11, catalog 12, pricing 13.
inline constexpr int kFirstMigration = 13;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::pricing::tables
