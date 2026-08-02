#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules::quotations::tables {

inline constexpr const char* kQuotation = "quotation";
inline constexpr const char* kRevision = "quotation_revision";
inline constexpr const char* kLine = "quotation_line";

// Global sequence: engine 1, administration 10, parties 11, catalog 12,
// pricing 13, orders 14, receivables 15, jobs 16, quotations 17.
inline constexpr int kFirstMigration = 17;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::quotations::tables
