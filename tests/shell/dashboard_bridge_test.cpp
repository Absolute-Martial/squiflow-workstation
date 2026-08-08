#include "shell/dashboard_bridge.hpp"
#include "support/check.hpp"

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;
    shell::DashboardBridge bridge;

    t::section("generation and immutable snapshot");
    auto first = bridge.begin_refresh(1);
    auto second = bridge.begin_refresh(1);
    t::check(first && second && second.value().generation > first.value().generation,
             "new refresh supersedes previous generation");
    app::dashboard::DashboardSnapshot snapshot;
    snapshot.metrics.push_back({protocol::ModuleId::jobs,
                                protocol::RightId::right_job_read,
                                "jobs.open", "dashboard.open_jobs", "3", "",
                                "jobs.list", std::nullopt, ""});
    auto stale = bridge.apply(first.value().generation, 1, snapshot);
    t::check(!stale && stale.error().code == app::DomainErrorCode::Cancelled,
             "slow prior refresh cannot replace current dashboard");
    t::check(bridge.apply(second.value().generation, 1, snapshot).has_value(),
             "current generation applies");
    t::check(shell::state_kind(bridge.state()) == shell::ViewStateKind::Ready &&
                 bridge.snapshot()->metrics.front().id == "jobs.open",
             "bridge owns ready presentation snapshot");

    t::section("account switch and offline cache");
    auto old_account = bridge.begin_refresh(1);
    auto new_account = bridge.begin_refresh(2);
    t::check(!bridge.apply(old_account.value().generation, 1, snapshot),
             "previous account completion is discarded");
    snapshot.offline = true;
    snapshot.stale = true;
    t::check(bridge.apply(new_account.value().generation, 2, snapshot).has_value() &&
                 shell::state_kind(bridge.state()) == shell::ViewStateKind::Offline,
             "offline cached dashboard is explicit");

    t::section("failure and cancellation");
    auto failing = bridge.begin_refresh(2);
    t::check(bridge.fail(failing.value().generation, 2,
                         {app::DomainErrorCode::Timeout,
                          "dashboard.error.timeout", std::nullopt}).has_value(),
             "current failure applies");
    t::check(shell::state_kind(bridge.state()) == shell::ViewStateKind::Failed,
             "failure is visible without destroying navigation");
    auto pending = bridge.begin_refresh(2);
    bridge.cancel();
    t::check(!bridge.apply(pending.value().generation, 2, snapshot) &&
                 shell::state_kind(bridge.state()) == shell::ViewStateKind::Idle,
             "completion after route close is discarded");
    t::check(!bridge.begin_refresh(0), "zero session generation rejected");

    return t::report();
}
