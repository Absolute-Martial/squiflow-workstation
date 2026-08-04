#include "shell/navigation_manifest.hpp"
#include "support/check.hpp"

#include <squiflow/protocol/module_graph.hpp>

#include <stdexcept>
#include <vector>

namespace {

std::vector<squiflow::protocol::ModuleId> all_modules() {
    std::vector<squiflow::protocol::ModuleId> result;
    for (std::size_t index = 0; index < squiflow::protocol::kModuleCount; ++index) {
        result.push_back(static_cast<squiflow::protocol::ModuleId>(index));
    }
    return result;
}

template <class Exception, class Action>
void rejects(Action&& action, const std::string& message) {
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
    using namespace squiflow;

    auto manifest = shell::make_navigation_manifest();
    t::check(manifest.size() == protocol::kModuleCount,
             "manifest has one primary route for every module");
    shell::require_navigation_complete(manifest, all_modules());
    t::check(true, "complete manifest accepted");

    std::vector<bool> represented(protocol::kModuleCount, false);
    for (const shell::ScreenContribution& route : manifest.all()) {
        const auto owner = static_cast<std::size_t>(route.owner);
        t::check(!represented[owner], "module represented exactly once");
        represented[owner] = true;
        t::check(route.required_right.has_value(), "primary route has typed read right");
        t::check(protocol::right_module(*route.required_right) == route.owner,
                 "primary route right belongs to its module");
        t::check(route.component_url ==
                     "qrc:/qt/qml/SquiFlow/screens/ModuleListScreen.qml",
                 "primary route uses compiled generic list screen");
        auto bridge = route.create_bridge();
        const auto* typed = dynamic_cast<const shell::RoutePresentationBridge*>(bridge.get());
        t::check(typed != nullptr && typed->route_id() == route.id,
                 "factory creates bridge bound to exact stable route id");
        t::check(typed != nullptr && !typed->list().columns().empty(),
                 "each primary route owns a validated module list bridge");
    }

    auto incomplete = all_modules();
    incomplete.pop_back();
    rejects<std::logic_error>([&] {
        shell::require_navigation_complete(manifest, incomplete);
    }, "route for unregistered module rejected");

    shell::ScreenRegistry empty;
    rejects<std::logic_error>([&] {
        shell::require_navigation_complete(empty, all_modules());
    }, "registered module without route rejected");

    shell::require_navigation_complete(
        empty, {protocol::ModuleId::files}, {protocol::ModuleId::files});
    t::check(true, "explicit registered headless exception accepted");
    rejects<std::logic_error>([&] {
        shell::require_navigation_complete(
            manifest, all_modules(), {protocol::ModuleId::files});
    }, "module cannot be both represented and headless");
    rejects<std::invalid_argument>([&] {
        shell::require_navigation_complete(empty, {}, {protocol::ModuleId::files});
    }, "unregistered headless exception rejected");

    return t::report();
}
