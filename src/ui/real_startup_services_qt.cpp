#if defined(SQUIFLOW_WITH_QT)

#include "ui/real_startup_services_qt.hpp"

// Platform headers first: Qt defines an empty `emit` macro, and log_throttle
// declares a member named `emit`, so every platform header must be parsed
// before Qt headers reach the translation unit.
#include "app/qt_message_mapping.hpp"
#include "app/workspace_runtime.hpp"
#include "engine/records/identity.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/storage/sqlite_store.hpp"
#include "modules/administration/data/repository.hpp"
#include "modules/administration/domain/device.hpp"
#include "modules/administration/domain/person.hpp"
#include "platform/async_log_sink.hpp"
#include "platform/crash_breadcrumb.hpp"
#include "platform/crash_handler.hpp"
#include "platform/local_directory_probe.hpp"
#include "platform/local_log_storage.hpp"
#include "platform/log_clock.hpp"
#include "platform/logger.hpp"
#include "platform/path_environment.hpp"
#include "platform/paths.hpp"
#include "platform/rotating_log_file.hpp"
#include "platform/secrets.hpp"
#include "platform/single_instance.hpp"
#include "shell/screen_registry.hpp"
// The shell surface brings Qt into this translation unit; it must be the
// last second first-party header so that no macOS Qt header is parsed before
// the platform namespace (see the comment at the top).
#include "shell/qml_surface_qt.hpp"

#include <QByteArray>
#include <QMessageLogContext>
#include <QString>
#include <QtGlobal>

#include <cstdlib>
#include <optional>
#include <span>
#include <string_view>
#include <utility>

namespace squiflow::app {
namespace {

namespace p = squiflow::platform;

constexpr char kSessionGenerationSecret[] = "workstation.session.generation";

// The shell requires a tenant label on every read and write that flows
// through the authenticated workspace. This machine holds exactly one shop's
// records, so the tenant question is "which shop's counter is this", and the
// answer lives in the machine's own record: the device row is persisted,
// real, and unique to this shop. A synthetic tenant constant would be a lie
// told to every query; the device id is the truth the store already holds.
// The server assigns the authoritative shop tenant at first pairing (Phase
// 8), and nothing here ever invents one.
TenantId tenant_of(const engine::Session& session) {
    return TenantId{session.device};
}

p::LogLevel qt_level(QtMsgType type) noexcept {
    switch (type) {
        case QtDebugMsg:
            return p::LogLevel::Debug;
        case QtInfoMsg:
            return p::LogLevel::Info;
        case QtWarningMsg:
            return p::LogLevel::Warning;
        case QtCriticalMsg:
        case QtFatalMsg:
            return p::LogLevel::Error;
    }
    return p::LogLevel::Warning;
}

p::Logger* g_active_logger = nullptr;

void forward_qt_message(QtMsgType type, const QMessageLogContext&,
                        const QString& message) {
    p::Logger* logger = g_active_logger;
    if (logger == nullptr) {
        return;
    }
    QtMessageRecursionGuard guard;
    if (!guard.entered()) {
        return;
    }
    const QByteArray utf8 = message.toUtf8();
    logger->log(qt_level(type), "qt",
                std::string_view(utf8.constData(), utf8.size()));
}

bool parse_generation(std::string_view text, std::uint64_t& value) noexcept {
    if (text.empty()) {
        return false;
    }
    char* end = nullptr;
    const unsigned long long parsed = std::strtoull(text.data(), &end, 10);
    if (end == text.data() || *end != '\0') {
        return false;
    }
    value = static_cast<std::uint64_t>(parsed);
    return true;
}

bool persist_generation(p::SecretStore& secrets,
                        std::uint64_t generation) noexcept {
    const std::string text = std::to_string(generation);
    const auto* bytes = reinterpret_cast<const std::byte*>(text.data());
    const p::SecretWriteResult result = secrets.store(
        kSessionGenerationSecret, std::span<const std::byte>(bytes, text.size()));
    return result.ok;
}

}  // namespace

RealStartupServices::RealStartupServices() = default;

RealStartupServices::~RealStartupServices() {
    stop_logging();
}

void RealStartupServices::set_shutdown_request(
    std::function<void(ShutdownReason)> request) {
    shutdown_request_ = std::move(request);
}

StepResult RealStartupServices::start_paths() {
    p::EnvironmentDiscovery discovery = p::discover_path_environment(
        p::ApplicationIdentity{"SquiFlow", "SquiFlow"});
    if (!discovery.ok) {
        return {StepDisposition::Failed, discovery.message};
    }
    p::LocalDirectoryProbe probe;
    p::PathResolver resolver(probe);
    p::PathResolution resolution = resolver.resolve(discovery.environment);
    if (!resolution.ok) {
        std::string detail =
            resolution.message.empty() ? std::string("path resolution failed")
                                       : resolution.message;
        if (!resolution.offending_path.empty()) {
            detail += ": " + resolution.offending_path;
        }
        return {StepDisposition::Failed, std::move(detail)};
    }
    layout_ = std::move(resolution.layout);
    path_warnings_ = std::move(resolution.warnings);
    paths_started_ = true;
    return {};
}

void RealStartupServices::stop_paths() noexcept {
    paths_started_ = false;
    path_warnings_.clear();
    layout_.reset();
}

StepResult RealStartupServices::start_logging() {
    if (!layout_) {
        return {StepDisposition::Failed,
                "paths must be resolved before logging starts"};
    }
    try {
        const p::LogRotationPolicy policy{};
        // The clock must outlive both the sink and the logger, and they both
        // hold it by reference.
        log_clock_ = std::make_unique<p::SystemLogClock>();
        log_storage_ = std::make_unique<p::LocalLogStorage>(
            layout_->directory(p::PathRole::Logs));
        log_file_ =
            std::make_unique<p::RotatingLogFile>(*log_storage_, policy);
        log_sink_ =
            std::make_unique<p::AsyncLogSink>(*log_file_, *log_clock_);
        logger_ = std::make_unique<p::Logger>(*log_sink_, *log_clock_);
        breadcrumb_ = std::make_unique<p::CrashBreadcrumb>();
        logger_->set_breadcrumb(breadcrumb_.get());
        for (const std::string& warning : path_warnings_) {
            logger_->warning("startup.paths", warning);
        }
        g_active_logger = logger_.get();
        qInstallMessageHandler(forward_qt_message);
        logging_started_ = true;
        logger_->info(
            "startup.logging", "logging started",
            {p::LogField{"directory", layout_->directory(p::PathRole::Logs)}});
        return {};
    } catch (const std::exception& error) {
        stop_logging();
        return {StepDisposition::Failed, error.what()};
    }
}

void RealStartupServices::stop_logging() noexcept {
    logging_started_ = false;
    g_active_logger = nullptr;
    try {
        if (logger_) {
            logger_->flush();
        }
    } catch (...) {
    }
    breadcrumb_.reset();
    logger_.reset();
    log_sink_.reset();
    log_file_.reset();
    log_storage_.reset();
    log_clock_.reset();
}

StepResult RealStartupServices::start_crash_handler() {
    if (!layout_ || !log_sink_) {
        return {StepDisposition::Failed,
                "crash handling needs resolved paths and a live log sink"};
    }
    try {
        crash_handler_ = p::make_crash_handler();
        // The async sink is the crash handler's direct sink: its writer thread
        // drains on flush, and the crash evidence is written after the app has
        // already stopped doing anything useful. This is deliberate - the
        // handler never touches the rotating file concurrently with the
        // logger.
        if (!crash_handler_->install(layout_->directory(p::PathRole::Crash),
                                     *log_sink_, *breadcrumb_)) {
            crash_handler_.reset();
            return {StepDisposition::Failed,
                    "crash handler refused the crash directory"};
        }
        return {};
    } catch (const std::exception& error) {
        crash_handler_.reset();
        return {StepDisposition::Failed, error.what()};
    }
}

void RealStartupServices::stop_crash_handler() noexcept {
    if (crash_handler_) {
        crash_handler_->uninstall();
    }
    crash_handler_.reset();
}

StepResult RealStartupServices::acquire_single_instance() {
    if (!layout_) {
        return {StepDisposition::Failed,
                "single instance needs a resolved data directory"};
    }
    try {
        // The lock contract is (application id, data directory): the platform
        // adapter derives its names from those, so the shell never hands it
        // pre-built names.
        auto lock = p::make_single_instance_lock();
        const p::InstanceAcquireResult result = lock->acquire(
            "squiflow", layout_->directory(p::PathRole::Data));
        switch (result.state) {
            case p::InstanceState::Primary:
                single_instance_ = std::move(lock);
                return {};
            case p::InstanceState::Secondary:
                return {StepDisposition::SecondaryInstance, {}};
            case p::InstanceState::Failed:
                return {StepDisposition::Failed, result.message};
            case p::InstanceState::Idle:
                return {StepDisposition::Failed,
                        "single instance lock returned idle"};
        }
    } catch (const std::exception& error) {
        return {StepDisposition::Failed, error.what()};
    }
    return {StepDisposition::Failed, "single instance refusal"};
}

void RealStartupServices::release_single_instance() noexcept {
    if (single_instance_) {
        single_instance_->release();
    }
    single_instance_.reset();
}

StoreConnection RealStartupServices::connect_store() {
    if (!layout_) {
        return {nullptr, "paths were not resolved"};
    }
#if defined(SQUIFLOW_WITH_SQLITE)
    try {
        auto store = std::make_unique<engine::SqliteStore>(
            layout_->database_file());
        return {std::move(store), {}};
    } catch (const std::exception& error) {
        return {nullptr, error.what()};
    }
#else
    // The verification lane has no SQLite (see engine CMake comment): the
    // real, persisted store is a Windows/MSVC artifact, so this lane is
    // forbidden from pretending a file ever held a record. The memory store
    // keeps the identical door order and the identical session rules; it
    // just cannot survive a process exit, which is exactly what the
    // verification lane exists to say about itself.
    static_cast<void>(layout_);
    return {std::make_unique<engine::MemoryStore>(),
            "verification lane: in-memory store, no persistence"};
#endif
}

bool RealStartupServices::integrity_ok(engine::Database& database,
                                      std::string& detail) {
    if (!database.ready()) {
        detail = "database is not ready";
        return false;
    }
    try {
        database.read([&](const engine::Store& store) {
            // Reading the person table proves the writable store actually
            // answers: a database that cannot be read cannot serve the shop
            // even if every directory exists. The shop's first run has zero
            // persons, which is a prefix of being provisioned, not a fault.
            using squiflow::modules::administration::tables::kPerson;
            engine::Query query{kPerson};
            static_cast<void>(store.select(query));
        });
        return true;
    } catch (const std::exception& error) {
        detail = error.what();
        return false;
    }
}

SessionLoadResult RealStartupServices::load_session(engine::Database& database) {
    if (!layout_) {
        return {false, engine::Session{}, 0, "paths are not resolved"};
    }
    if (!secrets_) {
        try {
            secrets_ = p::make_secret_store(layout_->directory(p::PathRole::Secrets));
        } catch (const std::exception& error) {
            return {false, engine::Session{}, 0, error.what()};
        }
        if (!secrets_) {
            return {false, engine::Session{}, 0,
                    "the secrets store is unavailable"};
        }
    }

    // 1. The operator. The signed-in person comes from the device's own
    //    records; if the machine has never been provisioned there is no
    //    person and the workstation must not invent one. First-run
    //    provisioning is a human decision, not a startup default.
    modules::administration::Person person;
    bool found = false;
    try {
        database.read([&](const engine::Store& store) {
            const std::vector<modules::administration::Person> people =
                modules::administration::data::all_people(store);
            const std::optional<modules::administration::Person> selected =
                [&people]() {
                    std::optional<modules::administration::Person> chosen;
                    for (const auto& candidate : people) {
                        if (!chosen || candidate.is_owner) {
                            chosen = candidate;
                        }
                    }
                    return chosen;
                }();
            found = selected.has_value();
            if (found) {
                person = *selected;
            }
        });
    } catch (const std::exception& error) {
        return {false, engine::Session{}, 0, error.what()};
    }
    if (!found) {
        return {false, engine::Session{}, 0,
                "this device has no provisioned operator"};
    }

    // 2. The machine identity. Every write is stamped with the device that
    //    made it; booting without one would corrupt that provenance, so an
    //    unprovisioned or retired machine is refused.
    std::optional<std::string> device_id;
    try {
        database.read([&](const engine::Store& store) {
            engine::Query query{squiflow::modules::administration::tables::kDevice};
            const std::vector<engine::Row> rows = store.select(query);
            for (const engine::Row& row : rows) {
                const modules::administration::Device machine =
                    modules::administration::device_from_row(row);
                if (!machine.retired) {
                    device_id = machine.id;
                    break;
                }
            }
        });
    } catch (const std::exception& error) {
        return SessionLoadResult{false, engine::Session{}, 0, error.what()};
    }
    if (!device_id) {
        return SessionLoadResult{false, engine::Session{}, 0,
                                 "this device has no active machine record"};
    }

    // 3. Session: display name, ownership, and the rights actually granted to
    //    this person in the store. There are no synthetic rights here; an
    //    empty rights list is an empty rights list.
    engine::Session session;
    session.person = engine::record_id_from_string(person.id);
    session.device = engine::record_id_from_string(*device_id);
    session.display_name = person.display_name;
    session.is_owner = person.is_owner;
    try {
        database.read([&](const engine::Store& store) {
            session.rights =
                modules::administration::data::rights_of(store, person.id);
        });
    } catch (const std::exception& error) {
        return SessionLoadResult{false, engine::Session{}, 0, error.what()};
    }
    if (!session.is_signed_in()) {
        return SessionLoadResult{false, engine::Session{}, 0,
                                 "person identity is invalid"};
    }

    // 4. The session generation lives with the machine, not the person: it
    //    climbs once per boot and never stays zero, so a screen context
    //    captured under an earlier boot can never be mistaken for one
    //    captured under this one.
    std::uint64_t generation = 1;
    const p::SecretReadResult previous = secrets_->load(kSessionGenerationSecret);
    if (previous.ok) {
        std::string text;
        const auto bytes = previous.value.bytes();
        text.assign(reinterpret_cast<const char*>(bytes.data()), bytes.size());
        static_cast<void>(parse_generation(text, generation));
    }
    if (!persist_generation(*secrets_, generation)) {
        return SessionLoadResult{false, engine::Session{}, 0,
                                 "session generation could not be persisted"};
    }
    return SessionLoadResult{true, std::move(session), generation, {}};
}

StepResult RealStartupServices::start_shell(
    const protocol::Activation& activation, const engine::RightsSet& rights,
    const std::vector<protocol::ModuleId>& registered,
    std::uint64_t session_generation, AuthenticatedWorkspace& workspace) {
    if (surface_) {
        return {StepDisposition::Failed, "shell already started"};
    }
    auto surface = std::make_unique<shell::QmlSurfaceQt>(
        [this](ShutdownReason reason) {
            if (shutdown_request_) {
                shutdown_request_(reason);
            }
        });
    const StepResult shell = surface->startShell();
    if (shell.disposition == StepDisposition::Failed) {
        return shell;
    }
    if (logger_) {
        logger_->info(
            "startup.shell", "shell started",
            {p::LogField{"session_generation", std::to_string(session_generation)}});
    }
    // Rights, registration and activation arrive from the runtime; the
    // surface only reflects them.
    surface->publishNavigationAccess(shell::make_navigation_access(
        activation, rights, registered, session_generation, 0));
    surface->attachWorkspace(workspace, tenant_of(workspace.current_session()),
                             activation);
    surface_ = std::move(surface);
    return {};
}

StepResult RealStartupServices::start_window() {
    if (!surface_) {
        return {StepDisposition::Failed, "shell must start before a window"};
    }
    return surface_->startWindow();
}

void RealStartupServices::stop_window() noexcept {
    if (surface_) {
        surface_->stopWindow();
    }
}

void RealStartupServices::stop_shell() noexcept {
    if (surface_) {
        surface_->stopShell();
        surface_.reset();
    }
}

void RealStartupServices::rollback_diagnostic(
    const RollbackFailure& failure) noexcept {
    if (!logger_) {
        return;
    }
    try {
        logger_->error("startup", failure.message);
    } catch (...) {
    }
}

}  // namespace squiflow::app

#else
#endif
