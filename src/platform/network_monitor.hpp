#pragma once

#include "platform/network_state.hpp"

#include <cstddef>
#include <functional>
#include <memory>

namespace squiflow::platform {

inline constexpr std::size_t kMaxNetworkObservers = 64;
using NetworkCallback = std::function<void(const NetworkSnapshot&)>;

class NetworkSubscription {
public:
    NetworkSubscription() noexcept = default;
    NetworkSubscription(const NetworkSubscription&) = delete;
    NetworkSubscription& operator=(const NetworkSubscription&) = delete;
    NetworkSubscription(NetworkSubscription&&) noexcept;
    NetworkSubscription& operator=(NetworkSubscription&&) noexcept;
    ~NetworkSubscription();
    void reset() noexcept;
    bool active() const noexcept;
private:
    struct State;
    friend class NetworkMonitor;
    NetworkSubscription(std::weak_ptr<State> state, std::uint64_t id) noexcept;
    std::weak_ptr<State> state_{};
    std::uint64_t id_{0};
};

class NetworkMonitor {
public:
    NetworkMonitor();
    NetworkMonitor(const NetworkMonitor&) = delete;
    NetworkMonitor& operator=(const NetworkMonitor&) = delete;
    NetworkSnapshot current() const noexcept;
    NetworkSubscription subscribe(NetworkCallback callback);
    bool publish(NetworkSnapshot state) noexcept;
    std::size_t observer_count() const noexcept;
private:
    std::shared_ptr<NetworkSubscription::State> state_;
};

} // namespace squiflow::platform
