#include "app/real_startup_runtime.hpp"
#include "app/startup_sequence.hpp"
#include "app/workspace_runtime.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "support/check.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace app = squiflow::app;
namespace engine = squiflow::engine;
namespace platform = squiflow::platform;
namespace protocol = squiflow::protocol;
using squiflow::testing::check;
using squiflow::testing::section;

namespace {

const std::string kPerson = "c1000000000000000000000000000001";
const std::string kDevice = "c1000000000000000000000000000002";

struct Services final : app::StartupServices {
    bool secondary{false};
    bool healthy{true};
    bool shell_started{false};
    bool window_started{false};
    bool workspace_attached{false};
    bool instance_released{false};
    bool logging_stopped{false};
    bool paths_stopped{false};
    std::size_t registered_count{0};
    std::uint64_t seen_generation{0};
    std::vector<app::RollbackFailure> diagnostics{};

    app::StepResult start_paths() override { return {}; }
    void stop_paths() noexcept override { paths_stopped = true; }
    app::StepResult start_logging() override { return {}; }
    void stop_logging() noexcept override { logging_stopped = true; }
    app::StepResult start_crash_handler() override { return {}; }
    void stop_crash_handler() noexcept override {}
    app::StepResult acquire_single_instance() override {
        return secondary
            ? app::StepResult{app::StepDisposition::SecondaryInstance, {}}
            : app::StepResult{};
    }
    void release_single_instance() noexcept override {
        instance_released = true;
    }
    app::StoreConnection connect_store() override {
        return {std::make_unique<engine::MemoryStore>(), {}};
    }
    bool integrity_ok(engine::Database& database,
                      std::string& detail) override {
        detail = healthy ? std::string{} : std::string{"integrity failed"};
        return healthy && database.ready();
    }
    app::SessionLoadResult load_session(engine::Database&) override {
        engine::Session session;
        session.person = engine::record_id_from_string(kPerson);
        session.device = engine::record_id_from_string(kDevice);
        session.display_name = "Owner";
        session.is_owner = true;
        session.rights.grant_all();
        return {true, std::move(session), 7, {}};
    }
    app::StepResult start_shell(
        const protocol::Activation& activation,
        const engine::RightsSet& rights,
        const std::vector<protocol::ModuleId>& registered,
        std::uint64_t session_generation,
        app::AuthenticatedWorkspace& workspace) override {
        shell_started = activation.is_active(protocol::ModuleId::administration) &&
                        rights.count() == protocol::kRightCount;
        registered_count = registered.size();
        seen_generation = session_generation;
        workspace_attached = workspace.signed_in() &&
                             workspace.current_session().is_signed_in();
        return shell_started && workspace_attached
            ? app::StepResult{}
            : app::StepResult{app::StepDisposition::Failed,
                              "invalid shell access"};
    }
    app::StepResult start_window() override {
        window_started = shell_started;
        return window_started ? app::StepResult{}
                              : app::StepResult{app::StepDisposition::Failed,
                                                "shell missing"};
    }
    void stop_window() noexcept override { window_started = false; }
    void stop_shell() noexcept override { shell_started = false; }
    platform::Logger* logger() noexcept override { return nullptr; }
    void rollback_diagnostic(
        const app::RollbackFailure& failure) noexcept override {
        diagnostics.push_back(failure);
    }
};

constexpr std::int64_t now() { return 1'800'000'000'000; }

}  // namespace

int main() {
    section("real runtime composes every startup concern");
    Services services;
    app::RealStartupRuntime runtime(services, now);
    app::StartupSequence sequence(runtime);
    const auto started = sequence.start();
    check(started.disposition == app::StartupDisposition::Running,
          "all twelve concrete steps reach running");
    const auto declared_order = app::startup_order();
    check(sequence.completed_steps() ==
              std::vector<app::StartupStep>(declared_order.begin(),
                                            declared_order.end()),
          "runtime follows the one declared order");
    check(runtime.database() && runtime.database()->ready() &&
              runtime.database()->version() >= 25,
          "module migrations opened a current database");
    check(runtime.registry() &&
              runtime.registry()->size() == protocol::kModuleCount &&
              runtime.registry()->unhandled().empty(),
          "live composition has every module and workflow");
    check(runtime.session() && runtime.session()->is_signed_in(),
          "identity session is retained");
    check(runtime.workspace() && runtime.workspace()->signed_in(),
          "authenticated workspace is composed and signed in");
    check(services.shell_started && services.window_started,
          "shell and window see completed application state");
    check(services.workspace_attached,
          "shell received the live authenticated workspace");
    check(services.registered_count == protocol::kModuleCount &&
              services.seen_generation == 7,
          "shell receives owned activation inputs");
    sequence.shutdown(app::ShutdownReason::WindowClosed);
    check(!services.window_started && !services.shell_started,
          "window close uses reverse lifecycle teardown");
    check(services.instance_released && services.logging_stopped &&
              services.paths_stopped,
          "platform resources are released");
    check(runtime.database() == nullptr && runtime.registry() == nullptr &&
              !runtime.session() && runtime.workspace() == nullptr,
          "runtime releases database, registry, session and workspace");

    section("secondary instance never connects storage");
    Services secondary_services;
    secondary_services.secondary = true;
    app::RealStartupRuntime secondary_runtime(secondary_services, now);
    app::StartupSequence secondary(secondary_runtime);
    const auto secondary_result = secondary.start();
    check(secondary_result.disposition ==
              app::StartupDisposition::SecondaryInstance,
          "secondary disposition is preserved");
    check(secondary_runtime.database() == nullptr &&
              secondary_runtime.registry() == nullptr,
          "secondary starts no database or modules");
    check(secondary_services.logging_stopped && secondary_services.paths_stopped,
          "secondary releases pre-instance resources");

    section("integrity failure unwinds concrete resources");
    Services damaged_services;
    damaged_services.healthy = false;
    app::RealStartupRuntime damaged_runtime(damaged_services, now);
    app::StartupSequence damaged(damaged_runtime);
    const auto damaged_result = damaged.start();
    check(damaged_result.disposition == app::StartupDisposition::Failed &&
              damaged_result.failure &&
              damaged_result.failure->step == app::StartupStep::IntegrityCheck,
          "integrity failure names the exact startup step");
    check(damaged_runtime.database() == nullptr &&
              damaged_runtime.registry() == nullptr,
          "integrity failure leaves no live application resources");
    check(damaged_services.instance_released &&
              damaged_services.logging_stopped && damaged_services.paths_stopped,
          "integrity failure unwinds platform resources");

    return squiflow::testing::report();
}
