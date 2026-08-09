#pragma once

#if defined(SQUIFLOW_WITH_QT)

// The shipping Qt-lane implementation of StartupServices. Every door the
// startup ADR orders is exercised against the real machine here: paths,
// logging, crash handling, single-instance activation, secrets, SQLite,
// integrity, device/shop identity, module registration, the QML surface and
// the reverse shutdown. Nothing in this file composes application state: the
// ordering authority stays in RealStartupRuntime.

#include "app/real_startup_runtime.hpp"
#include "platform/paths.hpp"

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace squiflow::platform {
class AsyncLogSink;
class CrashBreadcrumb;
class CrashHandler;
class Logger;
class LocalLogStorage;
class PathLayout;
class RotatingLogFile;
class SecretStore;
class SingleInstanceLock;
class SystemLogClock;
}  // namespace squiflow::platform

namespace squiflow::shell {
class QmlSurfaceQt;
}

namespace squiflow::app {
class AuthenticatedWorkspace;
}

namespace squiflow::app {

class RealStartupServices final : public StartupServices {
  public:
    RealStartupServices();
    ~RealStartupServices() override;

    RealStartupServices(const RealStartupServices&) = delete;
    RealStartupServices& operator=(const RealStartupServices&) = delete;

    // The shell asks the window to close; the composition root decides what
    // that means for the application.
    void set_shutdown_request(std::function<void(ShutdownReason)> request);
    bool shell_completed() const noexcept { return surface_ != nullptr; }
    platform::Logger* logger() noexcept { return logger_.get(); }

    StepResult start_paths() override;
    void stop_paths() noexcept override;
    StepResult start_logging() override;
    void stop_logging() noexcept override;
    StepResult start_crash_handler() override;
    void stop_crash_handler() noexcept override;
    StepResult acquire_single_instance() override;
    void release_single_instance() noexcept override;
    StoreConnection connect_store() override;
    bool integrity_ok(engine::Database& database,
                      std::string& detail) override;
    SessionLoadResult load_session(engine::Database& database) override;
    StepResult start_shell(const protocol::Activation& activation,
                           const engine::RightsSet& rights,
                           const std::vector<protocol::ModuleId>& registered,
                           std::uint64_t session_generation,
                           AuthenticatedWorkspace& workspace) override;
    StepResult start_window() override;
    void stop_window() noexcept override;
    void stop_shell() noexcept override;
    void rollback_diagnostic(const RollbackFailure& failure) noexcept override;

  private:
    std::function<void(ShutdownReason)> shutdown_request_{};
    std::optional<platform::PathLayout> layout_{};
    std::vector<std::string> path_warnings_{};
    std::unique_ptr<platform::SystemLogClock> log_clock_{};
    std::unique_ptr<platform::LocalLogStorage> log_storage_{};
    std::unique_ptr<platform::RotatingLogFile> log_file_{};
    std::unique_ptr<platform::AsyncLogSink> log_sink_{};
    std::unique_ptr<platform::Logger> logger_{};
    std::unique_ptr<platform::CrashBreadcrumb> breadcrumb_{};
    std::unique_ptr<platform::CrashHandler> crash_handler_{};
    std::unique_ptr<platform::SecretStore> secrets_{};
    std::unique_ptr<platform::SingleInstanceLock> single_instance_{};
    std::unique_ptr<shell::QmlSurfaceQt> surface_{};
    bool logging_started_{false};
    bool paths_started_{false};
};

}  // namespace squiflow::app

#endif
