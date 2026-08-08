#include "app/primary/local_primary_query.hpp"

#include "engine/records/lifecycle.hpp"
#include "modules/administration/data/tables.hpp"
#include "modules/agreements/data/tables.hpp"
#include "modules/catalog/data/tables.hpp"
#include "modules/companion/data/tables.hpp"
#include "modules/files/data/tables.hpp"
#include "modules/jobs/data/tables.hpp"
#include "modules/orders/data/tables.hpp"
#include "modules/parties/data/tables.hpp"
#include "modules/pricing/data/tables.hpp"
#include "modules/quotations/data/tables.hpp"
#include "modules/receivables/data/tables.hpp"
#include "modules/sourcing/data/tables.hpp"

#include <algorithm>
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
        case PageKind::Administration:
            return {administration::tables::kPerson, "display_name"};
        case PageKind::Parties: return {parties::tables::kParty, "display_name"};
        case PageKind::Catalog: return {catalog::tables::kProduct, "name"};
        case PageKind::Pricing: return {pricing::tables::kRate, "product_id"};
        case PageKind::Orders: return {orders::tables::kOrder, "created_at"};
        case PageKind::Receivables:
            return {receivables::tables::kInvoice, "created_at"};
        case PageKind::Jobs: return {jobs::tables::kJob, "created_at"};
        case PageKind::Quotations:
            return {quotations::tables::kQuotation, "created_at"};
        case PageKind::Agreements:
            return {agreements::tables::kAgreement, "created_at"};
        case PageKind::Sourcing:
            return {sourcing::tables::kSupplier, "updated_at"};
        case PageKind::Companion:
            return {companion::tables::kTask, "due_at"};
        case PageKind::Files: return {files::tables::kAsset, "created_at"};
        case PageKind::Count: break;
    }
    return {administration::tables::kPerson, "display_name"};
}

std::string storage_field(PageKind kind, std::string_view field) {
    if (field.empty()) return profile(kind).default_sort;
    switch (kind) {
        case PageKind::Administration:
            return field == "access" ? "username" : "display_name";
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
        case PageKind::Jobs:
            if (field == "customer") return "party_id";
            if (field == "status") return "state";
            return "ticket_number";
        case PageKind::Quotations:
            if (field == "customer") return "party_id";
            if (field == "status") return "state";
            return "id";
        case PageKind::Agreements:
            if (field == "customer") return "party_id";
            if (field == "status") return "state";
            return "customer_reference";
        case PageKind::Sourcing:
            return field == "status" ? "kind" : "id";
        case PageKind::Companion:
            if (field == "status") return "state";
            if (field == "due") return "due_at";
            return "title";
        case PageKind::Files:
            return field == "location" ? "media_type" : "content_hash";
        case PageKind::Count: break;
    }
    return profile(kind).default_sort;
}

std::string short_id(std::string_view id) {
    return std::string(id.substr(0, std::min<std::size_t>(8, id.size())));
}

std::string invoice_state(std::int64_t raw) {
    if (raw < 0 || raw > 4) return "unknown";
    return std::string(engine::to_string(static_cast<engine::DocumentState>(raw)));
}

std::string order_state(std::int64_t raw) {
    if (raw == 0) return "open";
    if (raw == 1) return "cancelled";
    return "unknown";
}

std::string job_state(std::int64_t raw) {
    switch (raw) {
        case 0: return "draft";
        case 1: return "in progress";
        case 2: return "done";
        case 3: return "cancelled";
        default: return "unknown";
    }
}

std::string quotation_state(std::int64_t raw) {
    switch (raw) {
        case 0: return "draft";
        case 1: return "issued";
        case 2: return "accepted";
        case 3: return "expired";
        default: return "unknown";
    }
}

std::string agreement_state(std::int64_t raw) {
    switch (raw) {
        case 0: return "draft";
        case 1: return "open";
        case 2: return "closed";
        case 3: return "superseded";
        default: return "unknown";
    }
}

std::string task_state(std::int64_t raw) {
    if (raw == 0) return "open";
    if (raw == 1) return "completed";
    return "unknown";
}

ListRow snapshot(PageKind kind, const engine::Row& row) {
    const std::string id = row.get("id").text_or({});
    switch (kind) {
        case PageKind::Administration: {
            std::string subtitle = row.get("username").text_or({});
            if (row.get("is_owner").boolean_or(false)) subtitle += " | owner";
            if (row.get("disabled").boolean_or(false)) subtitle += " | disabled";
            return {id, row.get("display_name").text_or({}), std::move(subtitle)};
        }
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
            std::string subtitle =
                std::to_string(row.get("amount_minor").integer_or(0));
            subtitle += " minor units";
            const std::string party = row.get("party_id").text_or({});
            if (!party.empty()) subtitle += " | party " + party;
            return {id, row.get("product_id").text_or({}), std::move(subtitle)};
        }
        case PageKind::Orders: {
            std::string subtitle = row.get("party_id").text_or("unknown party");
            subtitle += " | " + order_state(row.get("state").integer_or(-1));
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
                title = "Draft " + short_id(id);
            }
            std::string subtitle = row.get("party_id").text_or("unknown party");
            subtitle += " | " + invoice_state(row.get("state").integer_or(-1));
            return {id, std::move(title), std::move(subtitle)};
        }
        case PageKind::Jobs: {
            const auto number = row.get("ticket_number").integer_or(0);
            std::string title;
            if (number > 0) {
                title = row.get("ticket_series").text_or({});
                if (!title.empty()) title += "-";
                title += std::to_string(number);
            } else {
                title = "Draft " + short_id(id);
            }
            std::string subtitle = row.get("title").text_or({});
            const std::string party = row.get("party_id").text_or({});
            if (!party.empty()) {
                if (!subtitle.empty()) subtitle += " | ";
                subtitle += party;
            }
            if (!subtitle.empty()) subtitle += " | ";
            subtitle += job_state(row.get("state").integer_or(-1));
            return {id, std::move(title), std::move(subtitle)};
        }
        case PageKind::Quotations: {
            std::string title = "Quote " + short_id(id);
            std::string subtitle = row.get("party_id").text_or("walk-in");
            subtitle += " | " + quotation_state(row.get("state").integer_or(-1));
            subtitle += " | revision " +
                        std::to_string(row.get("current_revision").integer_or(1));
            return {id, std::move(title), std::move(subtitle)};
        }
        case PageKind::Agreements: {
            std::string title = row.get("customer_reference").text_or({});
            if (title.empty()) title = "Agreement " + short_id(id);
            std::string subtitle = row.get("party_id").text_or("unknown party");
            subtitle += " | " + agreement_state(row.get("state").integer_or(-1));
            return {id, std::move(title), std::move(subtitle)};
        }
        case PageKind::Sourcing: {
            std::string title = "Supplier " + short_id(id);
            std::string subtitle =
                row.get("kind").integer_or(-1) == 0 ? "local dealer" :
                row.get("kind").integer_or(-1) == 1 ? "importer" : "unknown";
            const std::string supplies = row.get("supplies").text_or({});
            if (!supplies.empty()) subtitle += " | " + supplies;
            return {id, std::move(title), std::move(subtitle)};
        }
        case PageKind::Companion: {
            std::string subtitle = task_state(row.get("state").integer_or(-1));
            const auto due = row.get("due_at").integer_or(0);
            if (due > 0) subtitle += " | due " + std::to_string(due);
            return {id, row.get("title").text_or({}), std::move(subtitle)};
        }
        case PageKind::Files: {
            const std::string hash = row.get("content_hash").text_or({});
            std::string title = "File ";
            title += hash.empty() ? short_id(id) :
                                    hash.substr(0, std::min<std::size_t>(12, hash.size()));
            const std::string extension = row.get("extension").text_or({});
            if (!extension.empty()) title += "." + extension;
            std::string subtitle = row.get("media_type").text_or("file");
            subtitle += " | " + std::to_string(row.get("size_bytes").integer_or(0));
            subtitle += " bytes";
            if (row.get("forgotten").boolean_or(false)) subtitle += " | forgotten";
            return {id, std::move(title), std::move(subtitle)};
        }
        case PageKind::Count: break;
    }
    return {};
}

}  // namespace

Result<ListPage, DomainError> LocalPrimaryQuery::load(
    PageKind kind, const ListRequest& request) {
    if (!is_valid(kind)) {
        return Result<ListPage, DomainError>::failure(
            {DomainErrorCode::ValidationFailed,
             "primary.error.invalid_page_kind", std::string{"kind"}});
    }
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
