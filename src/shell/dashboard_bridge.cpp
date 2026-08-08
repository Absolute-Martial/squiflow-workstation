#include "shell/dashboard_bridge.hpp"

#include <utility>

namespace squiflow::shell {

app::DomainError DashboardBridge::stale() const {
    return {app::DomainErrorCode::Cancelled, "dashboard.error.stale_refresh",
            std::nullopt};
}

app::Result<DashboardRefresh, app::DomainError> DashboardBridge::begin_refresh(
    std::uint64_t session_generation) {
    if (session_generation == 0) {
        return app::Result<DashboardRefresh, app::DomainError>::failure(
            {app::DomainErrorCode::InvalidContext,
             "dashboard.error.session_required", "session_generation"});
    }
    ++generation_;
    session_generation_ = session_generation;
    request_in_flight_ = true;
    state_ = LoadingState{generation_};
    return app::Result<DashboardRefresh, app::DomainError>::success(
        {generation_, session_generation_});
}

app::Result<void, app::DomainError> DashboardBridge::apply(
    std::uint64_t generation, std::uint64_t session_generation,
    app::dashboard::DashboardSnapshot snapshot) {
    if (!request_in_flight_ || generation != generation_ ||
        session_generation != session_generation_) {
        return app::Result<void, app::DomainError>::failure(stale());
    }
    request_in_flight_ = false;
    const bool offline = snapshot.offline;
    const bool stale_data = snapshot.stale;
    snapshot_ = std::move(snapshot);
    state_ = offline ? ViewModelState{OfflineState{true}}
                     : ViewModelState{ReadyState{stale_data, false}};
    return app::Result<void, app::DomainError>::success();
}

app::Result<void, app::DomainError> DashboardBridge::fail(
    std::uint64_t generation, std::uint64_t session_generation,
    app::DomainError error) {
    if (!request_in_flight_ || generation != generation_ ||
        session_generation != session_generation_) {
        return app::Result<void, app::DomainError>::failure(stale());
    }
    request_in_flight_ = false;
    if (error.code == app::DomainErrorCode::Offline && snapshot_) {
        state_ = OfflineState{true};
    } else {
        state_ = FailedState{std::move(error.message_key)};
    }
    return app::Result<void, app::DomainError>::success();
}

void DashboardBridge::cancel() noexcept {
    ++generation_;
    session_generation_ = 0;
    request_in_flight_ = false;
    snapshot_.reset();
    state_ = IdleState{};
}

}  // namespace squiflow::shell
