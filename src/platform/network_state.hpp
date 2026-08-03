#pragma once

#include <cstdint>

namespace squiflow::platform {

enum class NetworkReachability { Unknown, Offline, LocalOnly, SiteOnly, Online };
enum class NetworkTransport { Unknown, Ethernet, Cellular, Wifi, Bluetooth };

struct NetworkSnapshot {
    NetworkReachability reachability{NetworkReachability::Unknown};
    NetworkTransport transport{NetworkTransport::Unknown};
    bool captive_portal{false};
    bool metered{false};
    bool reachability_supported{false};
    bool transport_supported{false};
    bool captive_portal_supported{false};
    bool metered_supported{false};
    std::uint64_t generation{0};
    bool operator==(const NetworkSnapshot&) const = default;
};

bool has_usable_network(const NetworkSnapshot& state) noexcept;
bool should_attempt_server_work(const NetworkSnapshot& state) noexcept;
bool should_defer_optional_transfer(const NetworkSnapshot& state) noexcept;

} // namespace squiflow::platform
