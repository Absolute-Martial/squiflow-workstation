#include "app/primary/local_primary_query.hpp"

#include "engine/records/lifecycle.hpp"
#include "modules/catalog/data/tables.hpp"
#include "modules/orders/data/tables.hpp"
#include "modules/parties/data/tables.hpp"
#include "modules/pricing/data/tables.hpp"
#include "modules/receivables/data/tables.hpp"

#include <algorithm>
#include <array>
#include <exception>
#include <string>
#include <string_view>
#include <utility>

namespace squiflow::app::primary {
namespace {

struct Profile final {
    const char* table;
    const char* default_sort;
};

Profile profile(PageKind kind) noexcept {
    using namespace modules;
    switch (kind) {
        case PageKind::Parties: return {parties::tables::kParty, "display_name"};
        case PageKind::Catalog: return {catalog::tables::kProduct, "name"};
        case PageKind::Pricing: return {pricing::tables::kRate, "product_id"};
        case PageKind::Orders: return {orders::tables::kOrder, "created_at"};
        case PageKind::Receivables:
            return {receivables::tables::kInvoice, "created_at"};
    }
    return {parties::tables::kParty, "display_name"};
}

std::string storage_field(PageKind kind, std::string_view field) {
    if (field.empty()) return profile(kind).default_sort;
    switch (kind) {
        case PageKind::Parties:
            return field == "terms" ? "billing" : "display_name";
        case PageKind::Catalog: return "name";
        case PageKind::Pricing:
            return field == "rate" ? "amount_minor" : "product_id";
        case PageKind::Orders:
            if (field == "customer") return "party_id";
            if (field == "status") return "state";
            return "id";
        case PageKind::Receivables:
            if (field == "customer") return "party_id";
            if (field == "status") return "state";
            return "number";
    }
    return profile(kind).default_sort;
}

std::string invoice_state(std::int64_t raw) {
    if (raw < 0 || raw > 4) return "unknown";
    return std::string(engine::to_string(static_cast<engine::DocumentState>(raw)));
}

ListRow snapshot(PageKind kind, const engine::Row& row) {
    const std::string id = row.get("id").text_or({});
    switch (kind) {
        case PageKind::Parties: {
            std::string subtitle = row.get("kind").text_or("party");
            const std::string billing = row.get("billing").text_or({});
            if (!billing.empty()) subtitle += " | " + billing;
            if (row.get("archived").boolean_or(false)) subtitle += " | archived";
            return {id, row.get("display_name").text_or({}), std::move(subtitle)};
        }
        case PageKind::Catalog: {
            std::string subtitle = row.get("description").text_or({});
            if (row.get("archived").boolean_or(false)) {
                if (!subtitle.empty()) subtitle += " | ";
                subtitle += "archived";
            }
            return {id, row.get("name").text_or({}), std::move(subtitle)};
        }
        case PageKind::Pricing: {
            std::string subtitle = std::to_string(row.get("amount_minor").integer_or(0));
            subtitle += " minor units";
            const std::string party = row.get("party_id").text_or({});
            if (!party.empty()) subtitle += " | party " + party;
            return {id, row.get("product_id").text_or({}), std::move(subtitle)};
        }
        case PageKind::Orders: {
            const auto state = row.get("state").integer_or(-1);
            std::string subtitle = row.get("party_id").text_or("unknown party");
            subtitle += state == 0 ? " | open" : state == 1 ? " | cancelled"
                                                          : " | unknown state";
            return {id, id, std::move(subtitle)};
        }
        case PageKind::Receivables: {
            const auto number = row.get("number").integer_or(0);
            std::string title;
            if (number > 0) {
                title = row.get("number_series").text_or({});
                if (!title.empty()) title += "-";
                title += std::to_string(number);
            } else {
                title = "Draft ";
                title += id.substr(0, std::min<std::size_t>(8, id.size()));
            }
            std::string subtitle = row.get("party_id").text_or("unknown party");
            subtitle += " | " + invoice_state(row.get("state").integer_or(-1));
            return {id, std::move(title), std::move(subtitle)};
        }
    }
    return {};
}

}  // namespace

Result<ListPage, DomainError> LocalPrimaryQuery::load(
    PageKind kind, const ListRequest& request) {
    if (!database_.ready()) {
        return Result<ListPage, DomainError>::failure(
            {DomainErrorCode::InvalidContext, "primary.error.database_unavailable", {}});
    }
    try {
        ListPage page;
        database_.read([&](const engine::Store& store) {
            const Profile selected = profile(kind);
            engine::Query query(selected.table);
            if (!request.filter_text.empty()) {
                query.where(storage_field(kind, request.filter_field),
                            engine::Comparison::Contains,
                            engine::Value::text(request.filter_text));
            }
            query.order_by(storage_field(kind, request.sort_field),
                           request.descending ? engine::SortOrder::Descending
                                              : engine::SortOrder::Ascending);
            query.skip(request.offset).take(request.limit + 1);
            auto rows = store.select(query);
            page.has_more = rows.size() > request.limit;
            if (page.has_more) rows.resize(request.limit);
            page.rows.reserve(rows.size());
            for (const auto& row : rows) page.rows.push_back(snapshot(kind, row));
        });
        return Result<ListPage, DomainError>::success(std::move(page));
    } catch (const std::exception&) {
        return Result<ListPage, DomainError>::failure(
            {DomainErrorCode::InvalidContext, "primary.error.query_failed", {}});
    }
}

}  // namespace squiflow::app::primary
