// Catalog module  - driven entirely through the registry.
//
// The catalog manages products. A product is identified by the id the caller
// supplies; the name is a display label, not an identity. Two products can
// share a name  - that is not this module's problem. What this module refuses:
//   - a product with a blank name
//   - creating a product whose id already exists (use a different key to prove
//     it is a new product, not a replay)
//   - updating or archiving a product that is not in the catalog
//   - updating an archived product
//
// All three operations are Synchronizable, so every call needs an
// idempotency key. A replay (same key) on an operation that was already
// applied is accepted and flagged rather than refused.

#include <memory>
#include <string>

#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/catalog/data/repository.hpp"
#include "modules/catalog/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace catalog   = squiflow::modules::catalog;
namespace engine    = squiflow::engine;
namespace modules   = squiflow::modules;
namespace protocol  = squiflow::protocol;

namespace {

std::int64_t g_now = 1'700'000'000'000;
std::int64_t now() { return g_now += 1000; }

int g_key = 0;
std::string next_key() { return "k-catalog-" + std::to_string(++g_key); }

// Build a payload carrying text and/or boolean fields.
engine::Blob fields(
    std::initializer_list<std::pair<std::string, std::string>> text_pairs,
    std::initializer_list<std::pair<std::string, bool>>        bool_pairs = {}) {
    engine::Row row;
    for (const auto& [k, v] : text_pairs) row.set(k, engine::Value::text(v));
    for (const auto& [k, v] : bool_pairs) row.set(k, engine::Value::boolean(v));
    return engine::encode_payload(row);
}

// A self-contained shop: one database, one registry, the catalog module.
struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(catalog::make_module(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
    }

    // All catalog operations are Synchronizable  - the caller must always
    // supply a key. The run() overload below enforces this.
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

    // Convenience for archive, which has no payload.
    modules::Outcome run(protocol::OperationId op, const std::string& record,
                         const engine::Session& session, const std::string& key) {
        return run(op, record, {}, session, key);
    }

    // Read the store inside a transaction.
    template <typename Fn>
    void look(Fn&& fn) const {
        database->read([&](const engine::Store& store) { fn(store); });
    }
};

// Product ids  - 32 hex characters, as the engine requires.
const std::string kBanner       = "20000000000000000000000000000001";
const std::string kFlexPrint    = "20000000000000000000000000000002";
const std::string kIdCard       = "20000000000000000000000000000003";
const std::string kCertificate  = "20000000000000000000000000000004";

engine::Session owner_session() {
    engine::Session session;
    session.person       = engine::record_id_from_string("00000000000000000000000000000001");
    session.device       = engine::RecordId{1, 1};
    session.display_name = "Shopkeeper";
    session.is_owner     = true;
    session.rights.grant_all();
    return session;
}

// A session that holds only the read right  - tests that the write
// operations refuse when the caller has no permission.
engine::Session reader_session() {
    engine::Session session;
    session.person       = engine::record_id_from_string("00000000000000000000000000000002");
    session.device       = engine::RecordId{1, 2};
    session.display_name = "Staff";
    session.is_owner     = false;
    session.rights.grant(protocol::RightId::right_product_read);
    return session;
}

}  // namespace

int main() {
    // -------------------------------------------------------------------
    section("a product is created and persisted");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const modules::Outcome created = shop.run(
            protocol::OperationId::product_create, kBanner,
            fields({{"name", "Banner 13oz Vinyl"},
                    {"description", "Standard outdoor banner material"}}),
            owner, next_key());

        check(created.ok,     "product was created");
        check(created.queued, "product_create is Synchronizable so it joined the outbox");

        shop.look([&](const engine::Store& store) {
            const auto product = catalog::data::find_product(store, kBanner);
            check(product.has_value(),             "product is in the store");
            check(product->name == "Banner 13oz Vinyl",
                  "name stored correctly");
            check(product->description == "Standard outdoor banner material",
                  "description stored correctly");
            check(!product->archived,              "not archived on creation");
            check(product->created_at > 0,         "created_at is set");
            check(product->updated_at == product->created_at,
                  "updated_at matches created_at on first save");
        });
    }

    // -------------------------------------------------------------------
    section("several products can be created independently");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        shop.run(protocol::OperationId::product_create, kBanner,
                 fields({{"name", "Banner 13oz Vinyl"}}), owner, next_key());
        shop.run(protocol::OperationId::product_create, kFlexPrint,
                 fields({{"name", "Flex Print"}}), owner, next_key());
        shop.run(protocol::OperationId::product_create, kIdCard,
                 fields({{"name", "PVC ID Card"}}), owner, next_key());
        shop.run(protocol::OperationId::product_create, kCertificate,
                 fields({{"name", "Certificate Paper A4"}}), owner, next_key());

        shop.look([&](const engine::Store& store) {
            const auto products = catalog::data::all_products(store);
            check(products.size() == 4, "four products in the catalog");
            // all_products returns them in name order
            check(products[0].name == "Banner 13oz Vinyl",    "first by name");
            check(products[1].name == "Certificate Paper A4", "second by name");
            check(products[2].name == "Flex Print",           "third by name");
            check(products[3].name == "PVC ID Card",          "fourth by name");
        });
    }

    // -------------------------------------------------------------------
    section("creating the same id twice is refused, but a replay is accepted");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const std::string creation_key = next_key();

        shop.run(protocol::OperationId::product_create, kBanner,
                 fields({{"name", "Banner"}}), owner, creation_key);

        // A different key with the same id means the caller believes this
        // is a new product  - refused.
        const modules::Outcome duplicate = shop.run(
            protocol::OperationId::product_create, kBanner,
            fields({{"name", "Banner (duplicate)"}}), owner, next_key());
        check(!duplicate.ok, "creating with the same id and a new key is refused");
        check(!duplicate.error.empty(), "a refusal carries a reason");

        // The original key arriving again is a replay  - accepted.
        const modules::Outcome replay = shop.run(
            protocol::OperationId::product_create, kBanner,
            fields({{"name", "Banner"}}), owner, creation_key);
        check(replay.ok,      "replaying the same key is accepted");
        check(replay.replayed,"and flagged as a replay, not a new write");

        // The catalog still has exactly one product.
        shop.look([&](const engine::Store& store) {
            const auto products = catalog::data::all_products(store);
            check(products.size() == 1, "catalog still has exactly one entry");
            check(products.front().name == "Banner", "name is from the first write");
        });
    }

    // -------------------------------------------------------------------
    section("a product with a blank or missing name is refused");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        check(!shop.run(protocol::OperationId::product_create, kBanner,
                        fields({{"name", ""}}), owner, next_key()).ok,
              "empty name refused");
        check(!shop.run(protocol::OperationId::product_create, kBanner,
                        fields({{"name", "   "}}), owner, next_key()).ok,
              "whitespace-only name refused");
        // No name field at all.
        check(!shop.run(protocol::OperationId::product_create, kBanner,
                        fields({{"description", "Has description but no name"}}),
                        owner, next_key()).ok,
              "missing name field refused");

        shop.look([&](const engine::Store& store) {
            check(catalog::data::all_products(store).empty(),
                  "nothing was saved after any refusal");
        });
    }

    // -------------------------------------------------------------------
    section("a product can be updated and its history is correct");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        shop.run(protocol::OperationId::product_create, kBanner,
                 fields({{"name", "Old Name"}, {"description", "Old description"}}),
                 owner, next_key());

        std::int64_t created_at = 0;
        shop.look([&](const engine::Store& store) {
            const auto product = catalog::data::find_product(store, kBanner);
            created_at = product->created_at;
        });

        // Update only the name; description should stay.
        check(shop.run(protocol::OperationId::product_update, kBanner,
                       fields({{"name", "New Name"}}), owner, next_key()).ok,
              "name updated successfully");

        shop.look([&](const engine::Store& store) {
            const auto product = catalog::data::find_product(store, kBanner);
            check(product->name == "New Name",
                  "name changed to the new value");
            check(product->description == "Old description",
                  "description unchanged because it was not in the update payload");
            check(product->created_at == created_at,
                  "created_at never changes");
            check(product->updated_at > created_at,
                  "updated_at advanced");
        });

        // Update only the description; name should stay.
        check(shop.run(protocol::OperationId::product_update, kBanner,
                       fields({{"description", "New description"}}),
                       owner, next_key()).ok,
              "description updated successfully");

        shop.look([&](const engine::Store& store) {
            const auto product = catalog::data::find_product(store, kBanner);
            check(product->name        == "New Name",        "name still 'New Name'");
            check(product->description == "New description", "description changed");
        });
    }

    // -------------------------------------------------------------------
    section("updating a product with a blank name is refused");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        shop.run(protocol::OperationId::product_create, kBanner,
                 fields({{"name", "Banner"}}), owner, next_key());

        check(!shop.run(protocol::OperationId::product_update, kBanner,
                        fields({{"name", ""}}), owner, next_key()).ok,
              "clearing the name via update is refused");

        shop.look([&](const engine::Store& store) {
            const auto product = catalog::data::find_product(store, kBanner);
            check(product->name == "Banner",
                  "name unchanged after the refused update");
        });
    }

    // -------------------------------------------------------------------
    section("updating or archiving a product that does not exist is refused");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const std::string kGhost = "20000000000000000000000000000099";

        const modules::Outcome update_ghost = shop.run(
            protocol::OperationId::product_update, kGhost,
            fields({{"name", "Ghost"}}), owner, next_key());
        check(!update_ghost.ok, "update of unknown product refused");

        const modules::Outcome archive_ghost = shop.run(
            protocol::OperationId::product_archive, kGhost, owner, next_key());
        check(!archive_ghost.ok, "archive of unknown product refused");
    }

    // -------------------------------------------------------------------
    section("archiving a product stops it from being updated");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        shop.run(protocol::OperationId::product_create, kFlexPrint,
                 fields({{"name", "Flex Print"}}), owner, next_key());

        const std::string archive_key = next_key();
        check(shop.run(protocol::OperationId::product_archive, kFlexPrint,
                       owner, archive_key).ok,
              "product archived successfully");

        // Archiving twice with the same key is a replay.
        check(shop.run(protocol::OperationId::product_archive, kFlexPrint,
                       owner, archive_key).ok,
              "same key arriving again is treated as a replay");

        // Archiving twice with a new key: the service ignores the duplicate
        // (the product is already archived, nothing to do).
        check(shop.run(protocol::OperationId::product_archive, kFlexPrint,
                       owner, next_key()).ok,
              "archiving an already-archived product with a new key is accepted");

        // Trying to update after archiving is refused regardless of key.
        check(!shop.run(protocol::OperationId::product_update, kFlexPrint,
                        fields({{"name", "Updated after archive"}}),
                        owner, next_key()).ok,
              "update after archive is refused");

        shop.look([&](const engine::Store& store) {
            const auto product = catalog::data::find_product(store, kFlexPrint);
            check(product->archived,              "product is archived in the store");
            check(product->name == "Flex Print",  "name was not changed by the refused update");
        });
    }

    // -------------------------------------------------------------------
    section("a caller with no write right is refused at the gate");
    {
        Shop shop;
        const engine::Session reader = reader_session();

        const modules::Outcome no_right = shop.run(
            protocol::OperationId::product_create, kBanner,
            fields({{"name", "Should not be created"}}),
            reader, next_key());
        check(!no_right.ok, "create refused without right_product_write");

        shop.look([&](const engine::Store& store) {
            check(catalog::data::all_products(store).empty(),
                  "nothing was saved when the right was absent");
        });
    }

    // -------------------------------------------------------------------
    section("a damaged payload is refused with a clear message");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        // Build a blob that is not a valid payload.
        const engine::Blob garbage = {0x01, 0x02, 0x03, 0x04, 0x05};
        const modules::Outcome bad_payload = shop.run(
            protocol::OperationId::product_create, kBanner,
            garbage, owner, next_key());
        check(!bad_payload.ok,         "damaged payload refused");
        check(!bad_payload.error.empty(),"refusal carries a human-readable message");

        shop.look([&](const engine::Store& store) {
            check(catalog::data::all_products(store).empty(),
                  "nothing saved after a damaged payload");
        });
    }

    return squiflow::testing::report();
}
