#include "app/primary/local_primary_query.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/catalog/data/tables.hpp"
#include "modules/orders/data/tables.hpp"
#include "modules/parties/data/tables.hpp"
#include "modules/pricing/data/tables.hpp"
#include "modules/receivables/data/tables.hpp"
#include "support/check.hpp"

#include <memory>
#include <utility>

namespace {
using namespace squiflow;

engine::Row row(std::string id) {
    engine::Row value;
    value.set("id", engine::Value::text(std::move(id)));
    return value;
}

std::unique_ptr<engine::Database> database() {
    engine::MigrationRunner runner([] { return std::int64_t{1}; });
    runner.add({1, "primary test tables", [](engine::Store& store) {
        store.define_table(modules::parties::tables::kParty, "id");
        store.define_table(modules::catalog::tables::kProduct, "id");
        store.define_table(modules::pricing::tables::kRate, "id");
        store.define_table(modules::orders::tables::kOrder, "id");
        store.define_table(modules::receivables::tables::kInvoice, "id");
    }, {}});
    auto result = std::make_unique<engine::Database>(
        std::make_unique<engine::MemoryStore>(), std::move(runner));
    result->open();
    return result;
}

void seed(engine::Database& database) {
    database.write([](engine::Transaction& transaction) {
        auto party = row("party-1");
        party.set("display_name", engine::Value::text("Acme Works"));
        party.set("kind", engine::Value::text("organization"));
        party.set("billing", engine::Value::text("credit"));
        transaction.insert(modules::parties::tables::kParty, party);

        auto product = row("product-1");
        product.set("name", engine::Value::text("Service plan"));
        product.set("description", engine::Value::text("Monthly support"));
        transaction.insert(modules::catalog::tables::kProduct, product);

        auto rate = row("rate-1");
        rate.set("product_id", engine::Value::text("product-1"));
        rate.set("party_id", engine::Value::text("party-1"));
        rate.set("amount_minor", engine::Value::integer(12500));
        transaction.insert(modules::pricing::tables::kRate, rate);

        auto order = row("order-1");
        order.set("party_id", engine::Value::text("party-1"));
        order.set("state", engine::Value::integer(0));
        order.set("created_at", engine::Value::integer(10));
        transaction.insert(modules::orders::tables::kOrder, order);

        auto invoice = row("invoice-1");
        invoice.set("party_id", engine::Value::text("party-1"));
        invoice.set("state", engine::Value::integer(1));
        invoice.set("number_series", engine::Value::text("INV"));
        invoice.set("number", engine::Value::integer(42));
        invoice.set("created_at", engine::Value::integer(11));
        transaction.insert(modules::receivables::tables::kInvoice, invoice);
    });
}
}

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;
    using app::primary::PageKind;

    auto db = database();
    seed(*db);
    app::primary::LocalPrimaryQuery query(*db);

    t::section("local records become immutable bounded snapshots");
    auto parties = query.load(PageKind::Parties, {});
    t::check(parties && parties.value().rows.front().title == "Acme Works",
             "party display name is loaded from the real local store");
    auto catalog = query.load(PageKind::Catalog, {});
    t::check(catalog && catalog.value().rows.front().subtitle == "Monthly support",
             "catalog description is preserved");
    auto pricing = query.load(PageKind::Pricing, {});
    t::check(pricing && pricing.value().rows.front().subtitle.find("12500 minor units") == 0,
             "price remains exact integer minor units");
    auto orders = query.load(PageKind::Orders, {});
    t::check(orders && orders.value().rows.front().subtitle.ends_with("open"),
             "order state is translated without exposing storage rows");
    auto invoices = query.load(PageKind::Receivables, {});
    t::check(invoices && invoices.value().rows.front().title == "INV-42",
             "issued invoice uses its stable assigned number");

    t::section("filter, paging and unavailable storage");
    app::primary::ListRequest request;
    request.filter_field = "name";
    request.filter_text = "missing";
    t::check(query.load(PageKind::Parties, request).value().rows.empty(),
             "approved server-side filter returns an empty page honestly");
    request = {};
    request.offset = 1;
    t::check(query.load(PageKind::Parties, request).value().rows.empty(),
             "offset is applied in the storage query");
    db->close();
    t::check(!query.load(PageKind::Parties, {}),
             "closed database fails visibly instead of appearing empty");

    return t::report();
}
