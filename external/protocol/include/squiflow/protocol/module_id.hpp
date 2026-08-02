#pragma once

#include <cstddef>
#include <cstdint>

namespace squiflow::protocol {

enum class ModuleTier : std::uint8_t { Core, Extra };

enum class ModuleId : std::uint8_t {
#define SQF_MODULE(name, tier) name,
#include <squiflow/protocol/modules.def>
#undef SQF_MODULE
    Count
};

inline constexpr std::size_t kModuleCount = static_cast<std::size_t>(ModuleId::Count);

// True only for a module this build actually has. ModuleId is a fixed-width
// enumeration, so any integer that fits can be cast into one; anything that
// arrives from outside this program must pass through here before it is used
// to index anything.
constexpr bool is_valid(ModuleId module) noexcept {
    return static_cast<std::size_t>(module) < kModuleCount;
}

// Null when the number names no module in this build. This is the only safe
// way to turn a stored or transmitted number back into a ModuleId.
constexpr bool module_from_number(std::uint32_t number, ModuleId& out) noexcept {
    if (number >= kModuleCount) {
        return false;
    }
    out = static_cast<ModuleId>(number);
    return true;
}

}  // namespace squiflow::protocol
