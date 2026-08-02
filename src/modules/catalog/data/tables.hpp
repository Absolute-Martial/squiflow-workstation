#pragma once
#include <vector>
#include "engine/storage/migration_runner.hpp"
#include "engine/storage/store.hpp"
namespace squiflow::modules::catalog::tables {
inline constexpr const char* kProduct = "product";
inline constexpr int kFirstMigration = 12;
std::vector<engine::Migration> migrations();
}  // namespace squiflow::modules::catalog::tables
