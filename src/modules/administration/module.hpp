#pragma once

// The administration module: people, permissions, machines, shop settings,
// which parts of the application this shop uses, and the log of who changed
// what.
//
// Core, and first in the dependency order: every other module's rules are
// checked against a person and the rights this module records.

#include <cstdint>
#include <functional>

#include "modules/module.hpp"

namespace squiflow::modules::administration {

// The clock is passed in rather than read from the system, so that a test can
// say what time it is and a log line can be checked exactly.
ModulePtr make_module(std::function<std::int64_t()> clock);

}  // namespace squiflow::modules::administration
