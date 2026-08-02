#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules::files::tables {
inline constexpr const char* kAsset="file_asset";
inline constexpr const char* kLocation="file_location";
inline constexpr const char* kLink="file_link";
inline constexpr const char* kVolume="file_volume";
inline constexpr int kFirstMigration=21;
std::vector<engine::Migration> migrations();
}
