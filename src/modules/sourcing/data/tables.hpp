#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules::sourcing::tables {

inline constexpr const char* kSupplier = "supplier_profile";
inline constexpr const char* kMaterial = "sourcing_material";
inline constexpr const char* kPurchase = "supplier_purchase";

// Global sequence: engine 1, administration 10, parties 11, catalog 12,
// pricing 13, orders 14, receivables 15, jobs 16, quotations 17,
// agreements 18, sourcing 19.
inline constexpr int kFirstMigration = 19;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::sourcing::tables
