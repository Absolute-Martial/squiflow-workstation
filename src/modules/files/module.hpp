#pragma once

#include <cstdint>
#include <functional>

#include "modules/module.hpp"

namespace squiflow::modules::files {
ModulePtr make_module(std::function<std::int64_t()> clock);
}
