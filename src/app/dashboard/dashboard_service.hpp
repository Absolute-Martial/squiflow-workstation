#pragma once

#include "app/dashboard/dashboard_query.hpp"

namespace squiflow::app::dashboard {

class DashboardService final {
  public:
    static constexpr std::size_t kMaximumMetrics = 16;
    static constexpr std::size_t kMaximumActivity = 20;
    static constexpr std::size_t kMaximumQuickActions = 12;

    explicit DashboardService(DashboardQueryPort& query) noexcept : query_(query) {}

    Result<DashboardSnapshot, DomainError> refresh(
        const RequestContext& context,
        const protocol::Activation& activation) const;

  private:
    DashboardQueryPort& query_;
};

}  // namespace squiflow::app::dashboard
