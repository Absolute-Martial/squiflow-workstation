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
                                 protocol::RightId required_right) {
    const std::string route_id = id;
    return {owner,
            std::move(id),
            std::move(title_key),
            std::move(icon_name),
            "qrc:/qt/qml/SquiFlow/screens/ModuleListScreen.qml",
            std::move(group_key),
            group_rank,
            screen_rank,
            required_right,
            [route_id] { return std::make_unique<RoutePresentationBridge>(route_id); }};
}

std::size_t index_of(protocol::ModuleId module) {
    if (!protocol::is_valid(module)) {
        throw std::invalid_argument("navigation completeness contains an invalid module");
    }
    return static_cast<std::size_t>(module);
}

}  // namespace

RoutePresentationBridge::RoutePresentationBridge(std::string route_id)
    : route_id_(std::move(route_id)) {
    if (route_id_.empty()) {
        throw std::invalid_argument("route bridge requires a stable id");
    }
}

ScreenRegistry make_navigation_manifest() {
    ScreenRegistry manifest;
    using M = protocol::ModuleId;
    using R = protocol::RightId;
    manifest.add(primary_route(M::administration, "administration.home",
                               "navigation.administration", "settings", "group.system",
                               50, 10, R::right_person_manage));
    manifest.add(primary_route(M::parties, "parties.list", "navigation.parties",
                               "people", "group.work", 10, 10, R::right_party_read));
    manifest.add(primary_route(M::catalog, "catalog.list", "navigation.catalog",
                               "box", "group.work", 10, 20, R::right_product_read));
    manifest.add(primary_route(M::pricing, "pricing.rates", "navigation.pricing",
                               "tag", "group.work", 10, 30, R::right_rate_read));
    manifest.add(primary_route(M::orders, "orders.list", "navigation.orders",
                               "cart", "group.work", 10, 40, R::right_order_read));
    manifest.add(primary_route(M::receivables, "receivables.invoices",
                               "navigation.receivables", "receipt", "group.finance",
                               30, 10, R::right_invoice_read));
    manifest.add(primary_route(M::jobs, "jobs.list", "navigation.jobs", "briefcase",
                               "group.work", 10, 50, R::right_job_read));
    manifest.add(primary_route(M::quotations, "quotations.list", "navigation.quotations",
                               "quote", "group.sales", 20, 10, R::right_quotation_read));
    manifest.add(primary_route(M::agreements, "agreements.list", "navigation.agreements",
                               "handshake", "group.sales", 20, 20,
                               R::right_agreement_read));
    manifest.add(primary_route(M::sourcing, "sourcing.suppliers", "navigation.sourcing",
                               "truck", "group.purchasing", 40, 10,
                               R::right_supplier_read));
    manifest.add(primary_route(M::companion, "companion.tasks", "navigation.companion",
                               "check", "group.work", 10, 60, R::right_task_read));
    manifest.add(primary_route(M::files, "files.search", "navigation.files", "folder",
                               "group.files", 60, 10, R::right_file_search));
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
