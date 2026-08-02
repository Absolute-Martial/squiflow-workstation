#pragma once

#include <cstdint>

namespace squiflow::protocol {

// The wire contract between workstation and server.
//
// Major changes break compatibility and require both sides to be updated
// together. Minor changes are additive: a party that does not understand a new
// field ignores it.
inline constexpr std::uint16_t kWireMajor = 0;
inline constexpr std::uint16_t kWireMinor = 1;

inline constexpr const char* kWireVersionString = "0.1";

// A workstation refuses to sync with a server whose major version differs.
constexpr bool wire_compatible(std::uint16_t major) noexcept {
    return major == kWireMajor;
}

}  // namespace squiflow::protocol
