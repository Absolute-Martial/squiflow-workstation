#include "app/primary/local_primary_query.hpp"
#include "engine/storage/memory_store.hpp"
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
        store.define_table(modules::administration::tables::kPerson, "id");
        store.define_table(modules::parties::tables::kParty, "id");
        store.define_table(modules::catalog::tables::kProduct, "id");
        store.define_table(modules::pricing::tables::kRate, "id");
        store.define_table(modules::orders::tables::kOrder, "id");
        store.define_table(modules::receivables::tables::kInvoice, "id");
        store.define_table(modules::jobs::tables::kJob, "id");
        store.define_table(modules::quotations::tables::kQuotation, "id");
        store.define_table(modules::agreements::tables::kAgreement, "id");
        store.define_table(modules::sourcing::tables::kSupplier, "id");
        store.define_table(modules::companion::tables::kTask, "id");
        store.define_table(modules::files::tables::kAsset, "id");
    }, {}});
    auto result = std::make_unique<engine::Database>(
        std::make_unique<engine::MemoryStore>(), std::move(runner));
    result->open();
    return result;
}

void seed(engine::Database& database) {
    database.write([](engine::Transaction& transaction) {
        auto person = row("person-1");
        person.set("display_name", engine::Value::text("Shop owner"));
        person.set("username", engine::Value::text("owner"));
        person.set("password_hash", engine::Value::text("must-not-be-projected"));
        person.set("is_owner", engine::Value::boolean(true));
        transaction.insert(modules::administration::tables::kPerson, person);

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

        auto job = row("job-1");
        job.set("party_id", engine::Value::text("party-1"));
        job.set("state", engine::Value::integer(1));
        job.set("ticket_series", engine::Value::text("JOB"));
        job.set("ticket_number", engine::Value::integer(7));
        job.set("title", engine::Value::text("Window graphics"));
        job.set("created_at", engine::Value::integer(12));
        transaction.insert(modules::jobs::tables::kJob, job);

        auto quotation = row("quotation-1");
        quotation.set("party_id", engine::Value::text("party-1"));
        quotation.set("state", engine::Value::integer(1));
        quotation.set("current_revision", engine::Value::integer(2));
        quotation.set("created_at", engine::Value::integer(13));
        transaction.insert(modules::quotations::tables::kQuotation, quotation);

        auto agreement = row("agreement-1");
        agreement.set("party_id", engine::Value::text("party-1"));
        agreement.set("state", engine::Value::integer(1));
        agreement.set("customer_reference", engine::Value::text("Annual signage"));
        agreement.set("created_at", engine::Value::integer(14));
        transaction.insert(modules::agreements::tables::kAgreement, agreement);

        auto supplier = row("supplier-1");
        supplier.set("kind", engine::Value::integer(0));
        supplier.set("supplies", engine::Value::text("Vinyl"));
        supplier.set("updated_at", engine::Value::integer(15));
        transaction.insert(modules::sourcing::tables::kSupplier, supplier);

        auto task = row("task-1");
        task.set("title", engine::Value::text("Call customer"));
        task.set("state", engine::Value::integer(0));
        task.set("due_at", engine::Value::integer(1000));
        transaction.insert(modules::companion::tables::kTask, task);

        auto file = row("file-1");
        file.set("content_hash", engine::Value::text(std::string(64, 'a')));
        file.set("extension", engine::Value::text("pdf"));
        file.set("media_type", engine::Value::text("application/pdf"));
        file.set("size_bytes", engine::Value::integer(4096));
        file.set("created_at", engine::Value::integer(16));
        file.set("path", engine::Value::text("/private/never-project"));
        transaction.insert(modules::files::tables::kAsset, file);
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
    auto administration = query.load(PageKind::Administration, {});
    t::check(administration && administration.value().rows.front().title == "Shop owner" &&
                 administration.value().rows.front().subtitle.find("must-not-be-projected") ==
                     std::string::npos,
             "administration list omits password hashes");
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
    auto jobs = query.load(PageKind::Jobs, {});
    t::check(jobs && jobs.value().rows.front().title == "JOB-7" &&
                 jobs.value().rows.front().subtitle.ends_with("in progress"),
             "job ticket and production state are projected");
    auto quotations = query.load(PageKind::Quotations, {});
    t::check(quotations &&
                 quotations.value().rows.front().subtitle.ends_with("revision 2"),
             "quotation list preserves the current revision identity");
    auto agreements = query.load(PageKind::Agreements, {});
    t::check(agreements && agreements.value().rows.front().title == "Annual signage",
             "agreement list uses its stable customer reference");
    auto sourcing = query.load(PageKind::Sourcing, {});
    t::check(sourcing && sourcing.value().rows.front().subtitle ==
                                  "local dealer | Vinyl",
             "supplier profile is projected without copying party internals");
    auto companion = query.load(PageKind::Companion, {});
    t::check(companion && companion.value().rows.front().title == "Call customer" &&
                 companion.value().rows.front().subtitle.find("due 1000") !=
                     std::string::npos,
             "task due state is projected");
    auto files = query.load(PageKind::Files, {});
    t::check(files && files.value().rows.front().title == "File aaaaaaaaaaaa.pdf" &&
                 files.value().rows.front().subtitle.find("never-project") ==
                     std::string::npos,
             "file list exposes bounded metadata but never a local path");

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
    t::check(!query.load(PageKind::Count, {}),
             "invalid page-kind sentinel is rejected before storage access");
    db->close();
    t::check(!query.load(PageKind::Parties, {}),
             "closed database fails visibly instead of appearing empty");

    return t::report();
}
