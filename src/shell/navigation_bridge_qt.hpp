#pragma once

#if defined(SQUIFLOW_WITH_QT)

#include "app/contracts/request_context.hpp"
#include "app/primary/primary_query.hpp"
#include "shell/navigation_controller.hpp"
#include "shell/navigation_model_qt.hpp"

#include <QObject>
#include <QString>
#include <QUrl>

#include <cstdint>
#include <memory>
#include <optional>

namespace squiflow::app { class AuthenticatedWorkspace; }

namespace squiflow::shell {

class DashboardBridgeQt;
class ListScreenBridgeQt;
class RecordScreenBridgeQt;

class NavigationBridgeQt final : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString currentRoute READ currentRoute NOTIFY currentRouteChanged)
    Q_PROPERTY(QUrl currentComponentUrl READ currentComponentUrl NOTIFY currentRouteChanged)
    Q_PROPERTY(bool hasAccessibleModules READ hasAccessibleModules NOTIFY accessibleModulesChanged)
    Q_PROPERTY(QString lastErrorKey READ lastErrorKey NOTIFY lastErrorChanged)
    Q_PROPERTY(QObject* currentListBridge READ currentListBridge NOTIFY currentRouteChanged)
    Q_PROPERTY(QObject* currentDashboardBridge READ currentDashboardBridge NOTIFY currentRouteChanged)
    Q_PROPERTY(QObject* currentRecordBridge READ currentRecordBridge NOTIFY currentRouteChanged)

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
    QObject* currentRecordBridge() const noexcept;

    Q_INVOKABLE bool selectRoute(const QString& stable_id);
    Q_INVOKABLE bool goBack();
    Q_INVOKABLE bool goForward();
    void publishAccess(NavigationAccess access);
    void attachWorkspace(app::AuthenticatedWorkspace& workspace, app::TenantId tenant,
                         protocol::Activation activation);
    void detachWorkspace() noexcept;
    void shutdown() noexcept;

  signals:
    void currentRouteChanged();
    void accessibleModulesChanged();
    void lastErrorChanged();

  private:
    void applyAccessOnGui(NavigationAccess access);
    void synchronize();
    std::optional<app::RequestContext> requestContext();
    void fulfillList(app::primary::PageKind kind, qulonglong generation,
                     qulonglong offset, qulonglong limit, const QString& sort_field,
                     bool descending, const QString& filter_field,
                     const QString& filter_text);
    bool finish(const app::Result<void, NavigationError>& result);

    NavigationController& controller_;
    NavigationModelQt& model_;
    std::unique_ptr<ListScreenBridgeQt> current_list_bridge_{};
    std::unique_ptr<DashboardBridgeQt> current_dashboard_bridge_{};
    std::unique_ptr<RecordScreenBridgeQt> current_record_bridge_{};
    app::AuthenticatedWorkspace* workspace_{nullptr};
    std::optional<app::TenantId> tenant_{};
    protocol::Activation activation_{};
    std::uint64_t request_sequence_{0};
    QString last_error_key_{};
};

}  // namespace squiflow::shell

#endif
