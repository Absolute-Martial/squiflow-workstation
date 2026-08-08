#include "shell/dashboard_bridge_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include <QThread>

#include <utility>

namespace squiflow::shell {

DashboardBridgeQt::DashboardBridgeQt(DashboardBridge& bridge, QObject* parent)
    : QObject(parent), bridge_(bridge), model_(this) {
    model_.publish(bridge_);
}

bool DashboardBridgeQt::refresh() {
    Q_ASSERT(thread() == QThread::currentThread());
    const auto request = bridge_.begin_refresh(session_generation_);
    if (!request) {
        model_.publish(bridge_);
        return false;
    }
    const auto generation = request.value().generation;
    const auto session_generation = request.value().session_generation;
    if (receivers(SIGNAL(refreshRequested(qulonglong,qulonglong))) == 0) {
        (void)bridge_.fail(generation, session_generation,
                           {app::DomainErrorCode::InvalidContext,
                            "dashboard.error.provider_unavailable", std::nullopt});
        model_.publish(bridge_);
        return false;
    }
    emit refreshRequested(static_cast<qulonglong>(generation),
                          static_cast<qulonglong>(session_generation));
    model_.publish(bridge_);
    return true;
}

void DashboardBridgeQt::fail(
    std::uint64_t generation, std::uint64_t session_generation,
    app::DomainError error) {
    Q_ASSERT(thread() == QThread::currentThread());
    (void)bridge_.fail(generation, session_generation, std::move(error));
    model_.publish(bridge_);
}

void DashboardBridgeQt::publish(
    std::uint64_t generation, std::uint64_t session_generation,
    app::dashboard::DashboardSnapshot snapshot) {
    Q_ASSERT(thread() == QThread::currentThread());
    (void)bridge_.apply(generation, session_generation, std::move(snapshot));
    model_.publish(bridge_);
}

}  // namespace squiflow::shell

#endif
