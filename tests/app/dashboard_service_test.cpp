#include "app/dashboard/dashboard_service.hpp"
#include "support/check.hpp"

#include <utility>

namespace {
using namespace squiflow;
using app::dashboard::DashboardSnapshot;

class Query final : public app::dashboard::DashboardQueryPort {
  public:
    DashboardSnapshot snapshot{};
    app::Result<DashboardSnapshot, app::DomainError> load(
        const app::RequestContext&) override {
        return app::Result<DashboardSnapshot, app::DomainError>::success(snapshot);
    }
};

app::RequestContext context(engine::RightsSet rights, std::uint64_t generation = 1) {
    auto value = app::RequestContext::create({{1, 2}}, {3, 4}, std::move(rights),
                                              "dashboard-test", generation);
    if (!value) {
        throw std::logic_error("dashboard test context failed");
    }
    return std::move(value).value();
}

DashboardSnapshot populated() {
    using M = protocol::ModuleId;
    using R = protocol::RightId;
    DashboardSnapshot result;
    result.produced_at_ms = 100;
    result.metrics = {
        {M::receivables, R::right_invoice_read, "receivables.due",
         "dashboard.receivables_due", "125.00 USD", "dashboard.overdue",
         "receivables.invoices", 12500, "USD"},
        {M::jobs, R::right_job_read, "jobs.open", "dashboard.open_jobs", "4",
         "dashboard.jobs_active", "jobs.list", std::nullopt, ""}};
    result.activity = {
        {M::orders, R::right_order_read, "activity.order", "dashboard.order_updated",
         "Order 12", "orders.list", "0123456789abcdef0123456789abcdef", 90}};
    result.quick_actions = {
        {M::orders, R::right_order_write, "order.new", "dashboard.new_order",
         "orders.list"},
        {M::companion, R::right_task_write, "task.new", "dashboard.new_task",
         "companion.tasks"}};
    return result;
}
}

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;
    using R = protocol::RightId;

    Query query;
    query.snapshot = populated();
    app::dashboard::DashboardService service(query);
    engine::RightsSet rights;
    rights.grant(R::right_invoice_read);
    rights.grant(R::right_order_read);
    rights.grant(R::right_order_write);
    const auto activation = protocol::resolve_activation({}).activation;

    t::section("permission-filtered immutable snapshot");
    auto result = service.refresh(context(rights), activation);
    t::check(result.has_value(), "representative dashboard loads");
    t::check(result.value().metrics.size() == 1 &&
                 result.value().metrics.front().exact_minor_units == 12500,
             "money stays exact and unauthorized metrics disappear");
    t::check(result.value().activity.size() == 1,
             "authorized activity survives mapping");
    t::check(result.value().quick_actions.size() == 1 &&
                 result.value().quick_actions.front().id == "order.new",
             "quick actions require their write right");

    t::section("activation and empty account");
    auto inactive = activation;
    inactive.active[static_cast<std::size_t>(protocol::ModuleId::orders)] = false;
    auto without_orders = service.refresh(context(rights), inactive);
    t::check(without_orders && without_orders.value().activity.empty() &&
                 without_orders.value().quick_actions.empty(),
             "inactive module contributes no card or action");
    query.snapshot = {};
    auto empty = service.refresh(context(rights), activation);
    t::check(empty && empty.value().metrics.empty(), "new account is a valid empty dashboard");

    t::section("source failure and malformed data");
    query.snapshot = populated();
    query.snapshot.metrics.front().id = "bad id";
    auto malformed = service.refresh(context(rights), activation);
    t::check(!malformed && malformed.error().code ==
                               app::DomainErrorCode::ValidationFailed,
             "unsafe source identifiers fail closed");
    query.snapshot = populated();
    query.snapshot.quick_actions.push_back(query.snapshot.quick_actions.front());
    t::check(!service.refresh(context(rights), activation),
             "duplicate action identity is rejected");
    query.snapshot = populated();
    query.snapshot.metrics.resize(app::dashboard::DashboardService::kMaximumMetrics + 1);
    t::check(!service.refresh(context(rights), activation),
             "unbounded metric response is rejected");

    return t::report();
}
