#include <cstdint>
#include <memory>
#include <string>

#include "engine/audit/audit_log.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/agreements/data/repository.hpp"
#include "modules/agreements/module.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/parties/module.hpp"
#include "modules/pricing/module.hpp"
#include "modules/quotations/data/repository.hpp"
#include "modules/quotations/module.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"
#include "workflows/document_delivery.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace e = squiflow::engine;
namespace m = squiflow::modules;
namespace a = squiflow::modules::agreements;
namespace parties = squiflow::modules::parties;
namespace q = squiflow::modules::quotations;
namespace r = squiflow::modules::receivables;
namespace p = squiflow::protocol;
namespace w = squiflow::workflows;

namespace {
constexpr std::int64_t Now = 1'940'000'000'000LL;
std::int64_t now() { return Now; }
const std::string Person = "c1000000000000000000000000000001";
const std::string Device = "c1000000000000000000000000000002";
const std::string Party = "c2000000000000000000000000000001";
const std::string OtherParty = "c2000000000000000000000000000002";
const std::string Contact = "c3000000000000000000000000000001";
const std::string BadContact = "c3000000000000000000000000000002";
const std::string Invoice = "c4000000000000000000000000000001";
const std::string DraftInvoice = "c4000000000000000000000000000002";
const std::string Quote = "c5000000000000000000000000000001";
const std::string Revision = "c6000000000000000000000000000001";
const std::string Agreement = "c7000000000000000000000000000001";
const std::string Delivery = "c8000000000000000000000000000001";
const std::string Hash(64U, 'a');

e::Session owner() {
    e::Session session;
    session.person = e::record_id_from_string(Person);
    session.device = e::record_id_from_string(Device);
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

e::Blob prepare_body(const std::string& type = "invoice",
                     const std::string& document = Invoice,
                     const std::string& version = {},
                     const std::string& contact = Contact,
                     const std::string& channel = "email",
                     const std::string& subject = {},
                     const std::string& body = {},
                     bool approval = true) {
    e::Row row;
    row.set("party_id", e::Value::text(Party));
    row.set("document_type", e::Value::text(type));
    row.set("document_id", e::Value::text(document));
    if (!version.empty()) row.set("document_version_id", e::Value::text(version));
    row.set("recipient_contact_id", e::Value::text(contact));
    row.set("channel", e::Value::text(channel));
    row.set("transport_profile_id", e::Value::text("primary-email"));
    if (!subject.empty()) row.set("subject", e::Value::text(subject));
    if (!body.empty()) row.set("message_body", e::Value::text(body));
    row.set("attachment_name", e::Value::text("reviewed-document.pdf"));
    row.set("content_sha256", e::Value::text(Hash));
    row.set("approval_requested", e::Value::boolean(approval));
    return e::encode_payload(row);
}

e::Blob request_body(const std::string& token) {
    e::Row row;
    row.set("confirmation_token", e::Value::text(token));
    return e::encode_payload(row);
}

struct Shop {
    m::Registry registry{now};
    std::unique_ptr<e::Database> database;

    Shop() {
        registry.add(parties::make_module(now));
        registry.add(m::pricing::make_module(now));
        registry.add(r::make_module(now));
        registry.add(q::make_module(now));
        registry.add(a::make_module(now));
        registry.install_workflow(w::make_prepare_document_delivery(now));
        registry.install_workflow(w::make_request_document_delivery(now));
        e::MigrationRunner migrations{now};
        registry.collect_migrations(migrations);
        database = std::make_unique<e::Database>(
            std::make_unique<e::MemoryStore>(), std::move(migrations));
        database->open();
        seed();
    }

    void seed() {
        database->write([](e::Transaction& transaction) {
            parties::Party party;
            party.id = Party;
            party.display_name = "Delivery Customer";
            party.created_at = Now - 100;
            party.updated_at = Now - 100;
            party.created_by = Person;
            party.updated_by = Person;
            parties::data::save_party(transaction, party);
            parties::Party other = party;
            other.id = OtherParty;
            other.display_name = "Other Customer";
            parties::data::save_party(transaction, other);
            parties::ContactInfo email;
            email.id = Contact;
            email.party_id = Party;
            email.label = "email";
            email.value = "customer@example.com";
            email.added_at = Now - 90;
            parties::data::save_contact(transaction, email);
            parties::ContactInfo bad = email;
            bad.id = BadContact;
            bad.value = "victim@example.com\r\nBcc: thief@example.com";
            parties::data::save_contact(transaction, bad);

            r::Invoice invoice;
            invoice.id = Invoice;
            invoice.party_id = Party;
            invoice.state = e::DocumentState::Issued;
            invoice.number_series = "INV";
            invoice.number = 7;
            invoice.created_at = Now - 80;
            invoice.created_by = Person;
            invoice.issued_at = Now - 70;
            invoice.issued_by = Person;
            r::data::save_invoice(transaction, invoice);
            r::Invoice draft = invoice;
            draft.id = DraftInvoice;
            draft.state = e::DocumentState::Draft;
            draft.number_series.clear();
            draft.number = 0;
            draft.issued_at = 0;
            draft.issued_by.clear();
            r::data::save_invoice(transaction, draft);

            q::Quotation quote;
            quote.id = Quote;
            quote.party_id = Party;
            quote.state = q::QuotationState::Issued;
            quote.created_at = Now - 80;
            quote.created_by = Person;
            q::data::save_quotation(transaction, quote);
            q::QuotationRevision revision;
            revision.id = Revision;
            revision.quotation_id = Quote;
            revision.issued = true;
            revision.series = "Q";
            revision.number = 4;
            revision.created_at = Now - 80;
            revision.created_by = Person;
            revision.issued_at = Now - 70;
            revision.issued_by = Person;
            q::data::save_revision(transaction, revision);

            a::Agreement agreement;
            agreement.id = Agreement;
            agreement.party_id = Party;
            agreement.state = a::AgreementState::Draft;
            agreement.valid_from = Now;
            agreement.created_at = Now - 60;
            agreement.created_by = Person;
            a::data::save_agreement(transaction, agreement);
        });
    }

    m::Outcome run(p::OperationId operation, const std::string& id,
                   const std::string& key, const e::Blob& body,
                   const e::Session& session = owner(),
                   e::ConnectionState connection = e::ConnectionState::Online) {
        m::Call call;
        call.operation = operation;
        call.record_id = id;
        call.idempotency_key = key;
        call.payload = body;
        return registry.run(*database, call, session, connection);
    }
};

std::size_t count(const Shop& shop, const std::string& table) {
    std::size_t result = 0;
    shop.database->read([&](const e::Store& store) { result = store.count(table); });
    return result;
}

std::string token(const Shop& shop, const std::string& id = Delivery) {
    std::string result;
    shop.database->read([&](const e::Store& store) {
        const auto delivery = r::data::find_document_delivery(store, id);
        if (delivery) result = delivery->confirmation_token;
    });
    return result;
}
}  // namespace

int main() {
    section("optional message is prepared locally and sends nothing");
    {
        Shop shop;
        const auto result = shop.run(p::OperationId::prepare_document_delivery,
                                     Delivery, "prepare", prepare_body());
        check(result.ok && result.queued, "prepared delivery is synchronized");
        shop.database->read([](const e::Store& store) {
            const auto delivery = r::data::find_document_delivery(store, Delivery);
            check(delivery && delivery->state == r::DeliveryState::Prepared,
                  "delivery remains prepared");
            check(delivery && delivery->subject.empty() && delivery->message_body.empty(),
                  "subject and message body are optional");
            check(delivery && delivery->approval_requested,
                  "approval purpose is retained without deciding approval");
            check(delivery && delivery->recipient == "customer@example.com",
                  "recipient comes from the selected customer contact");
            check(delivery && !delivery->confirmation_token.empty(),
                  "review token freezes exact content");
        });
        check(count(shop, e::AuditLog::table_name()) == 1,
              "preparation leaves one audit entry");
    }

    section("only an online confirmed request reaches the remote outbox");
    {
        Shop shop;
        check(shop.run(p::OperationId::prepare_document_delivery, Delivery,
                       "p2", prepare_body("invoice", Invoice, {}, Contact, "email",
                                          "Please approve", "Optional note"))
                  .ok,
              "message prepared");
        const std::string reviewed = token(shop);
        check(!shop.run(p::OperationId::request_document_delivery, Delivery,
                        "offline", request_body(reviewed), owner(),
                        e::ConnectionState::Offline)
                   .ok,
              "offline sending is refused before writes");
        check(!shop.run(p::OperationId::request_document_delivery, Delivery,
                        "stale", request_body("stale-token"))
                   .ok,
              "stale review token is refused");
        const auto sent = shop.run(p::OperationId::request_document_delivery,
                                   Delivery, "send-once", request_body(reviewed));
        check(sent.ok && sent.queued, "online request is queued for remote backend");
        shop.database->read([](const e::Store& store) {
            const auto delivery = r::data::find_document_delivery(store, Delivery);
            check(delivery && delivery->state == r::DeliveryState::Requested,
                  "local state records a backend request, not transport success");
            check(delivery && delivery->request_idempotency_key == "send-once",
                  "remote request retains its idempotency key");
            check(delivery && delivery->transport_reference.empty() &&
                      delivery->accepted_at == 0,
                  "request does not forge backend acceptance");
            check(delivery && delivery->approval_requested,
                  "delivery still does not mark the document approved");
        });
        const auto replay = shop.run(p::OperationId::request_document_delivery,
                                     Delivery, "send-once", request_body(reviewed));
        check(replay.ok && replay.replayed, "same-key retry cannot duplicate the email");
        check(!shop.run(p::OperationId::request_document_delivery, Delivery,
                        "send-twice", request_body(reviewed))
                   .ok,
              "different-key double send is refused");
    }

    section("unsafe contacts channels rights and sources fail closed");
    {
        Shop shop;
        check(!shop.run(p::OperationId::prepare_document_delivery,
                        "c8000000000000000000000000000002", "bad-email",
                        prepare_body("invoice", Invoice, {}, BadContact))
                   .ok,
              "header-injection email is refused");
        check(!shop.run(p::OperationId::prepare_document_delivery,
                        "c8000000000000000000000000000003", "whatsapp",
                        prepare_body("invoice", Invoice, {}, Contact, "whatsapp"))
                   .ok,
              "WhatsApp remains explicitly reserved for the future");
        check(!shop.run(p::OperationId::prepare_document_delivery,
                        "c8000000000000000000000000000004", "draft",
                        prepare_body("invoice", DraftInvoice))
                   .ok,
              "draft invoice cannot leave the shop");
        e::Session denied = owner();
        denied.is_owner = false;
        denied.rights.revoke(p::RightId::right_document_send);
        check(!shop.run(p::OperationId::prepare_document_delivery,
                        "c8000000000000000000000000000005", "denied",
                        prepare_body(), denied)
                   .ok,
              "document-send right is required");
        check(count(shop, r::tables::kDocumentDelivery) == 0,
              "all refused preparations leave no delivery rows");
        check(count(shop, e::AuditLog::table_name()) == 0,
              "all refused preparations leave no audit rows");
    }

    section("quotation revision and agreement approval requests are exact");
    {
        Shop shop;
        check(shop.run(p::OperationId::prepare_document_delivery,
                       "c8000000000000000000000000000006", "quote",
                       prepare_body("quotation", Quote, Revision))
                  .ok,
              "issued quotation revision can be prepared for approval");
        check(!shop.run(p::OperationId::prepare_document_delivery,
                        "c8000000000000000000000000000007", "quote-unpinned",
                        prepare_body("quotation", Quote))
                   .ok,
              "quotation without exact revision is refused");
        check(shop.run(p::OperationId::prepare_document_delivery,
                       "c8000000000000000000000000000008", "agreement",
                       prepare_body("agreement", Agreement))
                  .ok,
              "draft agreement may be sent for customer approval");
        check(count(shop, r::tables::kDocumentDelivery) == 2,
              "only exact supported documents were prepared");
    }

    section("protocol makes preparation optional and sending online only");
    {
        const auto& prepare = p::operation(p::OperationId::prepare_document_delivery);
        const auto& request = p::operation(p::OperationId::request_document_delivery);
        check(prepare.offline == p::OfflineRule::OfflineAllowed,
              "preparation is available offline");
        check(request.offline == p::OfflineRule::OnlineOnly,
              "remote send request is online-only");
        check(prepare.right == p::RightId::right_document_send &&
                  request.right == p::RightId::right_document_send,
              "both operations use the dedicated send right");
    }

    return squiflow::testing::report();
}
