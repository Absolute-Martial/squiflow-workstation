#include "engine/audit/audit_log.hpp"
#include "engine/identity/session.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/registry.hpp"
#include "modules/sourcing/data/repository.hpp"
#include "modules/sourcing/data/tables.hpp"
#include "modules/sourcing/domain/sourcing.hpp"
#include "modules/sourcing/module.hpp"
#include "support/check.hpp"
#include "workflows/record_purchase.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace protocol = squiflow::protocol;
namespace sourcing = squiflow::modules::sourcing;
namespace workflows = squiflow::workflows;
using squiflow::testing::check;
using squiflow::testing::section;

namespace {

constexpr std::int64_t kNow = 1'700'100'000'000;
const std::string kPerson = "a1000000000000000000000000000001";
const std::string kDevice = "a1000000000000000000000000000002";
const std::string kSupplier = "a2000000000000000000000000000001";
const std::string kMaterial = "a3000000000000000000000000000001";
const std::string kPurchase = "a4000000000000000000000000000001";
const std::string kPaidPurchase = "a4000000000000000000000000000002";

std::int64_t now() { return kNow; }

engine::Session owner() {
    engine::Session session;
    session.person = engine::record_id_from_string(kPerson);
    session.device = engine::record_id_from_string(kDevice);
    session.display_name = "Owner";
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

engine::Blob request(bool paid = false) {
    engine::Row row;
    row.set("supplier_id", engine::Value::text(kSupplier));
    row.set("material_id", engine::Value::text(kMaterial));
    row.set("material_name", engine::Value::text("300gsm matte card"));
    row.set("material_description", engine::Value::text("SRA3 white"));
    row.set("purchased_at", engine::Value::integer(kNow - 1'000));
    row.set("quantity_scaled", engine::Value::integer(25'000));
    row.set("total_cost_minor", engine::Value::integer(18'750));
    row.set("bill_file_ref", engine::Value::text("bill/sha256/abc"));
    row.set("paid", engine::Value::boolean(paid));
    if (paid) {
        row.set("settled_at", engine::Value::integer(kNow - 500));
        row.set("settlement_note", engine::Value::text("Paid at counter"));
    }
    return engine::encode_payload(row);
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database{};

    Shop() {
        registry.add(sourcing::make_module(now));
        registry.install_workflow(workflows::make_record_purchase(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
        database->write([](engine::Transaction& transaction) {
            sourcing::SupplierProfile supplier;
            supplier.id = kSupplier;
            supplier.created_at = kNow - 10'000;
            supplier.created_by = kPerson;
            supplier.updated_at = supplier.created_at;
            supplier.updated_by = kPerson;
            sourcing::data::save_supplier(transaction, supplier);
        });
    }

    modules::Outcome run(const std::string& id, const std::string& key,
                         engine::Blob payload, const engine::Session& session = owner()) {
        modules::Call call;
        call.operation = protocol::OperationId::record_purchase;
        call.record_id = id;
        call.idempotency_key = key;
        call.payload = std::move(payload);
        return registry.run(*database, call, session, engine::ConnectionState::Online);
    }

    std::size_t count(const std::string& table) const {
        std::size_t result = 0;
        database->read([&](const engine::Store& store) { result = store.count(table); });
        return result;
    }
};

}  // namespace

int main() {
    section("owed purchase is one audited workflow");
    Shop shop;
    check(shop.registry.workflow_available(protocol::OperationId::record_purchase),
          "production definition is available");
    const auto recorded = shop.run(kPurchase, "purchase-owed", request());
    check(recorded.ok && recorded.queued && !recorded.replayed,
          "purchase commits and enters the outbox");
    shop.database->read([](const engine::Store& store) {
        const auto material = sourcing::data::find_material(store, kMaterial);
        const auto purchase = sourcing::data::find_purchase(store, kPurchase);
        check(material && material->name == "300gsm matte card",
              "material memory is created");
        check(purchase && purchase->state == sourcing::PurchaseState::Owed,
              "owed state is retained");
        check(purchase && purchase->quantity_scaled == 25'000 &&
                           purchase->total_cost_minor == 18'750,
              "quantity and cost are exact");
    });
    check(shop.count(engine::AuditLog::table_name()) == 1,
          "one workflow audit row is written");
    check(shop.count(engine::Outbox::table_name()) == 1,
          "one outbox row is written");

    section("replay and paid evidence");
    const auto replay = shop.run(kPurchase, "purchase-owed", request());
    check(replay.ok && replay.replayed && !replay.queued,
          "same idempotency key replays");
    check(shop.count(sourcing::tables::kPurchase) == 1,
          "replay does not duplicate purchase");
    const auto paid = shop.run(kPaidPurchase, "purchase-paid", request(true));
    check(paid.ok, "paid purchase is accepted");
    shop.database->read([](const engine::Store& store) {
        const auto purchase = sourcing::data::find_purchase(store, kPaidPurchase);
        check(purchase && purchase->state == sourcing::PurchaseState::Paid,
              "paid state is retained");
        check(purchase && purchase->settled_at == kNow - 500 &&
                           purchase->settled_by == kPerson,
              "settlement evidence is complete");
    });

    section("refusals roll back completely");
    engine::Row unknown;
    unknown.set("invented", engine::Value::text("value"));
    check(!shop.run("a4000000000000000000000000000003", "unknown-field",
                    engine::encode_payload(unknown)).ok,
          "unknown fields are refused");
    auto missing_supplier = request();
    engine::Row changed = engine::decode_payload(missing_supplier);
    changed.set("supplier_id",
                engine::Value::text("a2000000000000000000000000000099"));
    check(!shop.run("a4000000000000000000000000000004", "missing-supplier",
                    engine::encode_payload(changed)).ok,
          "unknown supplier is refused");
    check(!shop.run("a4000000000000000000000000000005", "damaged", {1, 2, 3}).ok,
          "damaged payload is refused");
    engine::Session denied = owner();
    denied.rights.revoke(protocol::RightId::right_purchase_record);
    check(!shop.run("a4000000000000000000000000000006", "denied", request(), denied).ok,
          "workflow right is enforced before the handler");
    check(shop.count(sourcing::tables::kPurchase) == 2,
          "refusals add no partial purchase");
    check(shop.count(engine::AuditLog::table_name()) == 2 &&
              shop.count(engine::Outbox::table_name()) == 2,
          "refusals add no audit or outbox rows");

    return squiflow::testing::report();
}
