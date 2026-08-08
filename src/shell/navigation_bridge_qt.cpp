#include "shell/navigation_bridge_qt.hpp"

#if defined(SQUIFLOW_WITH_QT)

#include "app/workspace_runtime.hpp"
#include "shell/dashboard_bridge_qt.hpp"
#include "shell/list_screen_bridge_qt.hpp"
#include "shell/navigation_manifest.hpp"
#include "shell/record_screen_bridge_qt.hpp"

#include <QMetaObject>
#include <QPointer>
#include <QThread>

#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace squiflow::shell {
namespace {

std::optional<app::primary::PageKind> page_kind_for_route(std::string_view route) {
    using K = app::primary::PageKind;
    if (route == "administration.home") return K::Administration;
    if (route == "parties.list") return K::Parties;
    if (route == "catalog.list") return K::Catalog;
    if (route == "pricing.rates") return K::Pricing;
    if (route == "orders.list") return K::Orders;
    if (route == "receivables.invoices") return K::Receivables;
    if (route == "jobs.list") return K::Jobs;
    if (route == "quotations.list") return K::Quotations;
    if (route == "agreements.list") return K::Agreements;
    if (route == "sourcing.suppliers") return K::Sourcing;
    if (route == "companion.tasks") return K::Companion;
    if (route == "files.search") return K::Files;
    return std::nullopt;
}

std::optional<protocol::OperationId> create_operation_for(
    app::primary::PageKind kind) {
    const char* name = nullptr;
    using K = app::primary::PageKind;
    switch (kind) {
        case K::Administration: name = "person_create"; break;
        case K::Parties: name = "party_create"; break;
        case K::Catalog: name = "product_create"; break;
        case K::Pricing: name = "rate_create"; break;
        case K::Orders: name = "order_create"; break;
        case K::Receivables: name = "invoice_create"; break;
        case K::Jobs: name = "job_create"; break;
        case K::Quotations: name = "quotation_create"; break;
        case K::Agreements: name = "agreement_create"; break;
        case K::Sourcing: name = "supplier_create"; break;
        case K::Companion: name = "task_create"; break;
        case K::Files:
        case K::Count: return std::nullopt;
    }
    const auto* operation = protocol::find_operation(name);
    return operation == nullptr ? std::nullopt
                                : std::optional<protocol::OperationId>{operation->id};
}

app::DomainError missing_workspace_error() {
    return {app::DomainErrorCode::InvalidContext,
            "request_context.session_generation_required",
            std::optional<std::string>{"session_generation"}};
}

}  // namespace

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

QObject* NavigationBridgeQt::currentRecordBridge() const noexcept {
    return current_record_bridge_.get();
}

bool NavigationBridgeQt::finish(const app::Result<void, NavigationError>& result) {
    const QString next_error = result.has_value()
        ? QString{}
        : QString::fromStdString(result.error().message_key);
    if (next_error != last_error_key_) {
        last_error_key_ = next_error;
        emit lastErrorChanged();
    }
    if (!result) return false;
    synchronize();
    return true;
}

void NavigationBridgeQt::synchronize() {
    current_record_bridge_.reset();
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
        const auto kind = page_kind_for_route(route->route_id());
        if (kind && workspace_ != nullptr && tenant_) {
            connect(current_list_bridge_.get(), &ListScreenBridgeQt::pageRequested,
                    this,
                    [this, kind = *kind](qulonglong generation, qulonglong offset,
                                         qulonglong limit, const QString& sort_field,
                                         bool descending, const QString& filter_field,
                                         const QString& filter_text) {
                        fulfillList(kind, generation, offset, limit, sort_field,
                                    descending, filter_field, filter_text);
                    });
            current_record_bridge_ = std::make_unique<RecordScreenBridgeQt>(
                *kind,
                [this](app::primary::PageKind record_kind,
                       std::string_view stable_id) {
                    auto context = requestContext();
                    if (workspace_ == nullptr || !context) {
                        return app::Result<app::primary::RecordSnapshot,
                                           app::DomainError>::failure(
                            missing_workspace_error());
                    }
                    return workspace_->record(*context, activation_, record_kind,
                                              stable_id);
                },
                [this](const app::primary::CommandRequest& request) {
                    auto context = requestContext();
                    if (workspace_ == nullptr || !context) {
                        return app::Result<app::primary::CommandAck,
                                           app::DomainError>::failure(
                            missing_workspace_error());
                    }
                    return workspace_->dispatch(*context, request);
                },
                create_operation_for(*kind), this);
        }
    }
    model_.refreshFromController();
    emit currentRouteChanged();
    emit accessibleModulesChanged();
}

std::optional<app::RequestContext> NavigationBridgeQt::requestContext() {
    if (workspace_ == nullptr || !tenant_ || !workspace_->signed_in() ||
        workspace_->session_generation() == 0) {
        return std::nullopt;
    }
    do {
        ++request_sequence_;
    } while (request_sequence_ == 0);
    const auto& session = workspace_->current_session();
    auto context = app::RequestContext::create(
        *tenant_, session.person, session.rights,
        "desktop:" + std::to_string(request_sequence_),
        workspace_->session_generation());
    if (!context) return std::nullopt;
    return context.value();
}

void NavigationBridgeQt::fulfillList(
    app::primary::PageKind kind, qulonglong generation, qulonglong offset,
    qulonglong limit, const QString& sort_field, bool descending,
    const QString& filter_field, const QString& filter_text) {
    if (current_list_bridge_ == nullptr || workspace_ == nullptr ||
        offset > static_cast<qulonglong>(std::numeric_limits<std::size_t>::max()) ||
        limit > static_cast<qulonglong>(std::numeric_limits<std::size_t>::max())) {
        if (current_list_bridge_) {
            current_list_bridge_->failPage(
                static_cast<std::uint64_t>(generation),
                "list.error.provider_unavailable");
        }
        return;
    }
    auto context = requestContext();
    if (!context) {
        current_list_bridge_->failPage(static_cast<std::uint64_t>(generation),
                                       "request_context.invalid");
        return;
    }
    app::primary::ListRequest request;
    request.offset = static_cast<std::size_t>(offset);
    request.limit = static_cast<std::size_t>(limit);
    request.sort_field = sort_field.toStdString();
    request.descending = descending;
    request.filter_field = filter_field.toStdString();
    request.filter_text = filter_text.toStdString();
    const auto page = workspace_->list(*context, activation_, kind, request);
    if (!page) {
        current_list_bridge_->failPage(static_cast<std::uint64_t>(generation),
                                       page.error().message_key);
        return;
    }
    std::vector<RowInput> rows;
    rows.reserve(page.value().rows.size());
    for (const auto& row : page.value().rows) {
        rows.push_back({row.stable_id, row.title, row.subtitle});
    }
    current_list_bridge_->applyPage(static_cast<std::uint64_t>(generation),
                                    std::move(rows), page.value().has_more);
}

void NavigationBridgeQt::attachWorkspace(
    app::AuthenticatedWorkspace& workspace, app::TenantId tenant,
    protocol::Activation activation) {
    Q_ASSERT(thread() == QThread::currentThread());
    workspace_ = &workspace;
    tenant_ = tenant;
    activation_ = std::move(activation);
    request_sequence_ = 0;
    synchronize();
}

void NavigationBridgeQt::detachWorkspace() noexcept {
    if (thread() != QThread::currentThread()) return;
    if (current_list_bridge_) current_list_bridge_->cancel();
    current_record_bridge_.reset();
    workspace_ = nullptr;
    tenant_.reset();
    request_sequence_ = 0;
    emit currentRouteChanged();
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
                if (self) self->applyAccessOnGui(std::move(access));
            },
            Qt::QueuedConnection);
        return;
    }
    applyAccessOnGui(std::move(access));
}

void NavigationBridgeQt::applyAccessOnGui(NavigationAccess access) {
    Q_ASSERT(thread() == QThread::currentThread());
    activation_ = access.activation;
    (void)finish(controller_.apply_access(std::move(access)));
}

void NavigationBridgeQt::shutdown() noexcept {
    if (thread() != QThread::currentThread()) return;
    detachWorkspace();
    controller_.shutdown();
    last_error_key_.clear();
    synchronize();
    emit lastErrorChanged();
}

}  // namespace squiflow::shell

#endif
