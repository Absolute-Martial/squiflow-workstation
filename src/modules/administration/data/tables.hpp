#pragma once

// The tables this module owns. No other module reads or writes them; anything
// that needs a person's name asks administration for it.

#include <vector>

#include "engine/storage/migration_runner.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::administration::tables {

inline constexpr const char* kPerson = "person";
inline constexpr const char* kPersonRight = "person_right";
inline constexpr const char* kDevice = "device";
inline constexpr const char* kSetting = "shop_setting";
inline constexpr const char* kModuleState = "module_state";
inline constexpr const char* kAudit = "audit_entry";

// Migration numbers are global across the whole application and the engine
// owns everything below ten. Administration is the first module, so it takes
// ten; the next module takes eleven, whatever order the modules happen to be
// registered in.
inline constexpr int kFirstMigration = 10;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::administration::tables
