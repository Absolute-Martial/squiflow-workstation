#include "shell/qml_surface_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include "shell/navigation_bridge_qt.hpp"
#include "shell/navigation_controller.hpp"
#include "shell/navigation_manifest.hpp"
#include "shell/navigation_model_qt.hpp"

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QWindow>

#include <utility>
#include <vector>

namespace squiflow::shell {
namespace {

std::vector<protocol::ModuleId> compiled_modules() {
    std::vector<protocol::ModuleId> result;
    result.reserve(protocol::kModuleCount);
    for (std::size_t index = 0; index < protocol::kModuleCount; ++index) {
        result.push_back(static_cast<protocol::ModuleId>(index));
    }
    return result;
}

}  // namespace

QmlSurfaceQt::QmlSurfaceQt(SurfaceLifecycle::ShutdownRequest request, QObject* parent)
    : QObject(parent), lifecycle_(std::move(request)) {}

QmlSurfaceQt::~QmlSurfaceQt() = default;

app::StepResult QmlSurfaceQt::startShell() {
    if (engine_) {
        return {app::StepDisposition::Failed, "QML engine already exists"};
    }
    try {
        navigation_registry_ =
            std::make_unique<ScreenRegistry>(make_navigation_manifest());
        require_navigation_complete(*navigation_registry_, compiled_modules());
        navigation_controller_ =
            std::make_unique<NavigationController>(*navigation_registry_);
        navigation_model_ =
            std::make_unique<NavigationModelQt>(*navigation_controller_, this);
        navigation_bridge_ = std::make_unique<NavigationBridgeQt>(
            *navigation_controller_, *navigation_model_, this);

        engine_ = std::make_unique<QQmlApplicationEngine>();
        engine_->rootContext()->setContextProperty("applicationSurface", this);
        engine_->rootContext()->setContextProperty("navigationModel",
                                                   navigation_model_.get());
        engine_->rootContext()->setContextProperty("navigationBridge",
                                                   navigation_bridge_.get());
        connect(engine_.get(), &QQmlApplicationEngine::objectCreationFailed,
                this, [this] { creation_failed_ = true; });
        if (pending_navigation_access_) {
            navigation_bridge_->publishAccess(
                std::move(*pending_navigation_access_));
            pending_navigation_access_.reset();
        }
    } catch (const std::exception& failure) {
        engine_.reset();
        navigation_bridge_.reset();
        navigation_model_.reset();
        navigation_controller_.reset();
        navigation_registry_.reset();
        return lifecycle_.start_shell(false, failure.what());
    }
    return lifecycle_.start_shell(true);
}

app::StepResult QmlSurfaceQt::startWindow() {
    if (!engine_) {
        return lifecycle_.start_window(false, "QML engine missing");
    }
    creation_failed_ = false;
    engine_->loadFromModule("SquiFlow", "Main");
    if (creation_failed_ || engine_->rootObjects().isEmpty()) {
        return lifecycle_.start_window(false, "QML root creation failed");
    }
    root_ = qobject_cast<QWindow*>(engine_->rootObjects().constFirst());
    if (!root_) {
        return lifecycle_.start_window(false, "QML root is not a window");
    }
    root_->show();
    return lifecycle_.start_window(true);
}

void QmlSurfaceQt::requestShutdown() {
    lifecycle_.request_shutdown();
}

void QmlSurfaceQt::publishNavigationAccess(NavigationAccess access) {
    if (navigation_bridge_) {
        navigation_bridge_->publishAccess(std::move(access));
    } else {
        pending_navigation_access_ = std::move(access);
    }
}

void QmlSurfaceQt::stopWindow() noexcept {
    if (root_) {
        root_->hide();
        delete root_.data();
        root_ = nullptr;
    }
    lifecycle_.stop_window();
}

void QmlSurfaceQt::stopShell() noexcept {
    engine_.reset();
    if (navigation_bridge_) {
        navigation_bridge_->shutdown();
    }
    navigation_bridge_.reset();
    navigation_model_.reset();
    navigation_controller_.reset();
    navigation_registry_.reset();
    pending_navigation_access_.reset();
    lifecycle_.stop_shell();
}

}  // namespace squiflow::shell

#endif
