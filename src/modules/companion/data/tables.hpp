#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules::companion::tables {

inline constexpr const char* kTask = "companion_task";
inline constexpr const char* kEvent = "companion_task_event";

// Global sequence ends agreements 18, sourcing 19, companion 20.
inline constexpr int kFirstMigration = 20;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::companion::tables
