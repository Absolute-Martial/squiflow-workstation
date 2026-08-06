#pragma once

#include "app/startup.hpp"
#include "engine/identity/session.hpp"
#include "engine/storage/store.hpp"
#include "modules/registry.hpp"

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace squiflow::engine {
class Database;
}

namespace squiflow::app {

struct StoreConnection final {
    std::unique_ptr<engine::Store> store{};
    std::string message{};
};

struct SessionLoadResult final {
    bool ok{false};
    engine::Session session{};
    std::uint64_t generation{0};
    std::string message{};
};

// Host-specific doors only. Ordering, migrations, activation and application
// composition remain owned by RealStartupRuntime and cannot be reordered by a
// platform adapter.
class StartupServices {
  public:
    virtual ~StartupServices() = default;
    virtual StepResult start_paths() = 0;
    virtual void stop_paths() noexcept = 0;
    virtual StepResult start_logging() = 0;
    virtual void stop_logging() noexcept = 0;
    virtual StepResult start_crash_handler() = 0;
    virtual void stop_crash_handler() noexcept = 0;
    virtual StepResult acquire_single_instance() = 0;
    virtual void release_single_instance() noexcept = 0;
    virtual StoreConnection connect_store() = 0;
    virtual bool integrity_ok(engine::Database& database,
                              std::string& detail) = 0;
    virtual SessionLoadResult load_session(engine::Database& database) = 0;
    virtual StepResult start_shell(
        const protocol::Activation& activation,
        const engine::RightsSet& rights,
        const std::vector<protocol::ModuleId>& registered,
        std::uint64_t session_generation) = 0;
    virtual StepResult start_window() = 0;
    virtual void stop_window() noexcept = 0;
    virtual void stop_shell() noexcept = 0;
    virtual void rollback_diagnostic(const RollbackFailure& failure) noexcept = 0;
};

class RealStartupRuntime final : public StartupRuntime {
  public:
    using Clock = std::function<std::int64_t()>;

    RealStartupRuntime(StartupServices& services, Clock clock);
    StepResult start(StartupStep step) override;
    void stop(StartupStep step, ShutdownReason reason) override;
    void rollback_diagnostic(const RollbackFailure& failure) noexcept override;

    const modules::Registry* registry() const noexcept { return registry_.get(); }
    const engine::Database* database() const noexcept { return database_.get(); }
    const std::optional<engine::Session>& session() const noexcept { return session_; }

  private:
    StepResult start_database();
    StepResult start_migrations();
    StepResult start_integrity();
    StepResult start_identity();
    StepResult start_activation();
    StepResult start_modules();
    StepResult start_shell();

    StartupServices& services_;
    Clock clock_;
    std::unique_ptr<engine::Store> connected_store_{};
    std::unique_ptr<engine::Database> database_{};
    std::unique_ptr<modules::Registry> registry_{};
    std::optional<engine::Session> session_{};
    std::vector<protocol::ModuleId> disabled_modules_{};
    std::uint64_t session_generation_{0};
};

}  // namespace squiflow::app
