#pragma once

#include "app/contracts/result.hpp"
#include "shell/screen_registry.hpp"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace squiflow::shell {

struct NavigationRow final {
    protocol::ModuleId owner{};
    std::string stable_id{};
    std::string title_key{};
    std::string icon_name{};
    std::string component_url{};
    std::string group_key{};
    std::uint16_t group_rank{0};
    std::uint16_t screen_rank{0};
    bool selected{false};

    friend bool operator==(const NavigationRow&, const NavigationRow&) = default;
};

enum class NavigationErrorCode : std::uint8_t {
    UnknownRoute,
    RouteNotVisible,
    StaleSnapshot,
    BridgeConstructionFailed,
    NoHistory,
};

struct NavigationError final {
    NavigationErrorCode code{NavigationErrorCode::UnknownRoute};
    std::string route_id{};
    std::string message_key{};

    friend bool operator==(const NavigationError&, const NavigationError&) = default;
};

class NavigationController final {
  public:
    static constexpr std::size_t kMaximumHistoryEntries = 32;

    explicit NavigationController(const ScreenRegistry& registry) noexcept;
    NavigationController(const NavigationController&) = delete;
    NavigationController& operator=(const NavigationController&) = delete;

    app::Result<void, NavigationError> apply_access(NavigationAccess access);
    app::Result<void, NavigationError> select(std::string_view stable_id);
    app::Result<void, NavigationError> go_back();
    app::Result<void, NavigationError> go_forward();
    void shutdown() noexcept;

    const std::vector<NavigationRow>& rows() const noexcept { return rows_; }
    std::string_view current_route() const noexcept { return current_route_; }
    const PresentationBridge* active_bridge() const noexcept { return active_bridge_.get(); }
    PresentationBridge* active_bridge() noexcept { return active_bridge_.get(); }
    std::uint64_t session_generation() const noexcept;
    std::uint64_t navigation_revision() const noexcept;
    bool has_access() const noexcept { return has_access_; }

  private:
    app::Result<void, NavigationError> activate(std::string_view stable_id,
                                                bool record_history);
    bool route_exists(std::string_view stable_id) const noexcept;
    bool route_visible(std::string_view stable_id) const noexcept;
    void update_selected_flags() noexcept;
    void prune_history();
    static NavigationError error(NavigationErrorCode code, std::string_view route_id,
                                 std::string message_key);

    const ScreenRegistry& registry_;
    NavigationAccess access_{};
    bool has_access_{false};
    std::vector<NavigationRow> rows_{};
    std::string current_route_{};
    std::unique_ptr<PresentationBridge> active_bridge_{};
    std::deque<std::string> back_history_{};
    std::deque<std::string> forward_history_{};
};

}  // namespace squiflow::shell
