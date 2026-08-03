#include "platform/network_monitor.hpp"

#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <utility>
#include <vector>

namespace squiflow::platform {

struct NetworkSubscription::State {
    mutable std::mutex mutex;
    NetworkSnapshot snapshot{};
    std::map<std::uint64_t, NetworkCallback> callbacks;
    std::uint64_t next_id{1};
};

NetworkSubscription::NetworkSubscription(std::weak_ptr<State> state, std::uint64_t id) noexcept
    : state_(std::move(state)), id_(id) {}
NetworkSubscription::NetworkSubscription(NetworkSubscription&& other) noexcept
    : state_(std::move(other.state_)), id_(std::exchange(other.id_, 0)) {}
NetworkSubscription& NetworkSubscription::operator=(NetworkSubscription&& other) noexcept {
    if (this != &other) { reset(); state_ = std::move(other.state_); id_ = std::exchange(other.id_, 0); }
    return *this;
}
NetworkSubscription::~NetworkSubscription() { reset(); }
void NetworkSubscription::reset() noexcept {
    if (id_ != 0) {
        if (auto state = state_.lock()) {
            std::lock_guard lock(state->mutex);
            state->callbacks.erase(id_);
        }
    }
    id_ = 0;
    state_.reset();
}
bool NetworkSubscription::active() const noexcept { return id_ != 0 && !state_.expired(); }

NetworkMonitor::NetworkMonitor() : state_(std::make_shared<NetworkSubscription::State>()) {}
NetworkSnapshot NetworkMonitor::current() const noexcept {
    std::lock_guard lock(state_->mutex);
    return state_->snapshot;
}
NetworkSubscription NetworkMonitor::subscribe(NetworkCallback callback) {
    if (!callback) throw std::invalid_argument("A network observer callback is required.");
    std::uint64_t id{}; NetworkSnapshot initial;
    {
        std::lock_guard lock(state_->mutex);
        if (state_->callbacks.size() >= kMaxNetworkObservers) throw std::length_error("The network observer limit was reached.");
        if (state_->next_id == 0) throw std::overflow_error("Network observer identifiers are exhausted.");
        id = state_->next_id++;
        state_->callbacks.emplace(id, callback);
        initial = state_->snapshot;
    }
    try { callback(initial); } catch (...) { }
    return NetworkSubscription(state_, id);
}
bool NetworkMonitor::publish(NetworkSnapshot next) noexcept {
    std::vector<NetworkCallback> callbacks;
    {
        std::lock_guard lock(state_->mutex);
        NetworkSnapshot comparable = next;
        comparable.generation = state_->snapshot.generation;
        if (comparable == state_->snapshot) return false;
        if (state_->snapshot.generation == std::numeric_limits<std::uint64_t>::max()) return false;
        next.generation = state_->snapshot.generation + 1;
        state_->snapshot = next;
        callbacks.reserve(state_->callbacks.size());
        for (const auto& [id, callback] : state_->callbacks) { (void)id; callbacks.push_back(callback); }
    }
    for (const auto& callback : callbacks) { try { callback(next); } catch (...) { } }
    return true;
}
std::size_t NetworkMonitor::observer_count() const noexcept {
    std::lock_guard lock(state_->mutex);
    return state_->callbacks.size();
}

} // namespace squiflow::platform
