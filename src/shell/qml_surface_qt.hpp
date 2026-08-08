#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "app/contracts/request_context.hpp"
#include "shell/screen_registry.hpp"
#include "shell/surface_lifecycle.hpp"
#include "shell/window_state.hpp"

#include <QObject>
#include <QPointer>

#include <memory>
#include <optional>

class QQmlApplicationEngine;
class QQuickImageProvider;
class QWindow;

namespace squiflow::app { class AuthenticatedWorkspace; }

namespace squiflow::shell {

class NavigationBridgeQt;
class NativeWindowBridgeQt;
class NavigationController;
class NavigationModelQt;
class ScreenRegistry;
class ShellStateQt;

class QmlSurfaceQt final : public QObject {
    Q_OBJECT

  public:
    explicit QmlSurfaceQt(SurfaceLifecycle::ShutdownRequest request,
                          WindowStateStore* window_state_store = nullptr,
                          QObject* parent = nullptr);
    ~QmlSurfaceQt() override;
    app::StepResult startShell();
    app::StepResult startWindow();
    Q_INVOKABLE void requestShutdown();
    void publishNavigationAccess(NavigationAccess access);
    void attachWorkspace(app::AuthenticatedWorkspace& workspace, app::TenantId tenant,
                         protocol::Activation activation);
    void detachWorkspace() noexcept;
    bool installImageProvider(std::unique_ptr<QQuickImageProvider> provider);
    void stopWindow() noexcept;
    void stopShell() noexcept;

  private:
    SurfaceLifecycle lifecycle_;
    WindowStateStore* window_state_store_{nullptr};
    std::unique_ptr<ScreenRegistry> navigation_registry_{};
    std::unique_ptr<NavigationController> navigation_controller_{};
    std::unique_ptr<NavigationModelQt> navigation_model_{};
    std::unique_ptr<NavigationBridgeQt> navigation_bridge_{};
    std::unique_ptr<NativeWindowBridgeQt> native_window_bridge_{};
    std::unique_ptr<ShellStateQt> shell_state_{};
    std::optional<NavigationAccess> pending_navigation_access_{};
    app::AuthenticatedWorkspace* pending_workspace_{nullptr};
    std::optional<app::TenantId> pending_tenant_{};
    protocol::Activation pending_activation_{};
    std::unique_ptr<QQuickImageProvider> pending_image_provider_{};
    std::unique_ptr<QQmlApplicationEngine> engine_{};
    QPointer<QWindow> root_{};
    bool creation_failed_{false};
};

}  // namespace squiflow::shell

#endif
