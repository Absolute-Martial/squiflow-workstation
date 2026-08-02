#pragma once
#include <cstdint>
#include <functional>
#include "modules/module.hpp"
namespace squiflow::modules::pricing {
ModulePtr make_module(std::function<std::int64_t()> clock);
}  // namespace squiflow::modules::pricing
