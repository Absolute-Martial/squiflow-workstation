#pragma once

#include "app/contracts/domain_error.hpp"
#include "app/contracts/request_context.hpp"
#include "app/contracts/result.hpp"

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/right_id.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace squiflow::app::dashboard {

struct DashboardMetric final {
    protocol::ModuleId owner{};
    std::optional<protocol::RightId> required_right{};
    std::string id{};
    std::string label_key{};
    std::string value_text{};
    std::string detail_key{};
    std::string route_id{};
    std::optional<std::int64_t> exact_minor_units{};
    std::string currency_code{};

    friend bool operator==(const DashboardMetric&, const DashboardMetric&) = default;
};

struct DashboardActivity final {
    protocol::ModuleId owner{};
    std::optional<protocol::RightId> required_right{};
    std::string id{};
    std::string title_key{};
    std::string detail_text{};
    std::string route_id{};
    std::string record_id{};
    std::int64_t occurred_at_ms{0};

    friend bool operator==(const DashboardActivity&, const DashboardActivity&) = default;
};

struct DashboardQuickAction final {
    protocol::ModuleId owner{};
    protocol::RightId required_right{};
    std::string id{};
    std::string label_key{};
    std::string route_id{};

    friend bool operator==(const DashboardQuickAction&, const DashboardQuickAction&) = default;
};

struct DashboardSnapshot final {
    std::uint64_t produced_at_ms{0};
    bool offline{false};
    bool stale{false};
    std::vector<DashboardMetric> metrics{};
    std::vector<DashboardActivity> activity{};
    std::vector<DashboardQuickAction> quick_actions{};

    friend bool operator==(const DashboardSnapshot&, const DashboardSnapshot&) = default;
};

class DashboardQueryPort {
  public:
    virtual ~DashboardQueryPort() = default;
    virtual Result<DashboardSnapshot, DomainError> load(
        const RequestContext& context) = 0;
};

}  // namespace squiflow::app::dashboard
