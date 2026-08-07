#pragma once

#include "app/dashboard/dashboard_query.hpp"
#include "app/contracts/domain_error.hpp"
#include "app/contracts/result.hpp"
#include "shell/view_model_state.hpp"

#include <cstdint>
#include <optional>

namespace squiflow::shell {

struct DashboardRefresh final {
    std::uint64_t generation{0};
    std::uint64_t session_generation{0};
};

class DashboardBridge final {
  public:
    app::Result<DashboardRefresh, app::DomainError> begin_refresh(
        std::uint64_t session_generation);
    app::Result<void, app::DomainError> apply(
        std::uint64_t generation, std::uint64_t session_generation,
        app::dashboard::DashboardSnapshot snapshot);
    app::Result<void, app::DomainError> fail(
        std::uint64_t generation, std::uint64_t session_generation,
        app::DomainError error);
    void cancel() noexcept;

    const ViewModelState& state() const noexcept { return state_; }
    const std::optional<app::dashboard::DashboardSnapshot>& snapshot() const noexcept {
        return snapshot_;
    }
    std::uint64_t generation() const noexcept { return generation_; }

  private:
    app::DomainError stale() const;

    ViewModelState state_{IdleState{}};
    std::optional<app::dashboard::DashboardSnapshot> snapshot_{};
    std::uint64_t generation_{0};
    std::uint64_t session_generation_{0};
    bool request_in_flight_{false};
};

}  // namespace squiflow::shell
