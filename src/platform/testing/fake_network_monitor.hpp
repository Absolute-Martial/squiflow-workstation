#pragma once

#include "platform/network_monitor.hpp"

namespace squiflow::platform::testing {

class FakeNetworkMonitor {
public:
    NetworkMonitor& monitor() noexcept { return monitor_; }
    const NetworkMonitor& monitor() const noexcept { return monitor_; }
    bool publish(NetworkSnapshot state) noexcept { return monitor_.publish(state); }
    bool publish_offline() noexcept { NetworkSnapshot s; s.reachability_supported=true; s.reachability=NetworkReachability::Offline; return publish(s); }
    bool publish_online() noexcept { NetworkSnapshot s; s.reachability_supported=true; s.reachability=NetworkReachability::Online; return publish(s); }
    bool publish_metered_online() noexcept { NetworkSnapshot s; s.reachability_supported=true; s.reachability=NetworkReachability::Online; s.metered_supported=true; s.metered=true; return publish(s); }
    bool publish_captive_portal() noexcept { NetworkSnapshot s; s.reachability_supported=true; s.reachability=NetworkReachability::Online; s.captive_portal_supported=true; s.captive_portal=true; return publish(s); }
private:
    NetworkMonitor monitor_{};
};

} // namespace squiflow::platform::testing
