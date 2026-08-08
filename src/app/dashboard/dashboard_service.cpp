#include "app/dashboard/dashboard_service.hpp"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace squiflow::app::dashboard {
namespace {

constexpr std::size_t kMaximumIdBytes = 64;
constexpr std::size_t kMaximumKeyBytes = 128;
constexpr std::size_t kMaximumValueBytes = 160;
constexpr std::size_t kMaximumDetailBytes = 512;

bool safe_identifier(std::string_view value, std::size_t maximum) noexcept {
    if (value.empty() || value.size() > maximum || value.front() == '.' ||
        value.back() == '.') {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char raw) {
        const auto character = static_cast<unsigned char>(raw);
        return std::isalnum(character) != 0 || character == static_cast<unsigned char>('.') ||
               character == static_cast<unsigned char>('_') ||
               character == static_cast<unsigned char>('-');
    });
}

DomainError invalid(std::string field) {
    return {DomainErrorCode::ValidationFailed,
            "dashboard.error.invalid_snapshot", std::move(field)};
}

bool valid_scope(protocol::ModuleId owner,
                 const std::optional<protocol::RightId>& right) noexcept {
    return protocol::is_valid(owner) &&
           (!right || (protocol::is_valid(*right) &&
                       protocol::right_module(*right) == owner));
}

bool allowed(const RequestContext& context, const protocol::Activation& activation,
             protocol::ModuleId owner,
             const std::optional<protocol::RightId>& right) noexcept {
    return activation.is_active(owner) &&
           (!right || context.permissions().has(*right));
}

template <class T>
bool unique_ids(const std::vector<T>& values) {
    std::unordered_set<std::string> ids;
    ids.reserve(values.size());
    for (const auto& value : values) {
        if (!ids.insert(value.id).second) {
            return false;
        }
    }
    return true;
}

bool valid_metric(const DashboardMetric& metric) noexcept {
    const bool money_consistent = metric.exact_minor_units.has_value()
        ? safe_identifier(metric.currency_code, 8)
        : metric.currency_code.empty();
    return valid_scope(metric.owner, metric.required_right) &&
           safe_identifier(metric.id, kMaximumIdBytes) &&
           safe_identifier(metric.label_key, kMaximumKeyBytes) &&
           !metric.value_text.empty() && metric.value_text.size() <= kMaximumValueBytes &&
           (metric.detail_key.empty() ||
            safe_identifier(metric.detail_key, kMaximumKeyBytes)) &&
           (metric.route_id.empty() ||
            safe_identifier(metric.route_id, kMaximumIdBytes)) && money_consistent;
}

bool valid_activity(const DashboardActivity& activity) noexcept {
    return valid_scope(activity.owner, activity.required_right) &&
           safe_identifier(activity.id, kMaximumIdBytes) &&
           safe_identifier(activity.title_key, kMaximumKeyBytes) &&
           activity.detail_text.size() <= kMaximumDetailBytes &&
           (activity.route_id.empty() ||
            safe_identifier(activity.route_id, kMaximumIdBytes)) &&
           (activity.record_id.empty() || activity.record_id.size() == 32) &&
           activity.occurred_at_ms >= 0;
}

bool valid_action(const DashboardQuickAction& action) noexcept {
    return protocol::is_valid(action.owner) &&
           protocol::is_valid(action.required_right) &&
           protocol::right_module(action.required_right) == action.owner &&
           safe_identifier(action.id, kMaximumIdBytes) &&
           safe_identifier(action.label_key, kMaximumKeyBytes) &&
           safe_identifier(action.route_id, kMaximumIdBytes);
}

}  // namespace

Result<DashboardSnapshot, DomainError> DashboardService::refresh(
    const RequestContext& context, const protocol::Activation& activation) const {
    auto loaded = query_.load(context);
    if (!loaded) {
        return Result<DashboardSnapshot, DomainError>::failure(loaded.error());
    }
    DashboardSnapshot snapshot = std::move(loaded).value();
    if (snapshot.metrics.size() > kMaximumMetrics ||
        snapshot.activity.size() > kMaximumActivity ||
        snapshot.quick_actions.size() > kMaximumQuickActions ||
        !unique_ids(snapshot.metrics) || !unique_ids(snapshot.activity) ||
        !unique_ids(snapshot.quick_actions)) {
        return Result<DashboardSnapshot, DomainError>::failure(invalid("dashboard"));
    }
    if (!std::all_of(snapshot.metrics.begin(), snapshot.metrics.end(), valid_metric)) {
        return Result<DashboardSnapshot, DomainError>::failure(invalid("metrics"));
    }
    if (!std::all_of(snapshot.activity.begin(), snapshot.activity.end(), valid_activity)) {
        return Result<DashboardSnapshot, DomainError>::failure(invalid("activity"));
    }
    if (!std::all_of(snapshot.quick_actions.begin(), snapshot.quick_actions.end(),
                     valid_action)) {
        return Result<DashboardSnapshot, DomainError>::failure(invalid("quick_actions"));
    }

    std::erase_if(snapshot.metrics, [&](const DashboardMetric& metric) {
        return !allowed(context, activation, metric.owner, metric.required_right);
    });
    std::erase_if(snapshot.activity, [&](const DashboardActivity& activity) {
        return !allowed(context, activation, activity.owner, activity.required_right);
    });
    std::erase_if(snapshot.quick_actions, [&](const DashboardQuickAction& action) {
        return !allowed(context, activation, action.owner, action.required_right);
    });
    return Result<DashboardSnapshot, DomainError>::success(std::move(snapshot));
}

}  // namespace squiflow::app::dashboard
