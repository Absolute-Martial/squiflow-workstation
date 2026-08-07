#if defined(SQUIFLOW_WITH_QT)

#include "app/application.hpp"
#include "engine/identity/rights_set.hpp"
#include "shell/qml_surface_qt.hpp"
#include "shell/screen_registry.hpp"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QString>
#include <QTimer>

#include <squiflow/protocol/module_graph.hpp>

#include <algorithm>
#include <cstddef>
#include <vector>

int squiflow_workstation_main_qt(int argc, char** argv) {
    QGuiApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("SquiFlow"));
    QCoreApplication::setOrganizationName(QStringLiteral("SquiFlow"));

    squiflow::shell::QmlSurfaceQt* active_surface = nullptr;
    squiflow::shell::QmlSurfaceQt surface(
        [&](squiflow::app::ShutdownReason) {
            if (active_surface != nullptr) {
                active_surface->stopWindow();
                active_surface->stopShell();
            }
            application.quit();
        });
    active_surface = &surface;

    if (surface.startShell().disposition == squiflow::app::StepDisposition::Failed) {
        return 2;
    }
    // Until the identity/session startup step publishes an authenticated
    // access snapshot, expose only routes with no required right. Never grant
    // production rights merely to make a UI smoke test convenient.
    squiflow::engine::RightsSet rights;
    std::vector<squiflow::protocol::ModuleId> modules;
    modules.reserve(squiflow::protocol::kModuleCount);
    for (std::size_t index = 0; index < squiflow::protocol::kModuleCount; ++index) {
        modules.push_back(static_cast<squiflow::protocol::ModuleId>(index));
    }
    const auto activation = squiflow::protocol::resolve_activation({});
    if (!activation.ok) {
        surface.stopShell();
        return 3;
    }
    surface.publishNavigationAccess(squiflow::shell::make_navigation_access(
        activation.activation, rights, modules, 1, 1));
    if (surface.startWindow().disposition == squiflow::app::StepDisposition::Failed) {
        surface.stopShell();
        return 4;
    }

    const bool smoke_test = std::any_of(
        argv + 1, argv + argc, [](const char* value) {
            return QString::fromUtf8(value) == QStringLiteral("--smoke-test");
        });
    if (smoke_test) {
        QTimer::singleShot(250, &surface, [&surface] { surface.requestShutdown(); });
    }
    return application.exec();
}

#endif
