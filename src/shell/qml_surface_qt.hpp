#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/screen_registry.hpp"
#include "shell/surface_lifecycle.hpp"

#include <QObject>
#include <QPointer>

#include <memory>
#include <optional>

class QQmlApplicationEngine;
class QWindow;

namespace squiflow::shell {

class NavigationBridgeQt;
class NavigationController;
class NavigationModelQt;
class ScreenRegistry;

class QmlSurfaceQt final : public QObject {
    Q_OBJECT

  public:
    explicit QmlSurfaceQt(SurfaceLifecycle::ShutdownRequest request,
                          QObject* parent = nullptr);
    ~QmlSurfaceQt() override;
    app::StepResult startShell();
    app::StepResult startWindow();
    Q_INVOKABLE void requestShutdown();
    void publishNavigationAccess(NavigationAccess access);
    void stopWindow() noexcept;
    void stopShell() noexcept;

  private:
    SurfaceLifecycle lifecycle_;
    std::unique_ptr<ScreenRegistry> navigation_registry_{};
    std::unique_ptr<NavigationController> navigation_controller_{};
    std::unique_ptr<NavigationModelQt> navigation_model_{};
    std::unique_ptr<NavigationBridgeQt> navigation_bridge_{};
    std::optional<NavigationAccess> pending_navigation_access_{};
    std::unique_ptr<QQmlApplicationEngine> engine_{};
    QPointer<QWindow> root_{};
    bool creation_failed_{false};
};

}  // namespace squiflow::shell

#endif
