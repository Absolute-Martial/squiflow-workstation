#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/dashboard_bridge.hpp"

#include <QObject>
#include <QString>
#include <QVariantList>

namespace squiflow::shell {

class DashboardModelQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList metrics READ metrics NOTIFY changed)
    Q_PROPERTY(QVariantList activity READ activity NOTIFY changed)
    Q_PROPERTY(QVariantList quickActions READ quickActions NOTIFY changed)
    Q_PROPERTY(bool loading READ loading NOTIFY changed)
    Q_PROPERTY(bool offline READ offline NOTIFY changed)
    Q_PROPERTY(bool empty READ empty NOTIFY changed)
    Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)

  public:
    explicit DashboardModelQt(QObject* parent = nullptr) : QObject(parent) {}

    QVariantList metrics() const { return metrics_; }
    QVariantList activity() const { return activity_; }
    QVariantList quickActions() const { return quick_actions_; }
    bool loading() const noexcept { return loading_; }
    bool offline() const noexcept { return offline_; }
    bool empty() const noexcept;
    QString errorKey() const { return error_key_; }

    void publish(const DashboardBridge& bridge);

  signals:
    void changed();

  private:
    QVariantList metrics_{};
    QVariantList activity_{};
    QVariantList quick_actions_{};
    bool loading_{false};
    bool offline_{false};
    QString error_key_{};
};

}  // namespace squiflow::shell

#endif
