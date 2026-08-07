#include "shell/navigation_bridge_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include "shell/dashboard_bridge_qt.hpp"
#include "shell/list_screen_bridge_qt.hpp"
#include "shell/navigation_manifest.hpp"

namespace squiflow::shell {

NavigationBridgeQt::NavigationBridgeQt(NavigationController& controller,
                                       NavigationModelQt& model, QObject* parent)
    : QObject(parent), controller_(controller), model_(model) {}

NavigationBridgeQt::~NavigationBridgeQt() = default;

QString NavigationBridgeQt::currentRoute() const {
    return QString::fromUtf8(controller_.current_route().data(),
                             static_cast<qsizetype>(controller_.current_route().size()));
}

QUrl NavigationBridgeQt::currentComponentUrl() const {
    const std::string_view route = controller_.current_route();
    for (const NavigationRow& row : controller_.rows()) {
        if (row.stable_id == route) {
            return QUrl(QString::fromStdString(row.component_url));
        }
    }
    return {};
}

bool NavigationBridgeQt::hasAccessibleModules() const noexcept {
    return !controller_.rows().empty();
}

QObject* NavigationBridgeQt::currentListBridge() const noexcept {
    return current_list_bridge_.get();
}

QObject* NavigationBridgeQt::currentDashboardBridge() const noexcept {
    return current_dashboard_bridge_.get();
}

bool NavigationBridgeQt::finish(const app::Result<void, NavigationError>& result) {
    const QString next_error = result.has_value()
        ? QString{}
        : QString::fromStdString(result.error().message_key);
    if (next_error != last_error_key_) {
        last_error_key_ = next_error;
        emit lastErrorChanged();
    }
    if (!result) {
        return false;
    }
    synchronize();
    return true;
}

void NavigationBridgeQt::synchronize() {
    current_list_bridge_.reset();
    current_dashboard_bridge_.reset();
    if (auto* dashboard = dynamic_cast<DashboardPresentationBridge*>(
            controller_.active_bridge())) {
        current_dashboard_bridge_ =
            std::make_unique<DashboardBridgeQt>(dashboard->dashboard(), this);
    } else if (auto* route = dynamic_cast<RoutePresentationBridge*>(
                   controller_.active_bridge())) {
        current_list_bridge_ =
            std::make_unique<ListScreenBridgeQt>(route->list(), this);
    }
    model_.refreshFromController();
    emit currentRouteChanged();
    emit accessibleModulesChanged();
}

bool NavigationBridgeQt::selectRoute(const QString& stable_id) {
    Q_ASSERT(thread() == QThread::currentThread());
    return finish(controller_.select(stable_id.toStdString()));
}

bool NavigationBridgeQt::goBack() {
    Q_ASSERT(thread() == QThread::currentThread());
    return finish(controller_.go_back());
}

bool NavigationBridgeQt::goForward() {
    Q_ASSERT(thread() == QThread::currentThread());
    return finish(controller_.go_forward());
}

void NavigationBridgeQt::publishAccess(NavigationAccess access) {
    if (thread() != QThread::currentThread()) {
        QPointer<NavigationBridgeQt> self(this);
        QMetaObject::invokeMethod(
            this,
            [self, access = std::move(access)]() mutable {
                if (self) {
                    self->applyAccessOnGui(std::move(access));
                }
            },
            Qt::QueuedConnection);
        return;
    }
    applyAccessOnGui(std::move(access));
}

void NavigationBridgeQt::applyAccessOnGui(NavigationAccess access) {
    Q_ASSERT(thread() == QThread::currentThread());
    (void)finish(controller_.apply_access(std::move(access)));
}

void NavigationBridgeQt::shutdown() noexcept {
    if (thread() != QThread::currentThread()) {
        return;
    }
    controller_.shutdown();
    last_error_key_.clear();
    synchronize();
    emit lastErrorChanged();
}

}  // namespace squiflow::shell

#endif
