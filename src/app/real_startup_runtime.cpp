#include "app/real_startup_runtime.hpp"

#include "app/composition_root.hpp"
#include "engine/storage/database.hpp"
#include "modules/administration/data/repository.hpp"

#include <exception>
#include <stdexcept>
#include <utility>

namespace squiflow::app {
namespace {

StepResult failed(std::string message) {
    return {StepDisposition::Failed, bounded_lifecycle_message(message)};
}

}  // namespace

RealStartupRuntime::RealStartupRuntime(StartupServices& services, Clock clock)
    : services_(services), clock_(std::move(clock)) {
    if (!clock_) {
        throw std::invalid_argument("real startup runtime needs a clock");
    }
}

StepResult RealStartupRuntime::start(StartupStep step) {
    switch (step) {
        case StartupStep::Paths: return services_.start_paths();
        case StartupStep::Logging: return services_.start_logging();
        case StartupStep::CrashHandler: return services_.start_crash_handler();
        case StartupStep::SingleInstance: return services_.acquire_single_instance();
        case StartupStep::DatabaseConnection: return start_database();
        case StartupStep::Migrations: return start_migrations();
        case StartupStep::IntegrityCheck: return start_integrity();
        case StartupStep::IdentitySession: return start_identity();
        case StartupStep::Activation: return start_activation();
        case StartupStep::ModuleRegistration: return start_modules();
        case StartupStep::Shell: return start_shell();
        case StartupStep::Window: return services_.start_window();
    }
    return failed("unknown startup step");
}

StepResult RealStartupRuntime::start_database() {
    StoreConnection connection = services_.connect_store();
    if (!connection.store) {
        return failed(connection.message.empty() ? "database connection failed"
                                                 : connection.message);
    }
    connected_store_ = std::move(connection.store);
    return {};
}

StepResult RealStartupRuntime::start_migrations() {
    if (!connected_store_) {
        return failed("database store was not connected");
    }
    try {
        // Module factories own their migration definitions. A short-lived
        // registry gathers those definitions here; the live registry is
        // created only at the fixed ModuleRegistration step.
        modules::Registry migration_registry{clock_};
        register_all_modules(migration_registry, clock_);
        engine::MigrationRunner runner{clock_};
        migration_registry.collect_migrations(runner);
        auto database = std::make_unique<engine::Database>(
            std::move(connected_store_), std::move(runner));
        database->open();
        database_ = std::move(database);
        return {};
    } catch (const std::exception& error) {
        database_.reset();
        connected_store_.reset();
        return failed(error.what());
    }
}

StepResult RealStartupRuntime::start_integrity() {
    if (!database_ || !database_->ready()) {
        return failed("database is not ready for integrity checking");
    }
    std::string detail;
    if (!services_.integrity_ok(*database_, detail)) {
        return failed(detail.empty() ? "database integrity check failed" : detail);
    }
    return {};
}

StepResult RealStartupRuntime::start_identity() {
    if (!database_ || !database_->ready()) {
        return failed("database is not ready for identity loading");
    }
    SessionLoadResult loaded = services_.load_session(*database_);
    if (!loaded.ok || !loaded.session.is_signed_in() || loaded.generation == 0) {
        return failed(loaded.message.empty() ? "no valid identity session"
                                             : loaded.message);
    }
    session_ = std::move(loaded.session);
    session_generation_ = loaded.generation;
    return {};
}

StepResult RealStartupRuntime::start_activation() {
    if (!database_) {
        return failed("database is not available for activation");
    }
    try {
        database_->read([this](const engine::Store& store) {
            disabled_modules_ =
                modules::administration::data::disabled_modules(store);
        });
        const protocol::ActivationResult activation =
            protocol::resolve_activation(disabled_modules_);
        if (!activation.ok) {
            return failed(activation.error);
        }
        return {};
    } catch (const std::exception& error) {
        return failed(error.what());
    }
}

StepResult RealStartupRuntime::start_modules() {
    try {
        auto registry = std::make_unique<modules::Registry>(clock_);
        register_all_modules(*registry, clock_);
        registry->set_disabled(disabled_modules_);
        registry_ = std::move(registry);
        return {};
    } catch (const std::exception& error) {
        return failed(error.what());
    }
}

StepResult RealStartupRuntime::start_shell() {
    if (!registry_ || !session_) {
        return failed("shell prerequisites are incomplete");
    }
    return services_.start_shell(registry_->activation(), session_->rights,
                                 registry_->registered(), session_generation_);
}

void RealStartupRuntime::stop(StartupStep step, ShutdownReason) {
    switch (step) {
        case StartupStep::Window:
            services_.stop_window();
            break;
        case StartupStep::Shell:
            services_.stop_shell();
            break;
        case StartupStep::ModuleRegistration:
            registry_.reset();
            break;
        case StartupStep::Activation:
            disabled_modules_.clear();
            break;
        case StartupStep::IdentitySession:
            session_.reset();
            session_generation_ = 0;
            break;
        case StartupStep::IntegrityCheck:
            break;
        case StartupStep::Migrations:
            if (database_) {
                database_->close();
            }
            break;
        case StartupStep::DatabaseConnection:
            database_.reset();
            connected_store_.reset();
            break;
        case StartupStep::SingleInstance:
            services_.release_single_instance();
            break;
        case StartupStep::CrashHandler:
            services_.stop_crash_handler();
            break;
        case StartupStep::Logging:
            services_.stop_logging();
            break;
        case StartupStep::Paths:
            services_.stop_paths();
            break;
    }
}

void RealStartupRuntime::rollback_diagnostic(
    const RollbackFailure& failure) noexcept {
    services_.rollback_diagnostic(failure);
}

}  // namespace squiflow::app
