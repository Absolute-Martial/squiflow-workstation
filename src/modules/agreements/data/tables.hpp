#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules::agreements::tables {

inline constexpr const char* kAgreement = "agreement";
inline constexpr const char* kLine = "agreement_line";

// Global sequence: engine 1, administration 10, parties 11, catalog 12,
// pricing 13, orders 14, receivables 15, jobs 16, quotations 17,
// agreements 18.
inline constexpr int kFirstMigration = 18;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::agreements::tables
