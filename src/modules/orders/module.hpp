#pragma once

#include <cstdint>
#include <functional>

#include "modules/module.hpp"

namespace squiflow::modules::orders {

ModulePtr make_module(std::function<std::int64_t()> clock);

}  // namespace squiflow::modules::orders
