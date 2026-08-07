#include "app/primary/primary_page_service.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>

namespace squiflow::app::primary {
namespace {

DomainError error(DomainErrorCode code, std::string message,
                  std::string field = {}) {
    return {code, std::move(message),
            field.empty() ? std::optional<std::string>{}
                          : std::optional<std::string>{std::move(field)}};
}

bool safe_id(std::string_view value) noexcept {
    if (value.empty() || value.size() > PrimaryPageService::kMaximumIdBytes) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char raw) {
        const auto c = static_cast<unsigned char>(raw);
        return std::isalnum(c) != 0 || c == static_cast<unsigned char>('-') ||
               c == static_cast<unsigned char>('_') ||
               c == static_cast<unsigned char>('.') ||
               c == static_cast<unsigned char>('/');
    });
}

bool valid_row(const ListRow& row) noexcept {
    return safe_id(row.stable_id) && !row.title.empty() &&
           row.title.size() <= PrimaryPageService::kMaximumTitleBytes &&
           row.subtitle.size() <= PrimaryPageService::kMaximumSubtitleBytes;
}

}  // namespace

protocol::ModuleId PrimaryPageService::owner(PageKind kind) noexcept {
    using M = protocol::ModuleId;
    switch (kind) {
        case PageKind::Parties: return M::parties;
        case PageKind::Catalog: return M::catalog;
        case PageKind::Pricing: return M::pricing;
        case PageKind::Orders: return M::orders;
        case PageKind::Receivables: return M::receivables;
    }
    return M::administration;
}

protocol::RightId PrimaryPageService::read_right(PageKind kind) noexcept {
    using R = protocol::RightId;
    switch (kind) {
        case PageKind::Parties: return R::right_party_read;
        case PageKind::Catalog: return R::right_product_read;
        case PageKind::Pricing: return R::right_rate_read;
        case PageKind::Orders: return R::right_order_read;
        case PageKind::Receivables: return R::right_invoice_read;
    }
    return R::right_audit_read;
}

bool PrimaryPageService::valid_field(PageKind kind, std::string_view field,
                                     bool filtering) noexcept {
    if (field.empty()) {
        return true;
    }
    switch (kind) {
        case PageKind::Parties:
            return field == "name" || field == "terms";
        case PageKind::Catalog:
            return field == "name";
        case PageKind::Pricing:
            return field == "name" || (!filtering && field == "rate");
        case PageKind::Orders:
        case PageKind::Receivables:
            return field == "number" || field == "customer" || field == "status";
    }
    return false;
}

Result<ListPage, DomainError> PrimaryPageService::list(
    const RequestContext& context, const protocol::Activation& activation,
    PageKind kind, const ListRequest& request) const {
    const auto module = owner(kind);
    const auto right = read_right(kind);
    if (!activation.is_active(module)) {
        return Result<ListPage, DomainError>::failure(
            error(DomainErrorCode::Unauthorized, "primary.error.module_inactive"));
    }
    if (!context.permissions().has(right)) {
        return Result<ListPage, DomainError>::failure(
            error(DomainErrorCode::Unauthorized, "primary.error.read_denied"));
    }
    if (request.limit == 0 || request.limit > kMaximumPageRows ||
        request.offset > kMaximumOffset) {
        return Result<ListPage, DomainError>::failure(
            error(DomainErrorCode::ValidationFailed,
                  "primary.error.invalid_page", "page"));
    }
    if (request.filter_text.size() > kMaximumFilterBytes ||
        (!request.filter_text.empty() && request.filter_field.empty()) ||
        !valid_field(kind, request.filter_field, true) ||
        !valid_field(kind, request.sort_field, false)) {
        return Result<ListPage, DomainError>::failure(
            error(DomainErrorCode::ValidationFailed,
                  "primary.error.invalid_query", "query"));
    }

    auto loaded = query_.load(kind, request);
    if (!loaded) {
        return Result<ListPage, DomainError>::failure(loaded.error());
    }
    ListPage page = std::move(loaded).value();
    if (page.rows.size() > request.limit) {
        return Result<ListPage, DomainError>::failure(
            error(DomainErrorCode::ValidationFailed,
                  "primary.error.unbounded_page", "rows"));
    }
    std::unordered_set<std::string> ids;
    ids.reserve(page.rows.size());
    for (const auto& row : page.rows) {
        if (!valid_row(row) || !ids.insert(row.stable_id).second) {
            return Result<ListPage, DomainError>::failure(
                error(DomainErrorCode::ValidationFailed,
                      "primary.error.invalid_snapshot", "rows"));
        }
    }
    return Result<ListPage, DomainError>::success(std::move(page));
}

}  // namespace squiflow::app::primary
