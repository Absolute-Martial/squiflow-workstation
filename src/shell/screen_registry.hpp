#pragma once

#include "engine/identity/rights_set.hpp"

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/right_id.hpp>

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace squiflow::shell {

class PresentationBridge {
  public:
    virtual ~PresentationBridge() = default;
};

using BridgeFactory = std::function<std::unique_ptr<PresentationBridge>()>;

struct ScreenContribution final {
    protocol::ModuleId owner{};
    std::string id{};
    std::string title_key{};
    std::string icon_name{};
    std::string component_url{};
    std::string group_key{};
    std::uint16_t group_rank{0};
    std::uint16_t screen_rank{0};
    std::optional<protocol::RightId> required_right{};
    BridgeFactory create_bridge{};
};

// Complete, immutable-by-convention value copied from application state.  It
// deliberately owns its activation, registration and rights data: a queued
// GUI refresh must never retain pointers to Session or modules::Registry.
struct NavigationAccess final {
    protocol::Activation activation{};
    engine::RightsSet rights{};
    std::array<bool, protocol::kModuleCount> registered{};
    std::uint64_t session_generation{0};
    std::uint64_t navigation_revision{0};

    bool allows(const ScreenContribution& screen) const noexcept;
    bool module_registered(protocol::ModuleId module) const noexcept;

    friend bool operator==(const NavigationAccess& left,
                           const NavigationAccess& right) noexcept {
        return left.activation.active == right.activation.active &&
               left.rights == right.rights &&
               left.registered == right.registered &&
               left.session_generation == right.session_generation &&
               left.navigation_revision == right.navigation_revision;
    }
};

NavigationAccess make_navigation_access(
    const protocol::Activation& activation,
    const engine::RightsSet& rights,
    const std::vector<protocol::ModuleId>& registered,
    std::uint64_t session_generation,
    std::uint64_t navigation_revision);

class ScreenRegistry final {
  public:
    static constexpr std::size_t kMaximumScreens = 128;
    static constexpr std::size_t kMaximumStableIdBytes = 64;
    static constexpr std::size_t kMaximumTextKeyBytes = 128;
    static constexpr std::size_t kMaximumComponentBytes = 256;

    void add(ScreenContribution screen);
    std::size_t size() const noexcept { return screens_.size(); }
    const std::vector<ScreenContribution>& all() const noexcept { return screens_; }
    std::vector<const ScreenContribution*> visible(const NavigationAccess& access) const;
    std::unique_ptr<PresentationBridge> create(
        std::string_view id, const NavigationAccess& access) const;

  private:
    const ScreenContribution* find(std::string_view id) const noexcept;
    std::vector<ScreenContribution> screens_{};
};

}  // namespace squiflow::shell
