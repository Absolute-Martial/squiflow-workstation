#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/navigation_controller.hpp"
#include "shell/navigation_model_qt.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

namespace squiflow::shell {

class NavigationBridgeQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentRoute READ currentRoute NOTIFY currentRouteChanged)
    Q_PROPERTY(QUrl currentComponentUrl READ currentComponentUrl NOTIFY currentRouteChanged)
    Q_PROPERTY(bool hasAccessibleModules READ hasAccessibleModules NOTIFY accessibleModulesChanged)
    Q_PROPERTY(QString lastErrorKey READ lastErrorKey NOTIFY lastErrorChanged)

  public:
    NavigationBridgeQt(NavigationController& controller, NavigationModelQt& model,
                       QObject* parent = nullptr);

    QString currentRoute() const;
    QUrl currentComponentUrl() const;
    bool hasAccessibleModules() const noexcept;
    QString lastErrorKey() const { return last_error_key_; }

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
    QString last_error_key_{};
};

}  // namespace squiflow::shell

#endif
