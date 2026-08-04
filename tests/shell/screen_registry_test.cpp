#include "shell/screen_registry.hpp"
#include "support/check.hpp"

#include <squiflow/protocol/module_graph.hpp>

#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using squiflow::protocol::ModuleId;
using squiflow::protocol::RightId;
using squiflow::shell::BridgeFactory;
using squiflow::shell::NavigationAccess;
using squiflow::shell::PresentationBridge;
using squiflow::shell::ScreenContribution;

struct Bridge final : PresentationBridge {};

BridgeFactory factory(int& constructions) {
    return [&constructions] {
        ++constructions;
        return std::make_unique<Bridge>();
    };
}

ScreenContribution screen(ModuleId owner, std::string id, RightId right,
                          int& constructions, std::uint16_t group_rank = 10,
                          std::uint16_t screen_rank = 10) {
    return {owner, std::move(id), "navigation.title", "document",
            "qrc:/qt/qml/SquiFlow/screens/Screen.qml", "navigation.group",
            group_rank, screen_rank, right, factory(constructions)};
}

std::vector<ModuleId> every_module() {
    std::vector<ModuleId> result;
    for (std::size_t index = 0; index < squiflow::protocol::kModuleCount; ++index) {
        result.push_back(static_cast<ModuleId>(index));
    }
    return result;
}

NavigationAccess access_with(const std::vector<ModuleId>& disabled = {}) {
    const auto resolved = squiflow::protocol::resolve_activation(disabled);
    if (!resolved.ok) {
        throw std::logic_error("test activation did not resolve");
    }
    squiflow::engine::RightsSet rights;
    rights.grant_all();
    return squiflow::shell::make_navigation_access(
        resolved.activation, rights, every_module(), 4, 9);
}

template <class Exception, class Action>
void rejected(Action&& action, const std::string& message) {
    try {
        action();
        squiflow::testing::check(false, message);
    } catch (const Exception&) {
        squiflow::testing::check(true, message);
    }
}

}  // namespace

int main() {
    namespace t = squiflow::testing;
    using squiflow::shell::ScreenRegistry;

    t::section("typed registration and visibility");
    ScreenRegistry registry;
    int made = 0;
    registry.add(screen(ModuleId::orders, "orders.list", RightId::right_order_read,
                        made, 20, 30));

    auto access = access_with();
    t::check(registry.visible(access).size() == 1,
             "registered active screen with its right is visible");
    auto bridge = registry.create("orders.list", access);
    t::check(bridge != nullptr && made == 1, "authorized bridge is created lazily");

    access.rights.revoke(RightId::right_order_read);
    t::check(registry.visible(access).empty(), "missing right hides route");
    t::check(registry.create("orders.list", access) == nullptr && made == 1,
             "missing right cannot construct bridge");

    access = access_with();
    access.registered[static_cast<std::size_t>(ModuleId::orders)] = false;
    t::check(registry.visible(access).empty(), "unregistered owner hides route");
    t::check(registry.create("orders.list", access) == nullptr && made == 1,
             "unregistered owner cannot construct bridge");

    t::section("activation keeps stored grants while filtering use");
    ScreenRegistry optional_registry;
    optional_registry.add(screen(ModuleId::jobs, "jobs.list", RightId::right_job_read,
                                 made));
    auto inactive = access_with({ModuleId::jobs});
    t::check(inactive.rights.has(RightId::right_job_read),
             "inactive module right remains in immutable snapshot");
    t::check(optional_registry.visible(inactive).empty(),
             "inactive extra-module route is hidden");
    auto reactivated = access_with();
    t::check(optional_registry.visible(reactivated).size() == 1,
             "reactivation restores visibility from retained grant");

    t::section("deterministic ordering");
    ScreenRegistry ordered;
    ordered.add(screen(ModuleId::jobs, "z.last", RightId::right_job_read, made, 2, 2));
    ordered.add(screen(ModuleId::orders, "b.same-rank", RightId::right_order_read,
                       made, 1, 5));
    ordered.add(screen(ModuleId::parties, "a.same-rank", RightId::right_party_read,
                       made, 1, 5));
    ordered.add(screen(ModuleId::catalog, "first", RightId::right_product_read,
                       made, 1, 1));
    const auto visible = ordered.visible(access_with());
    t::check(visible.size() == 4, "all ordered routes visible");
    t::check(visible[0]->id == "first" && visible[1]->id == "a.same-rank" &&
                 visible[2]->id == "b.same-rank" && visible[3]->id == "z.last",
             "routes sort by group rank then screen rank then stable id");

    t::section("malformed contributions are refused before storage");
    rejected<std::logic_error>([&] {
        registry.add(screen(ModuleId::orders, "orders.list", RightId::right_order_read,
                            made));
    }, "duplicate stable id rejected");
    rejected<std::invalid_argument>([&] {
        registry.add(screen(static_cast<ModuleId>(255), "bad.owner",
                            RightId::right_order_read, made));
    }, "invalid owner rejected");
    rejected<std::invalid_argument>([&] {
        registry.add(screen(ModuleId::orders, "bad.right",
                            static_cast<RightId>(65535), made));
    }, "invalid right rejected");
    rejected<std::invalid_argument>([&] {
        registry.add(screen(ModuleId::orders, "wrong.right",
                            RightId::right_party_read, made));
    }, "cross-module right rejected");
    rejected<std::invalid_argument>([&] {
        auto value = screen(ModuleId::orders, "Bad Route", RightId::right_order_read,
                            made);
        registry.add(std::move(value));
    }, "unsafe stable id rejected");
    rejected<std::invalid_argument>([&] {
        auto value = screen(ModuleId::orders, "bad.component",
                            RightId::right_order_read, made);
        value.component_url = "file:///tmp/Screen.qml";
        registry.add(std::move(value));
    }, "non-resource component URL rejected");
    rejected<std::invalid_argument>([&] {
        auto value = screen(ModuleId::orders, "missing.factory",
                            RightId::right_order_read, made);
        value.create_bridge = {};
        registry.add(std::move(value));
    }, "missing bridge factory rejected");

    t::section("snapshot and capacity boundaries");
    const auto all_active = squiflow::protocol::resolve_activation({});
    squiflow::engine::RightsSet rights;
    rights.grant_all();
    rejected<std::invalid_argument>([&] {
        auto modules = every_module();
        modules.push_back(ModuleId::orders);
        (void)squiflow::shell::make_navigation_access(
            all_active.activation, rights, modules, 1, 1);
    }, "snapshot rejects repeated registered module");
    rejected<std::invalid_argument>([&] {
        (void)squiflow::shell::make_navigation_access(
            all_active.activation, rights, {static_cast<ModuleId>(255)}, 1, 1);
    }, "snapshot rejects invalid registered module");

    ScreenRegistry full;
    for (std::size_t index = 0; index < ScreenRegistry::kMaximumScreens; ++index) {
        full.add(screen(ModuleId::orders, "orders.route-" + std::to_string(index),
                        RightId::right_order_read, made));
    }
    t::check(full.size() == ScreenRegistry::kMaximumScreens,
             "128 contribution capacity accepted");
    rejected<std::length_error>([&] {
        full.add(screen(ModuleId::orders, "orders.overflow",
                        RightId::right_order_read, made));
    }, "129th contribution rejected");

    const std::string maximum_id(ScreenRegistry::kMaximumStableIdBytes, 'a');
    ScreenRegistry boundary;
    boundary.add(screen(ModuleId::orders, maximum_id, RightId::right_order_read, made));
    t::check(boundary.size() == 1, "maximum-length stable id accepted");
    rejected<std::invalid_argument>([&] {
        boundary.add(screen(ModuleId::orders, maximum_id + "a",
                            RightId::right_order_read, made));
    }, "oversized stable id rejected");

    t::check(registry.create("unknown.route", access_with()) == nullptr,
             "unknown route refused without construction");
    return t::report();
}
