#include "platform/network_state_qt.hpp"
#include "platform/network_monitor.hpp"

#include <QMetaObject>
#include <QNetworkInformation>
#include <QString>

#include <utility>
#include <vector>

namespace squiflow::platform {
namespace {
NetworkReachability reachability(QNetworkInformation::Reachability value) noexcept {
    using Q = QNetworkInformation::Reachability;
    switch (value) {
    case Q::Unknown: return NetworkReachability::Unknown;
    case Q::Disconnected: return NetworkReachability::Offline;
    case Q::Local: return NetworkReachability::LocalOnly;
    case Q::Site: return NetworkReachability::SiteOnly;
    case Q::Online: return NetworkReachability::Online;
    }
    return NetworkReachability::Unknown;
}
NetworkTransport transport(QNetworkInformation::TransportMedium value) noexcept {
    using Q = QNetworkInformation::TransportMedium;
    switch (value) {
    case Q::Unknown: return NetworkTransport::Unknown;
    case Q::Ethernet: return NetworkTransport::Ethernet;
    case Q::Cellular: return NetworkTransport::Cellular;
    case Q::WiFi: return NetworkTransport::Wifi;
    case Q::Bluetooth: return NetworkTransport::Bluetooth;
    }
    return NetworkTransport::Unknown;
}
}

class QtNetworkStateAdapter::Impl {
public:
    explicit Impl(NetworkMonitor& monitor) : monitor_(monitor) {
        loaded_ = QNetworkInformation::loadDefaultBackend();
        information_ = QNetworkInformation::instance();
        if (!loaded_ || information_ == nullptr) { monitor_.publish(NetworkSnapshot{}); return; }
        backend_name_ = information_->backendName().toStdString();
        connections_.push_back(QObject::connect(information_, &QNetworkInformation::reachabilityChanged, information_, [this] { refresh(); }));
        connections_.push_back(QObject::connect(information_, &QNetworkInformation::transportMediumChanged, information_, [this] { refresh(); }));
        connections_.push_back(QObject::connect(information_, &QNetworkInformation::isMeteredChanged, information_, [this] { refresh(); }));
        connections_.push_back(QObject::connect(information_, &QNetworkInformation::isBehindCaptivePortalChanged, information_, [this] { refresh(); }));
        refresh();
    }
    ~Impl() { for (const auto& connection : connections_) QObject::disconnect(connection); }
    bool loaded() const noexcept { return loaded_ && information_ != nullptr; }
    const std::string& name() const noexcept { return backend_name_; }
private:
    void refresh() noexcept {
        if (information_ == nullptr) return;
        using F = QNetworkInformation::Feature;
        NetworkSnapshot state;
        state.reachability_supported = information_->supports(F::Reachability);
        state.transport_supported = information_->supports(F::TransportMedium);
        state.captive_portal_supported = information_->supports(F::CaptivePortal);
        state.metered_supported = information_->supports(F::Metered);
        if (state.reachability_supported) state.reachability = reachability(information_->reachability());
        if (state.transport_supported) state.transport = transport(information_->transportMedium());
        if (state.captive_portal_supported) state.captive_portal = information_->isBehindCaptivePortal();
        if (state.metered_supported) state.metered = information_->isMetered();
        monitor_.publish(state);
    }
    NetworkMonitor& monitor_;
    QNetworkInformation* information_{nullptr};
    bool loaded_{false};
    std::string backend_name_{};
    std::vector<QMetaObject::Connection> connections_{};
};

QtNetworkStateAdapter::QtNetworkStateAdapter(NetworkMonitor& monitor) : impl_(std::make_unique<Impl>(monitor)) {}
QtNetworkStateAdapter::~QtNetworkStateAdapter() = default;
bool QtNetworkStateAdapter::backend_loaded() const noexcept { return impl_->loaded(); }
const std::string& QtNetworkStateAdapter::backend_name() const noexcept { return impl_->name(); }

} // namespace squiflow::platform
