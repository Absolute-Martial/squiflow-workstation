#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/navigation_controller.hpp"
#include "shell/navigation_model_qt.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

#include <memory>

namespace squiflow::shell {

class DashboardBridgeQt;
class ListScreenBridgeQt;

class NavigationBridgeQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentRoute READ currentRoute NOTIFY currentRouteChanged)
    Q_PROPERTY(QUrl currentComponentUrl READ currentComponentUrl NOTIFY currentRouteChanged)
    Q_PROPERTY(bool hasAccessibleModules READ hasAccessibleModules NOTIFY accessibleModulesChanged)
    Q_PROPERTY(QString lastErrorKey READ lastErrorKey NOTIFY lastErrorChanged)
    Q_PROPERTY(QObject* currentListBridge READ currentListBridge NOTIFY currentRouteChanged)
    Q_PROPERTY(QObject* currentDashboardBridge READ currentDashboardBridge NOTIFY currentRouteChanged)

  public:
    NavigationBridgeQt(NavigationController& controller, NavigationModelQt& model,
                       QObject* parent = nullptr);
    ~NavigationBridgeQt() override;

    QString currentRoute() const;
    QUrl currentComponentUrl() const;
    bool hasAccessibleModules() const noexcept;
    QString lastErrorKey() const { return last_error_key_; }
    QObject* currentListBridge() const noexcept;
    QObject* currentDashboardBridge() const noexcept;

    Q_INVOKABLE bool selectRoute(const QString& stable_id);
    Q_INVOKABLE bool goBack();
    Q_INVOKABLE bool goForward();
    void publishAccess(NavigationAccess access);
    void shutdown() noexcept;

  signals:
    void currentRouteChanged();
    void accessibleModulesChanged();
    void lastErrorChanged();

  private:
    void applyAccessOnGui(NavigationAccess access);
    void synchronize();
    bool finish(const app::Result<void, NavigationError>& result);

    NavigationController& controller_;
    NavigationModelQt& model_;
    std::unique_ptr<ListScreenBridgeQt> current_list_bridge_{};
    std::unique_ptr<DashboardBridgeQt> current_dashboard_bridge_{};
    QString last_error_key_{};
};

}  // namespace squiflow::shell

#endif
