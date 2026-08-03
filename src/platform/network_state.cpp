#include "platform/network_state.hpp"

namespace squiflow::platform {

bool has_usable_network(const NetworkSnapshot& state) noexcept {
    return state.reachability_supported &&
           state.reachability == NetworkReachability::Online &&
           !(state.captive_portal_supported && state.captive_portal);
}

bool should_attempt_server_work(const NetworkSnapshot& state) noexcept {
    return has_usable_network(state);
}

bool should_defer_optional_transfer(const NetworkSnapshot& state) noexcept {
    return !has_usable_network(state) || (state.metered_supported && state.metered);
}

} // namespace squiflow::platform
