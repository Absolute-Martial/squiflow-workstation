#include "app/composition_root.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

#include <cstdint>
#include <set>

int main() {
    namespace app = squiflow::app;
    namespace protocol = squiflow::protocol;
    namespace test = squiflow::testing;

    squiflow::modules::Registry registry{[] {
        return std::int64_t{1'800'000'000'000};
    }};
    app::register_all_modules(registry, [] {
        return std::int64_t{1'800'000'000'000};
    });

    test::section("production composition is complete");
    test::check(registry.size() == protocol::kModuleCount,
                "all protocol modules are registered");
    test::check(registry.unhandled().empty(),
                "every ordinary and workflow operation has a handler");
    const auto registered = registry.registered();
    test::check(std::set<protocol::ModuleId>(registered.begin(), registered.end()).size() ==
                    protocol::kModuleCount,
                "module identities are unique");
    for (const auto operation : protocol::workflow_operations()) {
        test::check(registry.workflow_available(operation),
                    "every production workflow is available");
    }

    return test::report();
}
