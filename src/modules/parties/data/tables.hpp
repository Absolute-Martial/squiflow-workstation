#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::parties::tables {

inline constexpr const char* kParty = "party";
inline constexpr const char* kContact = "party_contact";

// Administration is 10; parties is 11.
inline constexpr int kFirstMigration = 11;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::parties::tables
