#include "shell/dashboard_model_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include <QVariantMap>

#include <type_traits>
#include <variant>

namespace squiflow::shell {
namespace {

QString text(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

}  // namespace

bool DashboardModelQt::empty() const noexcept {
    return !loading_ && error_key_.isEmpty() && metrics_.isEmpty() &&
           activity_.isEmpty() && quick_actions_.isEmpty();
}

void DashboardModelQt::publish(const DashboardBridge& bridge) {
    metrics_.clear();
    activity_.clear();
    quick_actions_.clear();
    error_key_.clear();
    loading_ = state_kind(bridge.state()) == ViewStateKind::Loading;
    offline_ = state_kind(bridge.state()) == ViewStateKind::Offline;
    if (const auto* failed = std::get_if<FailedState>(&bridge.state())) {
        error_key_ = text(failed->message_key);
    }
    if (bridge.snapshot()) {
        for (const auto& metric : bridge.snapshot()->metrics) {
            QVariantMap row{{QStringLiteral("id"), text(metric.id)},
                            {QStringLiteral("labelKey"), text(metric.label_key)},
                            {QStringLiteral("valueText"), text(metric.value_text)},
                            {QStringLiteral("detailKey"), text(metric.detail_key)},
                            {QStringLiteral("routeId"), text(metric.route_id)},
                            {QStringLiteral("currencyCode"), text(metric.currency_code)},
                            {QStringLiteral("hasExactMinorUnits"),
                             metric.exact_minor_units.has_value()}};
            if (metric.exact_minor_units) {
                row.insert(QStringLiteral("exactMinorUnits"),
                           QVariant::fromValue<qlonglong>(*metric.exact_minor_units));
            }
            metrics_.push_back(row);
        }
        for (const auto& item : bridge.snapshot()->activity) {
            activity_.push_back(QVariantMap{
                {QStringLiteral("id"), text(item.id)},
                {QStringLiteral("titleKey"), text(item.title_key)},
                {QStringLiteral("detailText"), text(item.detail_text)},
                {QStringLiteral("routeId"), text(item.route_id)},
                {QStringLiteral("recordId"), text(item.record_id)},
                {QStringLiteral("occurredAtMs"),
                 QVariant::fromValue<qlonglong>(item.occurred_at_ms)}});
        }
        for (const auto& action : bridge.snapshot()->quick_actions) {
            quick_actions_.push_back(QVariantMap{
                {QStringLiteral("id"), text(action.id)},
                {QStringLiteral("labelKey"), text(action.label_key)},
                {QStringLiteral("routeId"), text(action.route_id)}});
        }
    }
    emit changed();
}

}  // namespace squiflow::shell

#endif
