#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "shell/dashboard_bridge.hpp"
#include "shell/dashboard_model_qt.hpp"

#include <QObject>

namespace squiflow::shell {

class DashboardBridgeQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QObject* model READ model CONSTANT)

  public:
    explicit DashboardBridgeQt(DashboardBridge& bridge, QObject* parent = nullptr);

    QObject* model() noexcept { return &model_; }
    Q_INVOKABLE bool refresh();
    void publish(std::uint64_t generation, std::uint64_t session_generation,
                 app::dashboard::DashboardSnapshot snapshot);
    void fail(std::uint64_t generation, std::uint64_t session_generation,
              app::DomainError error);

  signals:
    void refreshRequested(qulonglong generation, qulonglong sessionGeneration);

  private:
    DashboardBridge& bridge_;
    DashboardModelQt model_;
    std::uint64_t session_generation_{1};
};

}  // namespace squiflow::shell

#endif
