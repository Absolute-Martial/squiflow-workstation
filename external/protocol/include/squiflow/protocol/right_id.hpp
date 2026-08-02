#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

#include <squiflow/protocol/module_id.hpp>

namespace squiflow::protocol {

enum class RightId : std::uint16_t {
#define SQF_RIGHT(name, module) name,
#include <squiflow/protocol/rights.def>
#undef SQF_RIGHT
    Count
};

inline constexpr std::size_t kRightCount = static_cast<std::size_t>(RightId::Count);

// True only for a right this build actually has. A grant read back from the
// database, or arriving from the server, is a number until it passes here.
constexpr bool is_valid(RightId right) noexcept {
    return static_cast<std::size_t>(right) < kRightCount;
}

// False when the number names no right in this build, leaving out untouched.
// A right this build does not know about is not an error worth refusing a
// whole record over; it is simply a permission that cannot be honoured here.
constexpr bool right_from_number(std::uint32_t number, RightId& out) noexcept {
    if (number >= kRightCount) {
        return false;
    }
    out = static_cast<RightId>(number);
    return true;
}

// Both abort rather than read out of bounds when handed an invalid right.
// Validate with is_valid or right_from_number at the point the value enters
// the program; by the time it reaches these, a bad value means the program
// state is already wrong and continuing would corrupt records.
std::string_view right_name(RightId right) noexcept;
ModuleId right_module(RightId right) noexcept;

// A right belonging to a switched-off module simply does not appear. The grant
// itself is kept, so switching the module back on restores it unchanged.

}  // namespace squiflow::protocol
