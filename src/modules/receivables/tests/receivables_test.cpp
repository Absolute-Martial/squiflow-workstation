#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/parties/module.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace parties = squiflow::modules::parties;
namespace recv = squiflow::modules::receivables;
namespace protocol = squiflow::protocol;

namespace {
std::atomic<std::int64_t> g_now{1'700'000'000'000};
std::int64_t now() { return g_now.fetch_add(1000) + 1000; }
std::atomic<int> g_key{0};
std::string key() { return "recv-key-" + std::to_string(g_key.fetch_add(1) + 1); }

const std::string kParty = "51000000000000000000000000000001";
const std::string kPerson = "51000000000000000000000000000002";
const std::string kDraft = "52000000000000000000000000000001";
const std::string kInvoice = "52000000000000000000000000000002";
const std::string kLine = "53000000000000000000000000000001";
const std::string kPayment = "54000000000000000000000000000001";
const std::string kAllocation = "55000000000000000000000000000001";

engine::Blob payload(
    std::initializer_list<std::pair<std::string, std::string>> texts,
    std::initializer_list<std::pair<std::string, std::int64_t>> numbers = {}) {
    engine::Row row;
    for (const auto& [name, value] : texts) row.set(name, engine::Value::text(value));
    for (const auto& [name, value] : numbers) row.set(name, engine::Value::integer(value));
    return engine::encode_payload(row);
}

engine::Session owner() {
    engine::Session session;
    session.person = engine::record_id_from_string(kPerson);
    session.device = engine::RecordId{1, 1};
    session.display_name = "Owner";
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;
    Shop() {
        registry.add(parties::make_module(now));
        registry.add(recv::make_module(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
        database->write([](engine::Transaction& tx) {
            parties::Party party;
            party.id = kParty;
            party.display_name = "Acme Organization";
            party.kind = parties::PartyKind::Organisation;
            party.is_customer = true;
            party.created_at = 1000;
            party.updated_at = 1000;
            party.created_by = kPerson;
            party.updated_by = kPerson;
            parties::data::save_party(tx, party);
        });
    }
    modules::Outcome run(protocol::OperationId operation, const std::string& record,
                         const engine::Blob& body, const engine::Session& session,
                         engine::ConnectionState connection = engine::ConnectionState::Online,
                         bool idempotent = true) {
        modules::Call call;
        call.operation = operation;
        call.record_id = record;
        call.payload = body;
        if (protocol::operation(operation).sync_class ==
            protocol::OperationClass::Synchronizable) {
            call.idempotency_key = idempotent ? key() : std::string{};
        }
        return registry.run(*database, call, session, connection);
    }
    template <typename Fn> void write(Fn&& fn) { database->write(std::forward<Fn>(fn)); }
    template <typename Fn> void read(Fn&& fn) const {
        database->read([&](const engine::Store& store) { fn(store); });
    }
};

void seed_issued_and_payment(Shop& shop) {
    shop.write([](engine::Transaction& tx) {
        recv::Invoice invoice;
        invoice.id = kInvoice;
        invoice.party_id = kParty;
        invoice.state = engine::DocumentState::Issued;
        invoice.number_series = "INV";
        invoice.number = 7;
        invoice.due_at = 1'700'100'000'000;
        invoice.created_at = 1'699'999'000'000;
        invoice.created_by = kPerson;
        invoice.issued_at = 1'700'000'000'000;
        invoice.issued_by = kPerson;
        recv::data::save_invoice(tx, invoice);

        recv::InvoiceLine line;
        line.id = kLine;
        line.invoice_id = kInvoice;
        line.description = "Issued banner";
        line.quantity_scaled = 1000;
        line.rate_minor = 10000;
        line.amount_minor = 10000;
        line.rate_origin = engine::RateOrigin::CatalogDefault;
        line.added_at = 1'699'999'000'000;
        line.added_by = kPerson;
        recv::data::save_invoice_line(tx, line);

        recv::Payment payment;
        payment.id = kPayment;
        payment.party_id = kParty;
        payment.amount_minor = 10000;
        payment.paid_at = 1'700'000'001'000;
        payment.method = "cash";
        payment.receipt_series = "RCPT";
        payment.receipt_number = 8;
        payment.recorded_at = 1'700'000'002'000;
        payment.recorded_by = kPerson;
        recv::data::save_payment(tx, payment);
    });
}
}  // namespace

int main() {
    const engine::Session session = owner();

    section("migration 15 and exact operation surface");
    {
        Shop shop;
        check(shop.registry.handled(protocol::OperationId::invoice_draft_create), "draft create handled");
        check(shop.registry.handled(protocol::OperationId::invoice_draft_update), "draft update handled");
        check(shop.registry.handled(protocol::OperationId::invoice_draft_discard), "draft discard handled");
        check(shop.registry.handled(protocol::OperationId::payment_allocate), "allocation handled");
        check(shop.registry.handled(protocol::OperationId::credit_account_set), "credit handled");
        check(shop.registry.handled(protocol::OperationId::statement_prepare), "prepare handled");
        check(shop.registry.handled(protocol::OperationId::statement_send), "send handled");
        check(shop.registry.handled(protocol::OperationId::document_print), "print handled");
        shop.read([](const engine::Store& store) {
            check(store.has_table(recv::tables::kInvoice), "invoice table exists");
            check(store.has_table(recv::tables::kAllocation), "allocation table exists");
            check(store.has_table(recv::tables::kCreditAccount), "credit table exists");
            check(store.has_table(recv::tables::kStatementDelivery), "delivery table exists");
        });
    }

    section("draft create update line and discard");
    {
        Shop shop;
        const modules::Outcome made = shop.run(
            protocol::OperationId::invoice_draft_create, kDraft,
            payload({{"party_id", kParty}, {"note", "draft"}}, {{"due_at", 1'800'000'000'000}}),
            session, engine::ConnectionState::Offline);
        check(made.ok && made.queued, "draft create works offline and queues");
        const engine::Blob line_body = payload(
            {{"action", "line_upsert"}, {"line_id", kLine},
             {"description", "Off catalog banner"}, {"rate_reason", "Owner quote"}},
            {{"quantity_scaled", 2500}, {"rate_minor", 10001}, {"rate_origin", 3}});
        check(shop.run(protocol::OperationId::invoice_draft_update, kDraft,
                       line_body, session, engine::ConnectionState::Offline).ok,
              "draft line upsert works offline");
        shop.read([](const engine::Store& store) {
            const auto line = recv::data::find_invoice_line(store, kLine);
            check(line && line->amount_minor == 25003, "line amount is checked and rounded");
            const engine::MoneyResult total = recv::data::outstanding_for_invoice(store, kDraft);
            check(total.ok && total.value.minor == 25003, "draft total is derived");
        });
        const modules::Outcome print = shop.run(
            protocol::OperationId::document_print, {},
            payload({{"document_type", "invoice"}, {"document_id", kDraft}}), session);
        check(!print.ok, "draft cannot be printed as issued evidence");
        check(shop.run(protocol::OperationId::invoice_draft_discard, kDraft, {}, session,
                       engine::ConnectionState::Offline).ok,
              "draft discard works offline");
        check(!shop.run(protocol::OperationId::invoice_draft_update, kDraft,
                        payload({{"action", "metadata"}, {"note", "rewrite"}}), session).ok,
              "discarded draft is frozen");
    }

    section("manual allocation is bounded and releasable");
    {
        Shop shop;
        seed_issued_and_payment(shop);
        const engine::Blob allocate = payload(
            {{"action", "allocate"}, {"allocation_id", kAllocation},
             {"target_module", "invoice"}, {"target_id", kInvoice}},
            {{"amount_minor", 6000}});
        check(shop.run(protocol::OperationId::payment_allocate, kPayment, allocate, session,
                       engine::ConnectionState::Offline).ok,
              "manual allocation works offline");
        check(!shop.run(protocol::OperationId::payment_allocate, kPayment,
                        payload({{"action", "allocate"},
                                 {"allocation_id", "55000000000000000000000000000002"},
                                 {"target_module", "invoice"}, {"target_id", kInvoice}},
                                {{"amount_minor", 5000}}), session).ok,
              "over-allocation is refused transactionally");
        check(shop.run(protocol::OperationId::payment_allocate, kPayment,
                       payload({{"action", "release"}, {"allocation_id", kAllocation},
                                {"reason", "Invoice correction"}}), session).ok,
              "allocation release preserves evidence");
        shop.read([](const engine::Store& store) {
            const auto allocation = recv::data::find_allocation(store, kAllocation);
            check(allocation && allocation->state == recv::AllocationState::Released,
                  "released row remains stored");
            const auto payment = recv::data::find_payment(store, kPayment);
            const engine::MoneyResult available = recv::unallocated_amount(
                *payment, recv::data::allocations_for_payment(store, kPayment));
            check(available.ok && available.value.minor == 10000,
                  "released money becomes unallocated");
        });
    }

    section("credit accounts require online customer organizations");
    {
        Shop shop;
        const engine::Blob terms = payload({}, {{"credit_limit_minor", 500000},
                                                  {"credit_period_days", 30},
                                                  {"cycle_day", 31}});
        check(!shop.run(protocol::OperationId::credit_account_set, kParty, terms, session,
                        engine::ConnectionState::Offline).ok,
              "credit terms are online-only");
        check(shop.run(protocol::OperationId::credit_account_set, kParty, terms, session).ok,
              "organization credit terms save online");
        shop.read([](const engine::Store& store) {
            const auto account = recv::data::find_credit_account(store, kParty);
            check(account && account->credit_limit_minor == 500000 && account->cycle_day == 31,
                  "credit terms persist exactly");
        });
    }

    section("statements print data and sending needs confirmation evidence");
    {
        Shop shop;
        seed_issued_and_payment(shop);
        check(shop.run(protocol::OperationId::payment_allocate, kPayment,
                       payload({{"action", "allocate"}, {"allocation_id", kAllocation},
                                {"target_module", "invoice"}, {"target_id", kInvoice}},
                               {{"amount_minor", 4000}}), session).ok,
              "statement fixture allocation succeeds");
        const engine::Blob request = payload(
            {{"statement_id", "statement-a"}, {"party_id", kParty}},
            {{"period_from", 1},
             {"period_through", g_now.load(std::memory_order_relaxed)}});
        const modules::Outcome statement = shop.run(
            protocol::OperationId::statement_prepare, {}, request, session,
            engine::ConnectionState::Offline);
        check(statement.ok && statement.rows.size() >= 4, "statement prepares offline");
        check(statement.ok && statement.rows.front().get("outstanding_minor").integer_or(-1) == 6000,
              "statement outstanding reconciles");
        const modules::Outcome invoice_print = shop.run(
            protocol::OperationId::document_print, {},
            payload({{"document_type", "invoice"}, {"document_id", kInvoice}}), session,
            engine::ConnectionState::Offline);
        check(invoice_print.ok && invoice_print.rows.size() == 2,
              "issued invoice print model includes lines");
        const modules::Outcome receipt_print = shop.run(
            protocol::OperationId::document_print, {},
            payload({{"document_type", "receipt"}, {"document_id", kPayment}}), session);
        check(receipt_print.ok && receipt_print.rows.size() == 1,
              "receipt is reproducible");
        check(!shop.run(protocol::OperationId::statement_send, {},
                        payload({{"delivery_id", "delivery-bad"},
                                 {"statement_id", "statement-a"},
                                 {"recipient", "accounts@example.test"},
                                 {"content_hash", "sha256:x"}}), session).ok,
              "send without transport confirmation is refused");
        check(shop.run(protocol::OperationId::statement_send, {},
                       payload({{"delivery_id", "delivery-good"},
                                {"statement_id", "statement-a"},
                                {"recipient", "accounts@example.test"},
                                {"content_hash", "sha256:x"},
                                {"transport_reference", "message-1"}}), session).ok,
              "confirmed send evidence persists");
        check(!shop.run(protocol::OperationId::statement_send, {},
                        payload({{"delivery_id", "delivery-good"},
                                 {"statement_id", "statement-a"},
                                 {"recipient", "attacker@example.test"},
                                 {"content_hash", "sha256:changed"},
                                 {"transport_reference", "message-rewritten"}}), session).ok,
              "confirmed delivery evidence cannot be overwritten");
        shop.read([](const engine::Store& store) {
            const auto delivery = recv::data::find_statement_delivery(
                store, "delivery-good");
            check(delivery && delivery->recipient == "accounts@example.test" &&
                      delivery->content_hash == "sha256:x" &&
                      delivery->transport_reference == "message-1",
                  "the first confirmed delivery evidence remains exact");
        });
        check(!shop.run(protocol::OperationId::statement_send, {},
                        payload({{"delivery_id", "delivery-offline"},
                                 {"statement_id", "statement-a"},
                                 {"recipient", "accounts@example.test"},
                                 {"content_hash", "sha256:x"},
                                 {"transport_reference", "message-2"}}), session,
                        engine::ConnectionState::Offline).ok,
              "statement send is online-only");
    }

    section("rights malformed payloads and idempotency fail safely");
    {
        Shop shop;
        engine::Session unsigned_session;
        unsigned_session.rights.grant_all();
        check(!shop.run(protocol::OperationId::invoice_draft_create, kDraft,
                        payload({{"party_id", kParty}}), unsigned_session).ok,
              "rights do not replace authentication");
        const modules::Outcome malformed = shop.run(
            protocol::OperationId::invoice_draft_create, kDraft,
            engine::Blob{1, 2, 3, 4}, session);
        check(!malformed.ok && malformed.error ==
                  "This request could not be read. Please try it again.",
              "malformed payload exposes no decoder internals");
        modules::Call call;
        call.operation = protocol::OperationId::invoice_draft_create;
        call.record_id = kDraft;
        call.payload = payload({{"party_id", kParty}});
        call.idempotency_key = "same-key";
        const modules::Outcome first = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Online);
        const modules::Outcome replay = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Online);
        check(first.ok && replay.ok && replay.replayed, "synchronizable replay is harmless");
    }

    return squiflow::testing::report();
}
