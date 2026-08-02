#include "engine/identity/rights_set.hpp"

namespace squiflow::engine {

std::vector<protocol::RightId> RightsSet::granted() const {
    std::vector<protocol::RightId> out;
    for (std::size_t i = 0; i < protocol::kRightCount; ++i) {
        if (bits_.test(i)) {
            out.push_back(static_cast<protocol::RightId>(i));
        }
    }
    return out;
}

std::vector<protocol::RightId> RightsSet::granted_in(
    protocol::ModuleId module) const {
    std::vector<protocol::RightId> out;
    for (std::size_t i = 0; i < protocol::kRightCount; ++i) {
        if (!bits_.test(i)) {
            continue;
        }
        const auto right = static_cast<protocol::RightId>(i);
        if (protocol::right_module(right) == module) {
            out.push_back(right);
        }
    }
    return out;
}

}  // namespace squiflow::engine
