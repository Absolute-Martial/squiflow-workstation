#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace squiflow::shell {

enum class ConnectivityState : std::uint8_t { Online, Offline, Syncing, Degraded };
enum class ThemeChoice : std::uint8_t { System, Light, Dark };

class ShellState final {
  public:
    static constexpr std::size_t kMaximumIdentityBytes = 160;
    static constexpr std::size_t kMaximumRouteBytes = 64;

    bool set_identity(std::string tenant_name, std::string user_name);
    void set_connectivity(ConnectivityState state) noexcept { connectivity_ = state; }
    void set_theme(ThemeChoice theme) noexcept { theme_ = theme; }
    void set_high_contrast(bool enabled) noexcept { high_contrast_ = enabled; }
    void set_reduced_motion(bool enabled) noexcept { reduced_motion_ = enabled; }
    void mark_dirty(bool dirty) noexcept;

    bool request_route(std::string route_id);
    std::optional<std::string> resolve_unsaved(bool discard);

    const std::string& tenant_name() const noexcept { return tenant_name_; }
    const std::string& user_name() const noexcept { return user_name_; }
    ConnectivityState connectivity() const noexcept { return connectivity_; }
    ThemeChoice theme() const noexcept { return theme_; }
    bool high_contrast() const noexcept { return high_contrast_; }
    bool reduced_motion() const noexcept { return reduced_motion_; }
    bool dirty() const noexcept { return dirty_; }
    bool awaiting_unsaved_decision() const noexcept {
        return pending_route_.has_value();
    }

  private:
    std::string tenant_name_{"Local shop"};
    std::string user_name_{"Signed-in user"};
    ConnectivityState connectivity_{ConnectivityState::Online};
    ThemeChoice theme_{ThemeChoice::System};
    bool high_contrast_{false};
    bool reduced_motion_{false};
    bool dirty_{false};
    std::optional<std::string> pending_route_{};
};

}  // namespace squiflow::shell
