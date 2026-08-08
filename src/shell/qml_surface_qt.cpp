#include "shell/qml_surface_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include "app/workspace_runtime.hpp"
#include "shell/navigation_bridge_qt.hpp"
#include "shell/native_window_bridge_qt.hpp"
#include "shell/navigation_controller.hpp"
#include "shell/navigation_manifest.hpp"
#include "shell/navigation_model_qt.hpp"
#include "shell/shell_state_qt.hpp"

#include <QCoreApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
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

QmlSurfaceQt::QmlSurfaceQt(SurfaceLifecycle::ShutdownRequest request,
                          WindowStateStore* window_state_store,
                          QObject* parent)
    : QObject(parent), lifecycle_(std::move(request)),
      window_state_store_(window_state_store) {}

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
        shell_state_ = std::make_unique<ShellStateQt>(this);
        native_window_bridge_ = std::make_unique<NativeWindowBridgeQt>(this);

        engine_ = std::make_unique<QQmlApplicationEngine>();
        if (pending_image_provider_) {
            engine_->addImageProvider(QStringLiteral("squiflow-files"), pending_image_provider_.release());
        }
#if defined(SQUIFLOW_WITH_UI_FLUENT)
        // A staged release carries qualified QML modules beside the executable;
        // source-tree paths remain the development/CI fallback.
        engine_->addImportPath(QCoreApplication::applicationDirPath() +
                               QStringLiteral("/qml"));
        // Layer 2/3 of the hybrid Fluent UI sourcing strategy (see
        // docs/plan/phase-7-fluent-ui-sourcing.md): pure-QML component
        // modules vendored under external/ui-fluent/. Layer 1 (Qt's native
        // FluentWinUI3 style) needs no import path; it is picked up from
        // src/ui/qtquickcontrols2.conf automatically.
        engine_->addImportPath(QStringLiteral(SQUIFLOW_UI_FLUENTCONTROLS_IMPORT_PATH));
        engine_->addImportPath(QStringLiteral(SQUIFLOW_UI_RINUI_IMPORT_PATH));
#endif
        engine_->rootContext()->setContextProperty("applicationSurface", this);
        engine_->rootContext()->setContextProperty("navigationModel",
                                                   navigation_model_.get());
        engine_->rootContext()->setContextProperty("navigationBridge",
                                                   navigation_bridge_.get());
        engine_->rootContext()->setContextProperty("shellState", shell_state_.get());
        engine_->rootContext()->setContextProperty("nativeWindowBridge",
                                                   native_window_bridge_.get());
        connect(engine_.get(), &QQmlApplicationEngine::objectCreationFailed,
                this, [this] { creation_failed_ = true; });
        if (pending_workspace_ != nullptr && pending_tenant_) {
            navigation_bridge_->attachWorkspace(*pending_workspace_, *pending_tenant_,
                                                pending_activation_);
        }
        if (pending_navigation_access_) {
            navigation_bridge_->publishAccess(
                std::move(*pending_navigation_access_));
            pending_navigation_access_.reset();
        }
    } catch (const std::exception& failure) {
        engine_.reset();
        navigation_bridge_.reset();
        shell_state_.reset();
        native_window_bridge_.reset();
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
    if (window_state_store_) {
        int screen_width = 0;
        int screen_height = 0;
        if (const auto* screen = root_->screen()) {
            const auto available = screen->availableGeometry();
            screen_width = available.width();
            screen_height = available.height();
        }
        const auto geometry = resolve_window_geometry(*window_state_store_,
                                                       screen_width,
                                                       screen_height);
        if (geometry.maximized) {
            root_->showMaximized();
        } else {
            root_->setGeometry(geometry.x, geometry.y, geometry.width,
                               geometry.height);
        }
    }
    root_->show();
    return lifecycle_.start_window(true);
}

void QmlSurfaceQt::requestShutdown() {
    lifecycle_.request_shutdown();
}

bool QmlSurfaceQt::installImageProvider(std::unique_ptr<QQuickImageProvider> provider) {
    if (engine_ || !provider) return false;
    pending_image_provider_ = std::move(provider);
    return true;
}

void QmlSurfaceQt::attachWorkspace(
    app::AuthenticatedWorkspace& workspace, app::TenantId tenant,
    protocol::Activation activation) {
    pending_workspace_ = &workspace;
    pending_tenant_ = tenant;
    pending_activation_ = std::move(activation);
    if (navigation_bridge_) {
        navigation_bridge_->attachWorkspace(workspace, tenant, pending_activation_);
    }
}

void QmlSurfaceQt::detachWorkspace() noexcept {
    if (navigation_bridge_) navigation_bridge_->detachWorkspace();
    pending_workspace_ = nullptr;
    pending_tenant_.reset();
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
        if (window_state_store_) {
            WindowGeometry geometry;
            geometry.maximized = root_->windowState() & Qt::WindowMaximized;
            if (!geometry.maximized) {
                const auto bounds = root_->geometry();
                geometry.x = bounds.x();
                geometry.y = bounds.y();
                geometry.width = bounds.width();
                geometry.height = bounds.height();
            }
            window_state_store_->save(geometry);
        }
        root_->hide();
        delete root_.data();
        root_ = nullptr;
    }
    lifecycle_.stop_window();
}

void QmlSurfaceQt::stopShell() noexcept {
    detachWorkspace();
    engine_.reset();
    pending_image_provider_.reset();
    if (navigation_bridge_) {
        navigation_bridge_->shutdown();
    }
    navigation_bridge_.reset();
    shell_state_.reset();
    native_window_bridge_.reset();
    navigation_model_.reset();
    navigation_controller_.reset();
    navigation_registry_.reset();
    pending_navigation_access_.reset();
    lifecycle_.stop_shell();
}

}  // namespace squiflow::shell

#endif
