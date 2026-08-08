#include "app/primary/local_record_query.hpp"

#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/administration/data/repository.hpp"
#include "modules/agreements/data/repository.hpp"
#include "modules/catalog/data/repository.hpp"
#include "modules/companion/data/repository.hpp"
#include "modules/files/data/repository.hpp"
#include "modules/jobs/data/repository.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/pricing/data/repository.hpp"
#include "modules/quotations/data/repository.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/sourcing/data/repository.hpp"
#include "support/check.hpp"

#include <memory>
#include <stdexcept>
#include <utility>

namespace {
using namespace squiflow;

constexpr std::int64_t kNow = 1'700'000'000'000;
const std::string kPerson = "10000000000000010000000000000001";
const std::string kParty = "10000000000000010000000000000002";
const std::string kProduct = "10000000000000010000000000000003";
const std::string kRate = "10000000000000010000000000000004";
const std::string kOrder = "10000000000000010000000000000005";
const std::string kOrderLine = "10000000000000010000000000000006";
const std::string kInvoice = "10000000000000010000000000000007";
const std::string kInvoiceLine = "10000000000000010000000000000008";
const std::string kJob = "10000000000000010000000000000009";
const std::string kQuotation = "1000000000000001000000000000000a";
const std::string kRevision = "1000000000000001000000000000000b";
const std::string kQuotationLine = "1000000000000001000000000000000c";
const std::string kAgreement = "1000000000000001000000000000000d";
const std::string kAgreementLine = "1000000000000001000000000000000e";
const std::string kSupplier = "1000000000000001000000000000000f";
const std::string kMaterial = "10000000000000010000000000000010";
const std::string kPurchase = "10000000000000010000000000000011";
const std::string kTask = "10000000000000010000000000000012";
const std::string kTaskEvent = "10000000000000010000000000000013";
const std::string kAsset = "10000000000000010000000000000014";
const std::string kLocation = "10000000000000010000000000000015";
const std::string kLink = "10000000000000010000000000000016";

std::unique_ptr<engine::Database> database() {
    engine::MigrationRunner runner([] { return std::int64_t{1}; });
    runner.add({1, "record test tables", [](engine::Store& store) {
        store.define_table(modules::administration::tables::kPerson, "id");
        store.define_table(modules::administration::tables::kPersonRight, "id");
        store.define_table(modules::parties::tables::kParty, "id");
        store.define_table(modules::parties::tables::kContact, "id");
        store.define_table(modules::catalog::tables::kProduct, "id");
        store.define_table(modules::pricing::tables::kRate, "id");
        store.define_table(modules::orders::tables::kOrder, "id");
        store.define_table(modules::orders::tables::kOrderLine, "id");
        store.define_table(modules::receivables::tables::kInvoice, "id");
        store.define_table(modules::receivables::tables::kInvoiceLine, "id");
        store.define_table(modules::receivables::tables::kPayment, "id");
        store.define_table(modules::receivables::tables::kAllocation, "id");
        store.define_table(modules::jobs::tables::kJob, "id");
        store.define_table(modules::quotations::tables::kQuotation, "id");
        store.define_table(modules::quotations::tables::kRevision, "id");
        store.define_table(modules::quotations::tables::kLine, "id");
        store.define_table(modules::agreements::tables::kAgreement, "id");
        store.define_table(modules::agreements::tables::kLine, "id");
        store.define_table(modules::sourcing::tables::kSupplier, "id");
        store.define_table(modules::sourcing::tables::kMaterial, "id");
        store.define_table(modules::sourcing::tables::kPurchase, "id");
        store.define_table(modules::companion::tables::kTask, "id");
        store.define_table(modules::companion::tables::kEvent, "id");
        store.define_table(modules::files::tables::kAsset, "id");
        store.define_table(modules::files::tables::kLocation, "id");
        store.define_table(modules::files::tables::kLink, "id");
        store.define_table(modules::files::tables::kVolume, "id");
    }, {}});
    auto db = std::make_unique<engine::Database>(
        std::make_unique<engine::MemoryStore>(), std::move(runner));
    db->open();
    return db;
}

void seed(engine::Database& db) {
    db.write([](engine::Transaction& tx) {
        modules::administration::Person person;
        person.id = kPerson;
        person.display_name = "Owner";
        person.username = "owner";
        person.password_hash = "top-secret-hash";
        person.is_owner = true;
        person.created_at = kNow - 1000;
        person.updated_at = kNow - 500;
        person.created_by = kPerson;
        modules::administration::data::save_person(tx, person);
        modules::administration::data::grant_right(
            tx, person.id, protocol::RightId::right_person_manage,
            kNow - 400, person.id);

        modules::parties::Party party;
        party.id = kParty;
        party.display_name = "Acme Works";
        party.kind = modules::parties::PartyKind::Organisation;
        party.terms.arrangement = modules::parties::BillingArrangement::CreditAccount;
        party.terms.net_days = 30;
        party.terms.customer_reference = "PO-77";
        party.notes = "Priority account";
        party.created_at = kNow - 1000;
        party.updated_at = kNow - 700;
        party.created_by = kPerson;
        party.updated_by = kPerson;
        modules::parties::data::save_party(tx, party);

        modules::catalog::Product product;
        product.id = kProduct;
        product.name = "Window graphics";
        product.description = "Full color vinyl";
        product.created_at = kNow - 1000;
        product.updated_at = kNow - 700;
        product.created_by = kPerson;
        product.updated_by = kPerson;
        modules::catalog::data::save_product(tx, product);

        modules::pricing::Rate rate;
        rate.id = kRate;
        rate.product_id = kProduct;
        rate.party_id = kParty;
        rate.amount_minor = 12500;
        rate.valid_from = kNow - 5000;
        rate.valid_until = 0;
        rate.created_at = kNow - 900;
        rate.created_by = kPerson;
        modules::pricing::data::save_rate(tx, rate);

        modules::orders::Order order;
        order.id = kOrder;
        order.party_id = kParty;
        order.note = "Urgent";
        order.created_at = kNow - 800;
        order.created_by = kPerson;
        modules::orders::data::save_order(tx, order);
        modules::orders::OrderLine order_line;
        order_line.id = kOrderLine;
        order_line.order_id = kOrder;
        order_line.product_id = kProduct;
        order_line.description = "Window graphics";
        order_line.quantity_scaled = 2000;
        order_line.unit_price_minor = 12500;
        order_line.price_source = modules::pricing::RateSource::PartyRate;
        order_line.added_at = kNow - 700;
        order_line.added_by = kPerson;
        modules::orders::data::save_line(tx, order_line);

        modules::receivables::Invoice invoice;
        invoice.id = kInvoice;
        invoice.party_id = kParty;
        invoice.created_at = kNow - 700;
        invoice.created_by = kPerson;
        modules::receivables::data::save_invoice(tx, invoice);
        modules::receivables::InvoiceLine invoice_line;
        invoice_line.id = kInvoiceLine;
        invoice_line.invoice_id = kInvoice;
        invoice_line.product_id = kProduct;
        invoice_line.description = "Window graphics";
        invoice_line.quantity_scaled = 2000;
        invoice_line.rate_minor = 12500;
        invoice_line.amount_minor = 25000;
        invoice_line.rate_origin = engine::RateOrigin::PartySpecific;
        invoice_line.added_at = kNow - 650;
        invoice_line.added_by = kPerson;
        modules::receivables::data::save_invoice_line(tx, invoice_line);

        modules::jobs::Job job;
        job.id = kJob;
        job.party_id = kParty;
        job.state = modules::jobs::JobState::InProgress;
        job.ticket_series = "JOB";
        job.ticket_number = 7;
        job.title = "Window graphics";
        job.description = "Printed window graphics";
        job.quantity_scaled = 2000;
        job.unit_price_minor = 12500;
        job.total_price_minor = 25000;
        job.created_at = kNow - 600;
        job.created_by = kPerson;
        job.started_at = kNow - 550;
        job.started_by = kPerson;
        modules::jobs::data::save_job(tx, job);

        modules::quotations::Quotation quotation;
        quotation.id = kQuotation;
        quotation.party_id = kParty;
        quotation.state = modules::quotations::QuotationState::Issued;
        quotation.current_revision = 1;
        quotation.customer_reference = "PO-77";
        quotation.created_at = kNow - 500;
        quotation.created_by = kPerson;
        modules::quotations::data::save_quotation(tx, quotation);
        modules::quotations::QuotationRevision revision;
        revision.id = kRevision;
        revision.quotation_id = kQuotation;
        revision.revision = 1;
        revision.issued = true;
        revision.series = "QT";
        revision.number = 42;
        revision.valid_until = kNow + 1000;
        revision.total_minor = 1;
        revision.created_at = kNow - 490;
        revision.created_by = kPerson;
        revision.issued_at = kNow - 480;
        revision.issued_by = kPerson;
        modules::quotations::data::save_revision(tx, revision);
        modules::quotations::QuotationLine quote_line;
        quote_line.id = kQuotationLine;
        quote_line.revision_id = kRevision;
        quote_line.quotation_id = kQuotation;
        quote_line.product_id = kProduct;
        quote_line.description = "Window graphics";
        quote_line.quantity_scaled = 2000;
        quote_line.unit_price_minor = 12500;
        quote_line.amount_minor = 25000;
        quote_line.rate_origin = engine::RateOrigin::PartySpecific;
        modules::quotations::data::save_line(tx, quote_line);

        modules::agreements::Agreement agreement;
        agreement.id = kAgreement;
        agreement.party_id = kParty;
        agreement.state = modules::agreements::AgreementState::Open;
        agreement.valid_from = kNow - 1000;
        agreement.valid_until = kNow + 100000;
        agreement.customer_reference = "Annual plan";
        agreement.created_at = kNow - 470;
        agreement.created_by = kPerson;
        agreement.opened_at = kNow - 460;
        agreement.opened_by = kPerson;
        modules::agreements::data::save_agreement(tx, agreement);
        modules::agreements::AgreementLine agreement_line;
        agreement_line.id = kAgreementLine;
        agreement_line.agreement_id = kAgreement;
        agreement_line.product_id = kProduct;
        agreement_line.agreed_name = "Window graphics";
        agreement_line.rate_minor = 9900;
        agreement_line.cap_scaled = 10000;
        agreement_line.consumed_scaled = 3000;
        modules::agreements::data::save_line(tx, agreement_line);

        modules::sourcing::SupplierProfile supplier;
        supplier.id = kSupplier;
        supplier.kind = modules::sourcing::SupplierKind::LocalDealer;
        supplier.supplies = "Vinyl";
        supplier.reliability_notes = "Same day";
        supplier.lead_time_days = 1;
        supplier.created_at = kNow - 450;
        supplier.created_by = kPerson;
        supplier.updated_at = kNow - 440;
        supplier.updated_by = kPerson;
        modules::sourcing::data::save_supplier(tx, supplier);
        modules::sourcing::Material material;
        material.id = kMaterial;
        material.name = "Vinyl";
        material.created_at = kNow - 430;
        material.created_by = kPerson;
        material.updated_at = kNow - 420;
        material.updated_by = kPerson;
        modules::sourcing::data::save_material(tx, material);
        modules::sourcing::Purchase purchase;
        purchase.id = kPurchase;
        purchase.supplier_id = kSupplier;
        purchase.material_id = kMaterial;
        purchase.purchased_at = kNow - 410;
        purchase.quantity_scaled = 5000;
        purchase.total_cost_minor = 15000;
        purchase.created_at = kNow - 409;
        purchase.created_by = kPerson;
        modules::sourcing::data::save_purchase(tx, purchase);

        modules::companion::Task task;
        task.id = kTask;
        task.kind = modules::companion::TaskKind::Reminder;
        task.title = "Call customer";
        task.note = "Confirm pickup";
        task.target = {protocol::ModuleId::jobs,
                       engine::record_id_from_string(kJob)};
        task.due_at = kNow + 1000;
        task.created_at = kNow - 405;
        task.created_by = kPerson;
        task.updated_at = kNow - 405;
        task.updated_by = kPerson;
        modules::companion::data::save_task(tx, task);
        modules::companion::TaskEvent event;
        event.id = kTaskEvent;
        event.task_id = kTask;
        event.kind = modules::companion::TaskEventKind::Created;
        event.happened_at = kNow - 405;
        event.happened_by = kPerson;
        modules::companion::data::save_event(tx, event);

        modules::files::FileAsset asset;
        asset.id = kAsset;
        asset.content_hash = std::string(64, 'a');
        asset.size_bytes = 4096;
        asset.extension = "pdf";
        asset.media_type = "application/pdf";
        asset.created_at = kNow - 400;
        asset.created_by = kPerson;
        modules::files::data::save_asset(tx, asset);
        modules::files::FileLocation location;
        location.id = kLocation;
        location.identity.device = engine::record_id_from_string(kPerson);
        location.identity.volume_id = "volume-a";
        location.identity.file_id = "file-a";
        location.asset_id = kAsset;
        location.path = "/private/top-secret/path.pdf";
        location.modified_at = kNow - 401;
        location.observed_at = kNow - 400;
        location.scan_generation = 1;
        modules::files::data::save_location(tx, location);
        modules::files::FileLink link;
        link.id = kLink;
        link.asset_id = kAsset;
        link.target = {protocol::ModuleId::jobs,
                       engine::record_id_from_string(kJob)};
        link.role = "proof";
        link.search_text = "window graphics";
        link.linked_at = kNow - 399;
        link.linked_by = kPerson;
        modules::files::data::save_link(tx, link);
    });
}
}

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;

    auto db = database();
    seed(*db);
    app::primary::LocalRecordQuery query(*db);

    t::section("sensitive fields stay out of immutable snapshots");
    auto person = query.load(app::primary::PageKind::Administration, kPerson);
    t::check(person && person.value().title == "Owner" &&
                 person.value().subtitle.find("top-secret-hash") == std::string::npos,
             "administration detail omits password hashes");
    auto file = query.load(app::primary::PageKind::Files, kAsset);
    t::check(file && file.value().lines.front().subtitle.find("/private/") ==
                         std::string::npos,
             "file detail omits trusted local paths and identities");

    t::section("exact totals are recomputed in C++");
    auto quotation = query.load(app::primary::PageKind::Quotations, kQuotation);
    t::check(quotation && !quotation.value().fields.empty() &&
                 quotation.value().fields.back().exact_minor_units == 25000,
             "quotation total ignores stale stored summary and is recomputed");
    auto order = query.load(app::primary::PageKind::Orders, kOrder);
    t::check(order && order.value().fields.back().exact_minor_units == 25000,
             "order total is computed from exact line arithmetic");

    t::section("bounded lines history and actions are projected per module");
    auto agreement = query.load(app::primary::PageKind::Agreements, kAgreement);
    t::check(agreement && agreement.value().lines.size() == 1 &&
                 agreement.value().actions.size() >= 2,
             "agreement detail exposes typed lines and actions");
    auto task = query.load(app::primary::PageKind::Companion, kTask);
    t::check(task && task.value().history.size() == 1,
             "task history is projected from stored events");

    t::section("missing records and unavailable storage fail visibly");
    t::check(!query.load(app::primary::PageKind::Parties,
                         "ffffffffffffffffffffffffffffffff") &&
                 query.load(app::primary::PageKind::Parties,
                            "ffffffffffffffffffffffffffffffff").error().code ==
                     app::DomainErrorCode::NotFound,
             "missing record stays not found");
    db->close();
    t::check(!query.load(app::primary::PageKind::Parties, kParty),
             "closed database does not masquerade as an empty record");

    return t::report();
}
