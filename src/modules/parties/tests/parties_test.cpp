// Parties, driven through the registry with a session.
// Every operation is Synchronizable, so every call needs an idempotency key.

#include <memory>
#include <string>

#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/parties/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace parties  = squiflow::modules::parties;
namespace engine   = squiflow::engine;
namespace modules  = squiflow::modules;
namespace protocol = squiflow::protocol;

namespace {

std::int64_t g_now = 1'700'000'000'000;
std::int64_t now() { return g_now += 1000; }

int g_key_counter = 0;
std::string next_key() {
    return "k-parties-" + std::to_string(++g_key_counter);
}

engine::Blob text_fields(
    std::initializer_list<std::pair<std::string, std::string>> text_pairs,
    std::initializer_list<std::pair<std::string, bool>> bool_pairs = {},
    std::initializer_list<std::pair<std::string, std::int64_t>> int_pairs = {}) {
    engine::Row row;
    for (const auto& p : text_pairs) row.set(p.first, engine::Value::text(p.second));
    for (const auto& p : bool_pairs) row.set(p.first, engine::Value::boolean(p.second));
    for (const auto& p : int_pairs)  row.set(p.first, engine::Value::integer(p.second));
    return engine::encode_payload(row);
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(parties::make_module(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
    }

    // Always supply a key: all five parties operations are Synchronizable.
    modules::Outcome run(protocol::OperationId op, const std::string& record,
                         const engine::Blob& payload, const engine::Session& session,
                         const std::string& key) {
        modules::Call call;
        call.operation       = op;
        call.record_id       = record;
        call.payload         = payload;
        call.idempotency_key = key;
        return registry.run(*database, call, session, engine::ConnectionState::Online);
    }

    template <typename Fn>
    void look(Fn&& fn) const {
        database->read([&](const engine::Store& store) { fn(store); });
    }
};

const std::string kAcme    = "10000000000000000000000000000001";
const std::string kPrinter = "10000000000000000000000000000002";
const std::string kContact = "10000000000000000000000000000003";

engine::Session owner_session() {
    engine::Session session;
    session.person       = engine::record_id_from_string("00000000000000000000000000000001");
    session.device       = engine::RecordId{1, 1};
    session.display_name = "Shopkeeper";
    session.is_owner     = true;
    session.rights.grant_all();
    return session;
}

}  // namespace

int main() {
    section("a party is created and can be found");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const modules::Outcome created = shop.run(
            protocol::OperationId::party_create, kAcme,
            text_fields({{"display_name", "Acme Print Supplies"}, {"kind", "organisation"}},
                        {{"is_supplier", true}, {"is_customer", false}}),
            owner, next_key());
        check(created.ok, "party was created");
        check(created.queued, "party_create is synchronisable so it was queued");

        shop.look([&](const engine::Store& store) {
            const auto party = parties::data::find_party(store, kAcme);
            check(party.has_value(), "found in the store");
            check(party->display_name == "Acme Print Supplies", "name is right");
            check(party->kind == parties::PartyKind::Organisation, "kind stored");
            check(party->is_supplier, "is a supplier");
            check(!party->is_customer, "not a customer");
            check(!party->archived, "not archived on creation");
        });
    }

    section("a party cannot be created twice, but a replay is fine");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const std::string first_key = next_key();
        shop.run(protocol::OperationId::party_create, kAcme,
                 text_fields({{"display_name", "Acme"}}), owner, first_key);

        const modules::Outcome dup = shop.run(
            protocol::OperationId::party_create, kAcme,
            text_fields({{"display_name", "Acme again"}}), owner, next_key());
        check(!dup.ok, "creating with the same id but a different key is refused");

        const modules::Outcome replay = shop.run(
            protocol::OperationId::party_create, kAcme,
            text_fields({{"display_name", "Acme"}}), owner, first_key);
        check(replay.ok, "replaying the exact same key is accepted");
        check(replay.replayed, "and flagged as a replay, not a real write");
    }

    section("a party with no name is refused");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(!shop.run(protocol::OperationId::party_create, kAcme,
                        text_fields({{"display_name", "   "}}), owner, next_key()).ok,
              "blank name refused");
        check(!shop.run(protocol::OperationId::party_create, kAcme,
                        text_fields({{"display_name", ""}}), owner, next_key()).ok,
              "empty name refused");
    }

    section("neither customer nor supplier is refused");
    {
        Shop shop;
        check(!shop.run(protocol::OperationId::party_create, kAcme,
                        text_fields({{"display_name", "Acme"}},
                                    {{"is_customer", false}, {"is_supplier", false}}),
                        owner_session(), next_key()).ok,
              "must be at least one");
    }

    section("a party can be updated, but not when archived");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        shop.run(protocol::OperationId::party_create, kAcme,
                 text_fields({{"display_name", "Old name"}}), owner, next_key());

        check(shop.run(protocol::OperationId::party_update, kAcme,
                       text_fields({{"display_name", "New name"}}), owner, next_key()).ok,
              "name updated");

        const std::string archive_key = next_key();
        check(shop.run(protocol::OperationId::party_archive, kAcme, {}, owner, archive_key).ok,
              "archived");
        check(shop.run(protocol::OperationId::party_archive, kAcme, {}, owner, archive_key).ok,
              "archiving with the same key is a replay, not an error");

        check(!shop.run(protocol::OperationId::party_update, kAcme,
                        text_fields({{"display_name", "After archive"}}), owner, next_key()).ok,
              "updating an archived party is refused");

        shop.look([&](const engine::Store& store) {
            const auto party = parties::data::find_party(store, kAcme);
            check(party.has_value() && party->display_name == "New name",
                  "the name stayed at 'New name', not overwritten by the refused update");
        });
    }

    section("billing terms are per customer");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        shop.run(protocol::OperationId::party_create, kAcme,
                 text_fields({{"display_name", "Acme"}}), owner, next_key());
        shop.run(protocol::OperationId::party_create, kPrinter,
                 text_fields({{"display_name", "Printer Co"}},
                             {{"is_customer", false}, {"is_supplier", true}}),
                 owner, next_key());

        const modules::Outcome terms = shop.run(
            protocol::OperationId::party_terms_set, kAcme,
            text_fields({{"billing", "credit_account"}, {"customer_ref", "PO-999"}},
                        {}, {{"net_days", 30}}),
            owner, next_key());
        check(terms.ok, "terms set on a customer");
        check(terms.queued, "terms change is synchronisable");

        shop.look([&](const engine::Store& store) {
            const auto party = parties::data::find_party(store, kAcme);
            check(party.has_value() &&
                  party->terms.arrangement == parties::BillingArrangement::CreditAccount,
                  "billing arrangement stored");
            check(party->terms.net_days == 30, "net days stored");
            check(party->terms.customer_reference == "PO-999", "customer reference stored");
        });

        check(!shop.run(protocol::OperationId::party_terms_set, kPrinter,
                        text_fields({{"billing", "credit_account"}}), owner, next_key()).ok,
              "billing terms refused on a non-customer");

        check(!shop.run(protocol::OperationId::party_terms_set, kAcme,
                        text_fields({}, {}, {{"net_days", -1}}), owner, next_key()).ok,
              "negative net days refused");
    }

    section("contact information is stored per party");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        shop.run(protocol::OperationId::party_create, kAcme,
                 text_fields({{"display_name", "Acme"}}), owner, next_key());

        const modules::Outcome added = shop.run(
            protocol::OperationId::party_contact_add, kContact,
            text_fields({{"party_id", kAcme}, {"label", "phone"}, {"value", "+977-1-4000001"}}),
            owner, next_key());
        check(added.ok, "contact added");
        check(added.queued, "contact add is synchronisable");

        shop.look([&](const engine::Store& store) {
            const auto contacts = parties::data::contacts_of(store, kAcme);
            check(contacts.size() == 1, "one contact");
            check(contacts.front().label == "phone", "label");
            check(contacts.front().value == "+977-1-4000001", "value");
        });

        const std::string kUnknown = "10000000000000000000000000000099";
        check(!shop.run(protocol::OperationId::party_contact_add,
                        "10000000000000000000000000000004",
                        text_fields({{"party_id", kUnknown}, {"label", "email"}, {"value", "x@y"}}),
                        owner, next_key()).ok,
              "contact for unknown party refused");

        check(!shop.run(protocol::OperationId::party_contact_add,
                        "10000000000000000000000000000005",
                        text_fields({{"party_id", kAcme}, {"value", "x"}}),
                        owner, next_key()).ok,
              "no label refused");
        check(!shop.run(protocol::OperationId::party_contact_add,
                        "10000000000000000000000000000006",
                        text_fields({{"party_id", kAcme}, {"label", "email"}}),
                        owner, next_key()).ok,
              "no value refused");
    }

    section("an unknown party is refused on every operation");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const std::string kGhost = "10000000000000000000000000000099";
        check(!shop.run(protocol::OperationId::party_update, kGhost,
                        text_fields({{"display_name", "Ghost"}}), owner, next_key()).ok,
              "update");
        check(!shop.run(protocol::OperationId::party_archive, kGhost, {}, owner, next_key()).ok,
              "archive");
        check(!shop.run(protocol::OperationId::party_terms_set, kGhost,
                        text_fields({{"billing", "pay_per_job"}}), owner, next_key()).ok,
              "terms");
    }

    return squiflow::testing::report();
}
