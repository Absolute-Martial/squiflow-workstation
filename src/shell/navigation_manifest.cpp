#include "shell/navigation_manifest.hpp"

#include <array>
#include <stdexcept>
#include <utility>

namespace squiflow::shell {
namespace {

ScreenContribution primary_route(protocol::ModuleId owner, std::string id,
                                 std::string title_key, std::string icon_name,
                                 std::string group_key, std::uint16_t group_rank,
                                 std::uint16_t screen_rank,
                                 protocol::RightId required_right,
                                 std::vector<ListColumn> columns,
                                 std::string component_url =
                                     "qrc:/qt/qml/SquiFlow/screens/ModuleListScreen.qml") {
    const std::string route_id = id;
    return {owner,
            std::move(id),
            std::move(title_key),
            std::move(icon_name),
            std::move(component_url),
            std::move(group_key),
            group_rank,
            screen_rank,
            required_right,
            [route_id, columns = std::move(columns)] {
                return std::make_unique<RoutePresentationBridge>(route_id, columns);
            }};
}

ListColumn column(std::string id, bool sortable = true, bool filterable = true) {
    return {id, "column." + id, sortable, filterable};
}

std::size_t index_of(protocol::ModuleId module) {
    if (!protocol::is_valid(module)) {
        throw std::invalid_argument("navigation completeness contains an invalid module");
    }
    return static_cast<std::size_t>(module);
}

}  // namespace

RoutePresentationBridge::RoutePresentationBridge(
    std::string route_id, std::vector<ListColumn> columns)
    : route_id_(std::move(route_id)), list_(std::move(columns)) {
    if (route_id_.empty()) {
        throw std::invalid_argument("route bridge requires a stable id");
    }
}

ScreenRegistry make_navigation_manifest() {
    ScreenRegistry manifest;
    using M = protocol::ModuleId;
    using R = protocol::RightId;
    manifest.add({M::administration, "dashboard.home", "navigation.dashboard",
                  "home", "qrc:/qt/qml/SquiFlow/dashboard/DashboardPage.qml",
                  "group.home", 0, 0, std::nullopt,
                  [] { return std::make_unique<DashboardPresentationBridge>(); }});
    manifest.add(primary_route(M::administration, "administration.home",
                               "navigation.administration", "settings", "group.system",
                               50, 10, R::right_person_manage,
                               {column("name"), column("access")},
                               "qrc:/qt/qml/SquiFlow/administration/AdministrationPage.qml"));
    manifest.add(primary_route(M::parties, "parties.list", "navigation.parties",
                               "people", "group.work", 10, 10, R::right_party_read,
                               {column("name"), column("terms")},
                               "qrc:/qt/qml/SquiFlow/parties/PartiesPage.qml"));
    manifest.add(primary_route(M::catalog, "catalog.list", "navigation.catalog",
                               "box", "group.work", 10, 20, R::right_product_read,
                               {column("name")},
                               "qrc:/qt/qml/SquiFlow/catalog/CatalogPage.qml"));
    manifest.add(primary_route(M::pricing, "pricing.rates", "navigation.pricing",
                               "tag", "group.work", 10, 30, R::right_rate_read,
                               {column("name"), column("rate", true, false)},
                               "qrc:/qt/qml/SquiFlow/pricing/PricingPage.qml"));
    manifest.add(primary_route(M::orders, "orders.list", "navigation.orders",
                               "cart", "group.work", 10, 40, R::right_order_read,
                               {column("number"), column("customer"), column("status"),
                                column("total", false, false)},
                               "qrc:/qt/qml/SquiFlow/orders/OrdersPage.qml"));
    manifest.add({M::orders, "orders.counter_sale", "navigation.counter_sale",
                  "calculator", "qrc:/qt/qml/SquiFlow/counter/CounterSalePage.qml",
                  "group.work", 10, 41, R::right_order_write,
                  [] { return std::make_unique<PresentationBridge>(); }});
    manifest.add(primary_route(M::receivables, "receivables.invoices",
                               "navigation.receivables", "receipt", "group.finance",
                               30, 10, R::right_invoice_read,
                               {column("number"), column("customer"), column("status"),
                                column("total", false, false)},
                               "qrc:/qt/qml/SquiFlow/receivables/ReceivablesPage.qml"));
    manifest.add(primary_route(M::jobs, "jobs.list", "navigation.jobs", "briefcase",
                               "group.work", 10, 50, R::right_job_read,
                               {column("number"), column("customer"), column("status")},
                               "qrc:/qt/qml/SquiFlow/jobs/JobsPage.qml"));
    manifest.add(primary_route(M::quotations, "quotations.list", "navigation.quotations",
                               "quote", "group.sales", 20, 10, R::right_quotation_read,
                               {column("number"), column("customer"), column("status"),
                                column("total", false, false)},
                               "qrc:/qt/qml/SquiFlow/quotations/QuotationsPage.qml"));
    manifest.add(primary_route(M::agreements, "agreements.list", "navigation.agreements",
                               "handshake", "group.sales", 20, 20,
                               R::right_agreement_read,
                               {column("name"), column("customer"), column("status")},
                               "qrc:/qt/qml/SquiFlow/agreements/AgreementsPage.qml"));
    manifest.add(primary_route(M::sourcing, "sourcing.suppliers", "navigation.sourcing",
                               "truck", "group.purchasing", 40, 10,
                               R::right_supplier_read,
                               {column("supplier"), column("status")},
                               "qrc:/qt/qml/SquiFlow/sourcing/SourcingPage.qml"));
    manifest.add(primary_route(M::companion, "companion.tasks", "navigation.companion",
                               "check", "group.work", 10, 60, R::right_task_read,
                               {column("title"), column("status"), column("due")},
                               "qrc:/qt/qml/SquiFlow/companion/CompanionPage.qml"));
    manifest.add(primary_route(M::files, "files.search", "navigation.files", "folder",
                               "group.files", 60, 10, R::right_file_search,
                               {column("name"), column("location")},
                               "qrc:/qt/qml/SquiFlow/files/FilesPage.qml"));
    return manifest;
}

void require_navigation_complete(
    const ScreenRegistry& manifest,
    const std::vector<protocol::ModuleId>& registered_modules,
    const std::vector<protocol::ModuleId>& deliberately_headless) {
    std::array<bool, protocol::kModuleCount> registered{};
    std::array<bool, protocol::kModuleCount> headless{};
    std::array<bool, protocol::kModuleCount> represented{};

    for (const protocol::ModuleId module : registered_modules) {
        const std::size_t index = index_of(module);
        if (registered[index]) {
            throw std::invalid_argument("navigation completeness repeats a registered module");
        }
        registered[index] = true;
    }
    for (const protocol::ModuleId module : deliberately_headless) {
        const std::size_t index = index_of(module);
        if (headless[index]) {
            throw std::invalid_argument("navigation completeness repeats a headless module");
        }
        if (!registered[index]) {
            throw std::invalid_argument("unregistered module cannot be declared headless");
        }
        headless[index] = true;
    }
    for (const ScreenContribution& screen : manifest.all()) {
        const std::size_t index = index_of(screen.owner);
        if (!registered[index]) {
            throw std::logic_error("navigation route belongs to an unregistered module");
        }
        represented[index] = true;
    }
    for (std::size_t index = 0; index < protocol::kModuleCount; ++index) {
        if (headless[index] && represented[index]) {
            throw std::logic_error("module cannot be both navigable and headless");
        }
        if (registered[index] && !represented[index] && !headless[index]) {
            throw std::logic_error("registered module has no navigation contribution");
        }
    }
}

}  // namespace squiflow::shell
