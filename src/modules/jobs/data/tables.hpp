#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules::jobs::tables {

inline constexpr const char* kJob = "job_ticket";

// Global sequence: engine 1, administration 10, parties 11, catalog 12,
// pricing 13, orders 14, receivables 15, jobs 16.
inline constexpr int kFirstMigration = 16;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::jobs::tables
