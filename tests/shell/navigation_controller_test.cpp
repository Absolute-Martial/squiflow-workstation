#include "shell/navigation_controller.hpp"
#include "support/check.hpp"

#include <squiflow/protocol/module_graph.hpp>

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using squiflow::protocol::ModuleId;
using squiflow::protocol::RightId;
using squiflow::shell::NavigationAccess;
using squiflow::shell::PresentationBridge;
using squiflow::shell::ScreenContribution;

struct CountingBridge final : PresentationBridge {
    CountingBridge(int& live, int& destroyed) : live_(live), destroyed_(destroyed) {
        ++live_;
    }
    ~CountingBridge() override {
        --live_;
        ++destroyed_;
    }
    int& live_;
    int& destroyed_;
};

ScreenContribution route(ModuleId owner, std::string id, RightId right, int rank,
                         int& made, int& live, int& destroyed) {
    return {owner, std::move(id), "title.key", "icon",
            "qrc:/qt/qml/SquiFlow/screens/ModuleListScreen.qml", "group.work", 10,
            static_cast<std::uint16_t>(rank), right,
            [&made, &live, &destroyed] {
                ++made;
                return std::make_unique<CountingBridge>(live, destroyed);
            }};
}

NavigationAccess access(std::uint64_t generation, std::uint64_t revision,
                        std::vector<RightId> rights,
                        const std::vector<ModuleId>& disabled = {}) {
    const auto activation = squiflow::protocol::resolve_activation(disabled);
    if (!activation.ok) {
        throw std::logic_error("test activation failed");
    }
    squiflow::engine::RightsSet granted;
    for (const RightId right : rights) {
        granted.grant(right);
    }
    return squiflow::shell::make_navigation_access(
        activation.activation, granted,
        {ModuleId::orders, ModuleId::jobs, ModuleId::quotations},
        generation, revision);
}

}  // namespace

int main() {
    namespace t = squiflow::testing;
    using squiflow::shell::NavigationController;
    using squiflow::shell::NavigationErrorCode;
    using squiflow::shell::ScreenRegistry;

    int made = 0;
    int live = 0;
    int destroyed = 0;
    ScreenRegistry registry;
    registry.add(route(ModuleId::orders, "orders.list", RightId::right_order_read,
                       10, made, live, destroyed));
    registry.add(route(ModuleId::jobs, "jobs.list", RightId::right_job_read,
                       20, made, live, destroyed));
    registry.add(route(ModuleId::quotations, "quotations.list",
                       RightId::right_quotation_read, 30, made, live, destroyed));

    t::section("initial access and selection");
    NavigationController controller(registry);
    auto initial = access(1, 1, {RightId::right_order_read, RightId::right_job_read,
                                  RightId::right_quotation_read});
    t::check(controller.apply_access(initial).has_value(), "initial snapshot accepted");
    t::check(controller.rows().size() == 3, "three authorized routes visible");
    t::check(controller.current_route() == "orders.list", "first ranked route selected");
    t::check(controller.active_bridge() != nullptr && made == 1 && live == 1,
             "exactly one initial bridge alive");

    t::check(controller.select("jobs.list").has_value(), "visible route selected");
    t::check(controller.current_route() == "jobs.list" && made == 2 && live == 1 &&
                 destroyed == 1,
             "select replaces rather than caches active bridge");
    t::check(controller.go_back().has_value() &&
                 controller.current_route() == "orders.list",
             "bounded stable-id history navigates back");
    t::check(controller.go_forward().has_value() &&
                 controller.current_route() == "jobs.list",
             "bounded stable-id history navigates forward");

    t::section("unknown, unauthorized and stale input");
    const int made_before_refusal = made;
    auto unknown = controller.select("missing.route");
    t::check(!unknown && unknown.error().code == NavigationErrorCode::UnknownRoute,
             "unknown route returns explicit failure");
    t::check(made == made_before_refusal, "unknown route does not invoke factory");

    auto revoked = access(1, 2, {RightId::right_order_read,
                                  RightId::right_quotation_read});
    t::check(controller.apply_access(revoked).has_value(), "rights revocation applied");
    t::check(controller.current_route() == "orders.list" && live == 1,
             "revoked current bridge destroyed before deterministic fallback");
    const auto hidden = controller.select("jobs.list");
    t::check(!hidden && hidden.error().code == NavigationErrorCode::RouteNotVisible,
             "hidden route selection refused explicitly");
    t::check(made == made_before_refusal + 1,
             "hidden route does not invoke its bridge factory");

    const auto stale = controller.apply_access(initial);
    t::check(!stale && stale.error().code == NavigationErrorCode::StaleSnapshot,
             "older navigation revision rejected");
    t::check(controller.navigation_revision() == 2,
             "stale update cannot replace current revision");
    auto conflicting = revoked;
    conflicting.rights.grant(RightId::right_job_read);
    const auto same_revision_conflict = controller.apply_access(conflicting);
    t::check(!same_revision_conflict &&
                 same_revision_conflict.error().code == NavigationErrorCode::StaleSnapshot,
             "same revision with different content rejected");
    const int made_before_repeat = made;
    t::check(controller.apply_access(revoked).has_value() && made == made_before_repeat,
             "identical repeated snapshot is idempotent");

    t::section("activation and session replacement");
    auto jobs_inactive = access(1, 3,
                                {RightId::right_order_read, RightId::right_job_read},
                                {ModuleId::jobs});
    t::check(controller.apply_access(jobs_inactive).has_value(),
             "module deactivation snapshot accepted");
    t::check(controller.rows().size() == 1 &&
                 jobs_inactive.rights.has(RightId::right_job_read),
             "inactive route hidden while grant remains stored");

    const int destroyed_before_session = destroyed;
    auto replacement = access(2, 1, {RightId::right_job_read});
    t::check(controller.apply_access(replacement).has_value(),
             "new session generation accepted despite lower revision");
    t::check(controller.current_route() == "jobs.list" &&
                 destroyed > destroyed_before_session,
             "session replacement destroys old bridge and selects from new rights");
    const auto old_session = controller.apply_access(jobs_inactive);
    t::check(!old_session && old_session.error().code == NavigationErrorCode::StaleSnapshot,
             "older session generation rejected");

    t::section("empty and failed bridge states");
    auto no_rights = access(2, 2, {});
    t::check(controller.apply_access(no_rights).has_value(), "empty access accepted");
    t::check(controller.rows().empty() && controller.current_route().empty() &&
                 controller.active_bridge() == nullptr && live == 0,
             "no visible routes owns no unauthorized bridge");

    ScreenRegistry failing_registry;
    failing_registry.add({ModuleId::orders, "orders.fail", "title.key", "icon",
                          "qrc:/qt/qml/SquiFlow/screens/ModuleListScreen.qml",
                          "group.work", 1, 1, RightId::right_order_read,
                          []() -> std::unique_ptr<PresentationBridge> { return {}; }});
    NavigationController failing(failing_registry);
    const auto failure = failing.apply_access(
        squiflow::shell::make_navigation_access(
            squiflow::protocol::resolve_activation({}).activation,
            [&] { squiflow::engine::RightsSet value;
                  value.grant(RightId::right_order_read); return value; }(),
            {ModuleId::orders}, 1, 1));
    t::check(!failure && failure.error().code ==
                            NavigationErrorCode::BridgeConstructionFailed,
             "null bridge factory result is an explicit presentation failure");
    t::check(failing.current_route().empty() && failing.active_bridge() == nullptr,
             "failed initial bridge leaves no selected route");

    controller.shutdown();
    t::check(!controller.has_access() && controller.rows().empty() && live == 0,
             "shutdown clears snapshot, rows, history and bridge");
    return t::report();
}
