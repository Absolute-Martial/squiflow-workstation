#if defined(SQUIFLOW_WITH_QT)

// The application entry point drives the fixed startup door sequence through
// StartupSequence and keeps the single host (QmlSurfaceQt in
// real_startup_services_qt.cpp) attached to that lifecycle. The surface owns
// the QML root and the requestShutdown slot by which Main.qml asks the
// composition to wind down; this file only installs the bridge between that
// request and the application shutdown. The entry point never grants rights:
// access arrives from the authenticated session composed by StartupRuntime.

#include "app/application.hpp"
#include "app/real_startup_runtime.hpp"
#include "app/workspace_runtime.hpp"
#include "engine/identity/session.hpp"
#include "platform/logger.hpp"
#include "platform/network_monitor.hpp"
#include "platform/network_state.hpp"
#include "platform/network_state_qt.hpp"
#include "ui/real_startup_services_qt.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QString>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string_view>
#include <utility>

namespace {

squiflow::engine::ConnectionState workspace_connection(
    const squiflow::platform::NetworkSnapshot& state) noexcept {
    switch (state.reachability) {
        case squiflow::platform::NetworkReachability::Online:
        case squiflow::platform::NetworkReachability::SiteOnly:
            return state.metered
                       ? squiflow::engine::ConnectionState::Metered
                       : squiflow::engine::ConnectionState::Online;
        case squiflow::platform::NetworkReachability::LocalOnly:
        case squiflow::platform::NetworkReachability::Offline:
        case squiflow::platform::NetworkReachability::Unknown:
            return squiflow::engine::ConnectionState::Offline;
    }
    return squiflow::engine::ConnectionState::Offline;
}

std::int64_t startup_clock() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::system_clock::now().time_since_epoch())
        .count();
}

}  // namespace

int squiflow_workstation_main_qt(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("SquiFlow"));
    QCoreApplication::setOrganizationName(QStringLiteral("SquiFlow"));

    squiflow::platform::NetworkMonitor network_monitor;
    squiflow::platform::QtNetworkStateAdapter network_adapter(network_monitor);

    squiflow::app::RealStartupServices services;
    squiflow::app::RealStartupRuntime runtime(services, startup_clock);
    squiflow::app::Application application_host(runtime);

    services.set_shutdown_request(
        [&application_host, &application](squiflow::app::ShutdownReason reason) {
            application_host.shutdown(reason);
            application.quit();
        });

    const auto started = application_host.start();
    // A CI smoke launch on a machine that has never been provisioned still
    // exercises every startup door; the expected refusal is not an error
    // there. Production launches return 2 so the operator never mistakes an
    // unprovisioned machine for a running shell.
    const bool smoke_test =
        std::any_of(argv + 1, argv + argc, [](const char* value) {
            return QString::fromUtf8(value) == QStringLiteral("--smoke-test");
        });
    switch (started.disposition) {
        case squiflow::app::StartupDisposition::Running:
            break;
        case squiflow::app::StartupDisposition::SecondaryInstance:
            return 0;
        case squiflow::app::StartupDisposition::Failed:
            if (services.logger() != nullptr) {
                services.logger()->error(
                    "startup.identity",
                    started.failure.has_value()
                        ? started.failure->message
                        : "startup refused for an unknown reason");
            }
            if (smoke_test && started.failure.has_value() &&
                started.failure->step ==
                    squiflow::app::StartupStep::IdentitySession) {
                return 0;
            }
            return 2;
    }

    network_monitor.subscribe([&runtime](const squiflow::platform::NetworkSnapshot& snapshot) {
        if (runtime.active_workspace() != nullptr) {
            runtime.active_workspace()->set_connection_state(
                workspace_connection(snapshot));
        }
    });

    // CI smoke launch: give the shell a moment to mount, then take the same
    // reverse-shutdown path a window close would take.
    const bool smoke_running =
        std::any_of(argv + 1, argv + argc, [](const char* value) {
            return QString::fromUtf8(value) == QStringLiteral("--smoke-test");
        });
    if (smoke_running) {
        QTimer::singleShot(
            500, &application,
            [&application_host, &application] {
                application_host.shutdown(
                    squiflow::app::ShutdownReason::NormalExit);
                application.quit();
            });
    }

    return application.exec();
}

#endif
