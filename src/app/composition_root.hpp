#pragma once
#include "modules/registry.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string_view>
namespace squiflow::app {
inline constexpr std::array<std::string_view,12> kCompositionModules={"administration","parties","catalog","pricing","quotations","orders","jobs","receivables","agreements","sourcing","companion","files"};
void register_all_modules(modules::Registry&,std::function<std::int64_t()> clock);
}