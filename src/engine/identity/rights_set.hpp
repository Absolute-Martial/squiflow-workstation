#pragma once

#include <bitset>
#include <vector>

#include <squiflow/protocol/right_id.hpp>

namespace squiflow::engine {

// Rights granted to one person. There are no roles: the shopkeeper grants each
// right to each person directly, which is what was asked for.
//
// A bitset because the whole set fits in a handful of bytes and the check has
// to be cheap enough that nobody is ever tempted to cache the answer.
class RightsSet {
  public:
    RightsSet() = default;

    void grant(protocol::RightId right) noexcept {
        bits_.set(static_cast<std::size_t>(right));
    }

    void revoke(protocol::RightId right) noexcept {
        bits_.reset(static_cast<std::size_t>(right));
    }

    bool has(protocol::RightId right) const noexcept {
        return bits_.test(static_cast<std::size_t>(right));
    }

    std::size_t count() const noexcept { return bits_.count(); }
    bool empty() const noexcept { return bits_.none(); }

    void grant_all() noexcept { bits_.set(); }
    void clear() noexcept { bits_.reset(); }

    std::vector<protocol::RightId> granted() const;

    // Grants for a switched-off module are kept, not deleted. Switching the
    // module back on restores exactly what the person had before, which is why
    // filtering happens at the point of use rather than at the point of grant.
    std::vector<protocol::RightId> granted_in(protocol::ModuleId module) const;

    friend bool operator==(const RightsSet&, const RightsSet&) = default;

  private:
    std::bitset<protocol::kRightCount> bits_;
};

}  // namespace squiflow::engine
