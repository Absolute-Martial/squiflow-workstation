#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/registry.hpp"
#include "modules/sourcing/data/repository.hpp"
#include "modules/sourcing/module.hpp"
#include "modules/sourcing/service/sourcing_service.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace sourcing = squiflow::modules::sourcing;
namespace protocol = squiflow::protocol;

namespace {

std::atomic<std::int64_t> g_now{1'800'000'000'000};
std::int64_t now() { return g_now.fetch_add(1000) + 1000; }
std::atomic<int> g_key{0};
std::string key() { return "src-key-" + std::to_string(g_key.fetch_add(1) + 1); }

const std::string kPerson = "81000000000000000000000000000001";
const std::string kSupplierA = "82000000000000000000000000000001";
const std::string kSupplierB = "82000000000000000000000000000002";
const std::string kSupplierC = "82000000000000000000000000000003";
const std::string kMaterialA = "83000000000000000000000000000001";
const std::string kMaterialB = "83000000000000000000000000000002";
const std::string kMaterialC = "83000000000000000000000000000003";
const std::string kPurchaseA = "84000000000000000000000000000001";
const std::string kPurchaseB = "84000000000000000000000000000002";
const std::string kPurchaseC = "84000000000000000000000000000003";
const std::string kPurchaseD = "84000000000000000000000000000004";
const std::string kPurchaseE = "84000000000000000000000000000005";

engine::Blob payload(
    std::initializer_list<std::pair<std::string, std::string>> texts = {},
    std::initializer_list<std::pair<std::string, std::int64_t>> numbers = {}) {
    engine::Row row;
    for (const auto& [name, value] : texts) {
        row.set(name, engine::Value::text(value));
    }
    for (const auto& [name, value] : numbers) {
        row.set(name, engine::Value::integer(value));
    }
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

engine::Session staff(std::initializer_list<protocol::RightId> rights) {
    engine::Session session;
    session.person = engine::record_id_from_string(kPerson);
    session.device = engine::RecordId{1, 2};
    session.display_name = "Staff";
    for (const protocol::RightId right : rights) {
        session.rights.grant(right);
    }
    return session;
}

template <typename Fn>
bool violates(Fn&& fn) {
    try {
        fn();
    } catch (const modules::RuleViolation&) {
        return true;
    }
    return false;
}

sourcing::Material material(const std::string& id = kMaterialA,
                             const std::string& name = "300gsm matte card") {
    sourcing::Material value;
    value.id = id;
    value.name = name;
    value.description = "SRA3, white, double-sided coated";
    value.created_at = 1'700'000'000'000;
    value.created_by = kPerson;
    value.updated_at = value.created_at;
    value.updated_by = kPerson;
    return value;
}

sourcing::Purchase purchase(const std::string& id,
                             const std::string& supplier_id,
                             const std::string& material_id,
                             std::int64_t purchased_at,
                             sourcing::PurchaseState state = sourcing::PurchaseState::Owed) {
    sourcing::Purchase value;
    value.id = id;
    value.supplier_id = supplier_id;
    value.material_id = material_id;
    value.purchased_at = purchased_at;
    value.quantity_scaled = 25'000;
    value.total_cost_minor = 18'750;
    value.bill_file_ref = "bill/sha256/abc";
    value.state = state;
    value.created_at = 1'700'100'000'000;
    value.created_by = kPerson;
    if (state == sourcing::PurchaseState::Paid) {
        value.settled_at = purchased_at;
        value.settled_by = kPerson;
        value.settlement_note = "Paid at the counter";
    }
    return value;
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(sourcing::make_module(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
    }

    modules::Outcome run(protocol::OperationId operation,
                         const std::string& record,
                         const engine::Blob& body,
                         const engine::Session& session,
                         engine::ConnectionState connection = engine::ConnectionState::Online,
                         bool with_key = true) {
        modules::Call call;
        call.operation = operation;
        call.record_id = record;
        call.payload = body;
        if (protocol::operation(operation).sync_class ==
            protocol::OperationClass::Synchronizable) {
            call.idempotency_key = with_key ? key() : std::string{};
        }
        return registry.run(*database, call, session, connection);
    }

    template <typename Fn>
    void read(Fn&& fn) const {
        database->read([&](const engine::Store& store) { fn(store); });
    }
};

void create_supplier(Shop& shop,
                     const engine::Session& session,
                     const std::string& id = kSupplierA) {
    const modules::Outcome outcome = shop.run(
        protocol::OperationId::supplier_create, id,
        payload({{"kind", "local"},
                 {"supplies", "Card, flex, and certificate paper"},
                 {"reliability_notes", "Calls before substituting stock"},
                 {"sourcing_notes", "Rear warehouse, blue shutter"}},
                {{"lead_time_days", 2}}),
        session);
    if (!outcome.ok) {
        throw modules::RuleViolation(outcome.error);
    }
}

void record(Shop& shop,
            const sourcing::Material& item,
            const sourcing::Purchase& receipt) {
    sourcing::SourcingService service{now};
    shop.database->write([&](engine::Transaction& transaction) {
        service.record_purchase(transaction, item, receipt);
    });
}

}  // namespace

int main() {
    const engine::Session session = owner();

    section("migration 19 and the exact sourcing operation surface");
    {
        Shop shop;
        check(shop.registry.handled(protocol::OperationId::supplier_create),
              "supplier create handled");
        check(shop.registry.handled(protocol::OperationId::supplier_update),
              "supplier update handled");
        check(shop.registry.handled(protocol::OperationId::purchase_settle),
              "purchase settle handled");
        check(shop.registry.handled(protocol::OperationId::purchase_lookup),
              "purchase lookup handled");
        check(!shop.registry.handled(protocol::OperationId::record_purchase),
              "the later workflow is not stolen by this module");
        check(!shop.registry.handled(protocol::OperationId::agreement_create),
              "the module claims nothing belonging elsewhere");
        check(sourcing::tables::kFirstMigration == 19, "sourcing owns migration 19");

        const auto& create = protocol::operation(protocol::OperationId::supplier_create);
        const auto& update = protocol::operation(protocol::OperationId::supplier_update);
        const auto& settle = protocol::operation(protocol::OperationId::purchase_settle);
        const auto& lookup = protocol::operation(protocol::OperationId::purchase_lookup);
        check(create.sync_class == protocol::OperationClass::Synchronizable,
              "supplier create synchronizes");
        check(update.sync_class == protocol::OperationClass::Synchronizable,
              "supplier update synchronizes");
        check(settle.sync_class == protocol::OperationClass::Synchronizable,
              "purchase settlement synchronizes");
        check(lookup.sync_class == protocol::OperationClass::LocalOnly,
              "purchase lookup never goes to the server");
        check(create.offline == protocol::OfflineRule::OfflineAllowed &&
                  update.offline == protocol::OfflineRule::OfflineAllowed &&
                  settle.offline == protocol::OfflineRule::OfflineAllowed &&
                  lookup.offline == protocol::OfflineRule::OfflineAllowed,
              "all four operations carry the declared offline allowance");
        check(create.right == protocol::RightId::right_supplier_write &&
                  update.right == protocol::RightId::right_supplier_write,
              "supplier editing needs its own right");
        check(settle.right == protocol::RightId::right_purchase_settle,
              "settling a purchase needs the settlement right");
        check(lookup.right == protocol::RightId::right_supplier_read,
              "lookup needs only the supplier read right");
        check(create.module == protocol::ModuleId::sourcing &&
                  lookup.module == protocol::ModuleId::sourcing,
              "the operation table attributes both sides to sourcing");

        shop.read([](const engine::Store& store) {
            check(store.has_table(sourcing::tables::kSupplier),
                  "supplier profile table exists");
            check(store.has_table(sourcing::tables::kMaterial), "material table exists");
            check(store.has_table(sourcing::tables::kPurchase), "purchase table exists");
            check(store.count(sourcing::tables::kSupplier) == 0, "new shop has no suppliers");
            check(store.count(sourcing::tables::kMaterial) == 0, "new shop has no materials");
            check(store.count(sourcing::tables::kPurchase) == 0, "new shop has no purchases");
        });
    }

    section("a supplier profile extends one party identity and owns only sourcing memory");
    {
        Shop shop;
        const modules::Outcome created = shop.run(
            protocol::OperationId::supplier_create, kSupplierA,
            payload({{"kind", "importer"},
                     {"supplies", "PVC card blanks"},
                     {"reliability_notes", "Consistent batches"},
                     {"sourcing_notes", "Order before Dashain"}},
                    {{"lead_time_days", 14}}),
            session, engine::ConnectionState::Offline);
        check(created.ok, "an owner can add sourcing memory offline");
        check(created.queued, "the offline profile is queued for sync");

        shop.read([](const engine::Store& store) {
            const auto supplier = sourcing::data::find_supplier(store, kSupplierA);
            check(supplier.has_value(), "the supplier profile is on file");
            check(supplier->id == kSupplierA, "the profile uses the party id directly");
            check(supplier->kind == sourcing::SupplierKind::Importer,
                  "importer status is remembered");
            check(supplier->supplies == "PVC card blanks", "what they supply is remembered");
            check(supplier->lead_time_days == 14, "lead time is remembered");
            check(supplier->created_at > 0 && !supplier->created_by.empty(),
                  "creation evidence is recorded");
            check(supplier->updated_at == supplier->created_at &&
                      supplier->updated_by == supplier->created_by,
                  "a new profile begins with coherent update evidence");
            check(!to_row(*supplier).has("display_name"),
                  "sourcing does not copy the party display name");
            check(!to_row(*supplier).has("phone"),
                  "sourcing does not steal party contact ownership");
        });

        const modules::Outcome changed = shop.run(
            protocol::OperationId::supplier_update, kSupplierA,
            payload({{"kind", "local"},
                     {"supplies", "PVC blanks and lanyards"}},
                    {{"lead_time_days", 3}}),
            session);
        check(changed.ok, "the sourcing profile can be amended");
        shop.read([](const engine::Store& store) {
            const auto supplier = sourcing::data::find_supplier(store, kSupplierA);
            check(supplier->kind == sourcing::SupplierKind::LocalDealer,
                  "supplier kind changed");
            check(supplier->supplies == "PVC blanks and lanyards", "supplies changed");
            check(supplier->lead_time_days == 3, "lead time changed");
            check(supplier->reliability_notes == "Consistent batches",
                  "an omitted field survives the amendment");
            check(supplier->updated_at > supplier->created_at,
                  "the amendment has a later timestamp");
        });

        check(!shop.run(protocol::OperationId::supplier_create, kSupplierA, payload({}), session).ok,
              "a duplicate profile is refused");
        check(!shop.run(protocol::OperationId::supplier_update, kSupplierB, payload({}), session).ok,
              "an unknown profile cannot be amended");
        check(!shop.run(protocol::OperationId::supplier_create, kSupplierB,
                        payload({{"kind", "wholesaler"}}), session).ok,
              "an invented supplier kind is refused");
        check(!shop.run(protocol::OperationId::supplier_create, kSupplierB,
                        payload({}, {{"lead_time_days", -1}}), session).ok,
              "negative lead time is refused");
        check(!shop.run(protocol::OperationId::supplier_update, kSupplierA,
                        payload({{"lead_time_days", "soon"}}), session).ok,
              "lead time must really be numeric");
        shop.read([](const engine::Store& store) {
            check(store.count(sourcing::tables::kSupplier) == 1,
                  "refused supplier writes leave no extra row");
        });
    }

    section("domain invariants make paid and owed mutually honest");
    {
        check(std::string{sourcing::to_string(sourcing::SupplierKind::LocalDealer)} ==
                  "local dealer",
              "local dealer has a readable name");
        check(std::string{sourcing::to_string(sourcing::SupplierKind::Importer)} == "importer",
              "importer has a readable name");
        check(std::string{sourcing::to_string(sourcing::PurchaseState::Owed)} == "owed",
              "owed has a readable name");
        check(std::string{sourcing::to_string(sourcing::PurchaseState::Paid)} == "paid",
              "paid has a readable name");

        sourcing::SupplierProfile supplier;
        supplier.id = kSupplierA;
        supplier.created_at = 100;
        supplier.created_by = kPerson;
        supplier.updated_at = 100;
        supplier.updated_by = kPerson;
        check(!violates([&] { sourcing::validate(supplier); }), "a minimal profile is valid");
        supplier.lead_time_days = -1;
        check(violates([&] { sourcing::validate(supplier); }), "negative lead time is invalid");
        supplier.lead_time_days = 0;
        supplier.updated_at = 99;
        check(violates([&] { sourcing::validate(supplier); }),
              "an update cannot predate creation");
        supplier.updated_at = 100;
        supplier.updated_by = " ";
        check(violates([&] { sourcing::validate(supplier); }),
              "update evidence needs a person");

        sourcing::Material item = material();
        check(!violates([&] { sourcing::validate(item); }), "a named material is valid");
        item.name = " \t";
        check(violates([&] { sourcing::validate(item); }),
              "a whitespace-only material name is refused");
        item = material();
        item.id.clear();
        check(violates([&] { sourcing::validate(item); }), "a material needs its own record");

        sourcing::Purchase receipt = purchase(
            kPurchaseA, kSupplierA, kMaterialA, 1'699'000'000'000);
        check(sourcing::is_outstanding(receipt), "an owed purchase is outstanding");
        check(!violates([&] { sourcing::validate(receipt); }), "a coherent debt is valid");
        receipt.quantity_scaled = 0;
        check(violates([&] { sourcing::validate(receipt); }), "zero quantity is refused");
        receipt.quantity_scaled = -1;
        check(violates([&] { sourcing::validate(receipt); }), "negative quantity is refused");
        receipt.quantity_scaled = std::numeric_limits<std::int64_t>::max();
        check(!violates([&] { sourcing::validate(receipt); }),
              "a large positive historical quantity is preserved, not narrowed");
        receipt.total_cost_minor = -1;
        check(violates([&] { sourcing::validate(receipt); }), "negative purchase cost is refused");
        receipt.total_cost_minor = std::numeric_limits<std::int64_t>::max();
        check(!violates([&] { sourcing::validate(receipt); }),
              "the largest representable non-negative cost is accepted exactly");
        receipt.purchased_at = receipt.created_at + 1;
        check(violates([&] { sourcing::validate(receipt); }),
              "a purchase cannot be logged before its stated purchase date");

        receipt = purchase(kPurchaseA, kSupplierA, kMaterialA, 1'699'000'000'000);
        receipt.settled_at = receipt.purchased_at;
        check(violates([&] { sourcing::validate(receipt); }),
              "an owed purchase cannot carry a settlement date");
        receipt.settled_at = 0;
        receipt.settled_by = kPerson;
        check(violates([&] { sourcing::validate(receipt); }),
              "an owed purchase cannot carry a settlement actor");
        receipt.settled_by.clear();
        receipt.settlement_note = "looks paid";
        check(violates([&] { sourcing::validate(receipt); }),
              "an owed purchase cannot carry a settlement note");

        receipt = purchase(kPurchaseA, kSupplierA, kMaterialA, 1'699'000'000'000,
                           sourcing::PurchaseState::Paid);
        check(!sourcing::is_outstanding(receipt), "a paid purchase is not outstanding");
        check(!violates([&] { sourcing::validate(receipt); }),
              "immediate payment with evidence is valid");
        receipt.settled_by.clear();
        check(violates([&] { sourcing::validate(receipt); }),
              "paid without an actor is refused");
        receipt.settled_by = kPerson;
        receipt.settled_at = receipt.purchased_at - 1;
        check(violates([&] { sourcing::validate(receipt); }),
              "payment cannot predate the purchase");

        receipt = purchase(kPurchaseA, kSupplierA, kMaterialA, 1'699'000'000'000);
        sourcing::settle_purchase(receipt, receipt.purchased_at + 100, kPerson, "Bank transfer");
        check(receipt.state == sourcing::PurchaseState::Paid, "settlement moves owed to paid");
        check(receipt.settled_at == 1'699'000'000'100, "the settlement moment is exact");
        check(receipt.settled_by == kPerson, "the settlement actor is exact");
        check(receipt.settlement_note == "Bank transfer", "the settlement note survives");
        check(violates([&] {
                  sourcing::settle_purchase(receipt, receipt.settled_at + 1, kPerson, "again");
              }),
              "settlement evidence cannot be silently rewritten");
    }

    section("recording a purchase is atomic and remains memory rather than inventory");
    {
        Shop shop;
        create_supplier(shop, session);
        const sourcing::Material item = material();
        const sourcing::Purchase receipt = purchase(
            kPurchaseA, kSupplierA, kMaterialA, 1'699'000'000'000);
        record(shop, item, receipt);

        shop.read([](const engine::Store& store) {
            const auto saved_item = sourcing::data::find_material(store, kMaterialA);
            const auto saved = sourcing::data::find_purchase(store, kPurchaseA);
            check(saved_item.has_value(), "the named material is on file");
            check(saved_item->name == "300gsm matte card", "its exact name survives");
            check(saved.has_value(), "the purchase is on file");
            check(saved->supplier_id == kSupplierA, "it names the supplier");
            check(saved->material_id == kMaterialA, "it names the material");
            check(saved->quantity_scaled == 25'000, "historical quantity survives exactly");
            check(saved->total_cost_minor == 18'750, "actual cost survives exactly");
            check(saved->bill_file_ref == "bill/sha256/abc", "bill evidence is retained");
            check(saved->state == sourcing::PurchaseState::Owed, "credit remains visibly owed");
            check(to_row(*saved_item).get("quantity_scaled").is_null(),
                  "the material has no stock balance");
            check(to_row(*saved_item).get("consumed_scaled").is_null(),
                  "the material has no consumption counter");
        });

        record(shop, item, purchase(kPurchaseB, kSupplierA, kMaterialA, 1'699'100'000'000,
                                    sourcing::PurchaseState::Paid));
        shop.read([](const engine::Store& store) {
            check(store.count(sourcing::tables::kMaterial) == 1,
                  "a repeated material is reused, not duplicated");
            check(store.count(sourcing::tables::kPurchase) == 2,
                  "each receipt remains a separate history fact");
        });

        check(violates([&] { record(shop, item, receipt); }),
              "the same purchase id cannot be recorded twice");
        sourcing::Purchase unknown = purchase(
            kPurchaseC, kSupplierB, kMaterialB, 1'699'200'000'000);
        check(violates([&] { record(shop, material(kMaterialB, "10oz flex"), unknown); }),
              "a purchase cannot name an unknown supplier profile");
        shop.read([](const engine::Store& store) {
            check(!sourcing::data::find_material(store, kMaterialB).has_value(),
                  "an unknown supplier leaves no orphan material");
            check(!sourcing::data::find_purchase(store, kPurchaseC).has_value(),
                  "and no half purchase");
        });

        sourcing::Material bad = material(kMaterialB, " ");
        sourcing::Purchase bad_receipt = purchase(
            kPurchaseC, kSupplierA, kMaterialB, 1'699'200'000'000);
        check(violates([&] { record(shop, bad, bad_receipt); }),
              "a bad material refuses the whole receipt");
        bad = material(kMaterialB, "10oz flex");
        bad_receipt.quantity_scaled = 0;
        check(violates([&] { record(shop, bad, bad_receipt); }),
              "a bad purchase refuses the material too");
        shop.read([](const engine::Store& store) {
            check(!sourcing::data::find_material(store, kMaterialB).has_value(),
                  "neither bad ordering left a material behind");
            check(!sourcing::data::find_purchase(store, kPurchaseC).has_value(),
                  "neither bad ordering left a purchase behind");
        });

        sourcing::Purchase mismatch = purchase(
            kPurchaseC, kSupplierA, kMaterialC, 1'699'200'000'000);
        check(violates([&] { record(shop, bad, mismatch); }),
              "purchase and material ids must agree");
        check(violates([&] {
                  record(shop, material(kMaterialB, "300gsm matte card"),
                         purchase(kPurchaseC, kSupplierA, kMaterialB, 1'699'200'000'000));
              }),
              "one exact material name cannot acquire a second id");
        check(violates([&] {
                  record(shop, material(kMaterialA, "350gsm matte card"),
                         purchase(kPurchaseC, kSupplierA, kMaterialA, 1'699'200'000'000));
              }),
              "one material id cannot silently change names");
        shop.read([](const engine::Store& store) {
            check(store.count(sourcing::tables::kMaterial) == 1,
                  "all refused attempts preserved one material");
            check(store.count(sourcing::tables::kPurchase) == 2,
                  "all refused attempts preserved two purchases");
        });
    }

    section("settlement clears one debt once and preserves its evidence");
    {
        Shop shop;
        create_supplier(shop, session);
        record(shop, material(), purchase(
            kPurchaseA, kSupplierA, kMaterialA, 1'699'000'000'000));
        record(shop, material(), purchase(
            kPurchaseB, kSupplierA, kMaterialA, 1'699'100'000'000,
            sourcing::PurchaseState::Paid));

        const modules::Outcome settled = shop.run(
            protocol::OperationId::purchase_settle, kPurchaseA,
            payload({{"note", "NEFT 7741"}}), session);
        check(settled.ok, "an owed purchase can be settled");
        check(settled.queued, "settlement is queued for sync");
        shop.read([](const engine::Store& store) {
            const auto saved = sourcing::data::find_purchase(store, kPurchaseA);
            check(saved->state == sourcing::PurchaseState::Paid, "the debt is now paid");
            check(saved->settled_at > saved->purchased_at, "settlement has a real later moment");
            check(saved->settled_by == kPerson, "settlement records the signed-in person");
            check(saved->settlement_note == "NEFT 7741", "settlement note is retained");
            check(sourcing::data::outstanding_purchases(store, 100).empty(),
                  "the outstanding screen clears immediately");
        });

        const modules::Outcome twice = shop.run(
            protocol::OperationId::purchase_settle, kPurchaseA, payload({}), session);
        check(!twice.ok, "an already-settled purchase cannot be settled twice");
        check(twice.error == "That purchase is already settled.",
              "the refusal explains exactly why");
        check(!shop.run(protocol::OperationId::purchase_settle, kPurchaseB,
                        payload({}), session).ok,
              "an immediately paid purchase cannot be settled again");
        check(!shop.run(protocol::OperationId::purchase_settle, kPurchaseC,
                        payload({}), session).ok,
              "an unknown purchase cannot be settled");

        record(shop, material(), purchase(
            kPurchaseC, kSupplierA, kMaterialA, 1'699'200'000'000));
        const modules::Outcome offline = shop.run(
            protocol::OperationId::purchase_settle, kPurchaseC, payload({}), session,
            engine::ConnectionState::Offline);
        check(offline.ok, "an owner may settle while offline");
        check(offline.queued, "the offline settlement is queued, not lost");
    }

    section("lookup answers material history and outstanding debt locally, newest first");
    {
        Shop shop;
        create_supplier(shop, session, kSupplierA);
        create_supplier(shop, session, kSupplierB);
        const sourcing::Material card = material(kMaterialA, "300gsm matte card");
        const sourcing::Material flex = material(kMaterialB, "10oz flex");
        record(shop, card, purchase(kPurchaseA, kSupplierA, kMaterialA,
                                    1'699'000'000'000));
        record(shop, card, purchase(kPurchaseB, kSupplierB, kMaterialA,
                                    1'699'200'000'000, sourcing::PurchaseState::Paid));
        record(shop, card, purchase(kPurchaseC, kSupplierA, kMaterialA,
                                    1'699'200'000'000));
        record(shop, flex, purchase(kPurchaseD, kSupplierA, kMaterialB,
                                    1'699'300'000'000));
        record(shop, flex, purchase(kPurchaseE, kSupplierB, kMaterialB,
                                    1'699'400'000'000, sourcing::PurchaseState::Paid));

        const modules::Outcome cards = shop.run(
            protocol::OperationId::purchase_lookup, "",
            payload({{"material_id", kMaterialA}}, {{"limit", 100}}), session,
            engine::ConnectionState::Offline);
        check(cards.ok, "material history works offline");
        check(!cards.queued, "a local lookup is never queued");
        check(cards.rows.size() == 3, "all three card purchases are returned");
        check(cards.rows[0].get("id").text_or({}) == kPurchaseC,
              "equal-date results use descending id as a deterministic tie-break");
        check(cards.rows[1].get("id").text_or({}) == kPurchaseB,
              "the second equal-date purchase follows predictably");
        check(cards.rows[2].get("id").text_or({}) == kPurchaseA,
              "the oldest purchase is last");
        check(cards.rows[0].get("material_name").text_or({}) == "300gsm matte card",
              "the local read model includes the material name");
        check(cards.rows[0].get("total_cost_minor").integer_or(-1) == 18'750,
              "the cost-paid history is exact");

        const modules::Outcome supplier = shop.run(
            protocol::OperationId::purchase_lookup, "",
            payload({{"supplier_id", kSupplierB}}), session);
        check(supplier.ok && supplier.rows.size() == 2,
              "supplier history filters without losing either material");
        check(supplier.rows[0].get("id").text_or({}) == kPurchaseE,
              "supplier history remains newest first");

        const modules::Outcome owed = shop.run(
            protocol::OperationId::purchase_lookup, "",
            payload({}, {{"outstanding_only", 1}}), session);
        check(owed.ok && owed.rows.size() == 3,
              "one local screen lists exactly what is still owed");
        check(owed.rows[0].get("id").text_or({}) == kPurchaseD,
              "the newest debt comes first");
        check(owed.rows[1].get("id").text_or({}) == kPurchaseC,
              "the next debt follows");
        check(owed.rows[2].get("id").text_or({}) == kPurchaseA,
              "the oldest debt remains visible");

        const modules::Outcome combined = shop.run(
            protocol::OperationId::purchase_lookup, "",
            payload({{"material_id", kMaterialA}, {"supplier_id", kSupplierA}},
                    {{"outstanding_only", 1}, {"limit", 1}}), session);
        check(combined.ok && combined.rows.size() == 1,
              "material, supplier, debt and limit filters combine with AND");
        check(combined.rows[0].get("id").text_or({}) == kPurchaseC,
              "the bound keeps the newest matching receipt");

        const modules::Outcome unknown = shop.run(
            protocol::OperationId::purchase_lookup, "",
            payload({{"material_id", kMaterialC}}), session);
        check(unknown.ok && unknown.rows.empty(), "unknown material is an empty history, not an error");
        check(!shop.run(protocol::OperationId::purchase_lookup, "",
                        payload({}, {{"limit", 0}}), session).ok,
              "zero result limit is refused");
        check(!shop.run(protocol::OperationId::purchase_lookup, "",
                        payload({}, {{"limit", -1}}), session).ok,
              "negative result limit is refused");
        check(!shop.run(protocol::OperationId::purchase_lookup, "",
                        payload({}, {{"limit", 501}}), session).ok,
              "a hostile oversized result limit is refused");
        check(shop.run(protocol::OperationId::purchase_lookup, "",
                       payload({}, {{"limit", 500}}), session).ok,
              "the documented upper boundary is accepted");
        check(!shop.run(protocol::OperationId::purchase_lookup, "",
                        payload({}, {{"outstanding_only", 2}}), session).ok,
              "an integer other than zero or one is not accepted as a boolean");
        check(!shop.run(protocol::OperationId::purchase_lookup, "",
                        payload({{"limit", "many"}}), session).ok,
              "a text limit is refused rather than guessed");
    }

    section("rights malformed payloads idempotency and rollback fail safely");
    {
        Shop shop;
        engine::Session unsigned_session;
        unsigned_session.rights.grant_all();
        const modules::Outcome unsigned_create = shop.run(
            protocol::OperationId::supplier_create, kSupplierA, payload({}), unsigned_session);
        check(!unsigned_create.ok, "rights do not replace authentication");
        check(unsigned_create.reason == engine::DenialReason::NotSignedIn,
              "the refusal says not signed in");

        const engine::Session reader = staff({protocol::RightId::right_supplier_read});
        const engine::Session writer = staff({protocol::RightId::right_supplier_write});
        const engine::Session settler = staff({protocol::RightId::right_purchase_settle});
        check(!shop.run(protocol::OperationId::supplier_create, kSupplierA,
                        payload({}), reader).ok,
              "supplier read does not grant supplier write");
        check(shop.run(protocol::OperationId::supplier_create, kSupplierA,
                       payload({}), writer).ok,
              "supplier write can create a profile");
        check(!shop.run(protocol::OperationId::supplier_update, kSupplierA,
                        payload({}), settler).ok,
              "purchase settlement does not grant profile editing");
        const modules::Outcome can_read = shop.run(
            protocol::OperationId::purchase_lookup, "", payload({}), reader,
            engine::ConnectionState::Offline);
        check(can_read.ok, "read-only staff may use the declared offline lookup exception");
        check(!can_read.queued, "the staff lookup stays local");
        const modules::Outcome staff_offline_write = shop.run(
            protocol::OperationId::supplier_update, kSupplierA, payload({}), writer,
            engine::ConnectionState::Offline);
        check(!staff_offline_write.ok, "offline allowance does not bypass the staff write guard");
        check(staff_offline_write.reason == engine::DenialReason::ReadOnlyOffline,
              "the staff write refusal names the offline guard");

        record(shop, material(), purchase(
            kPurchaseA, kSupplierA, kMaterialA, 1'699'000'000'000));
        check(!shop.run(protocol::OperationId::purchase_settle, kPurchaseA,
                        payload({}), writer).ok,
              "supplier write cannot settle a debt");
        check(shop.run(protocol::OperationId::purchase_settle, kPurchaseA,
                       payload({}), settler).ok,
              "the dedicated settlement right can settle it");

        const modules::Outcome malformed_create = shop.run(
            protocol::OperationId::supplier_create, kSupplierB,
            engine::Blob{1, 2, 3, 4}, session);
        check(!malformed_create.ok &&
                  malformed_create.error == "This request could not be read. Please try it again.",
              "a malformed supplier payload is refused in standard words");
        const modules::Outcome malformed_lookup = shop.run(
            protocol::OperationId::purchase_lookup, "", engine::Blob{9, 9, 9}, reader);
        check(!malformed_lookup.ok &&
                  malformed_lookup.error == "This request could not be read. Please try it again.",
              "a malformed local lookup is refused in the same words");

        bool no_record = false;
        try {
            shop.run(protocol::OperationId::supplier_create, "", payload({}), session);
        } catch (const modules::RegistryError&) {
            no_record = true;
        }
        check(no_record, "a synchronizable write without a record id is a loud wiring error");
        bool no_key = false;
        try {
            shop.run(protocol::OperationId::supplier_create, kSupplierB,
                     payload({}), session, engine::ConnectionState::Online, false);
        } catch (const modules::RegistryError&) {
            no_key = true;
        }
        check(no_key, "a synchronizable write without an idempotency key is loud");

        modules::Call confused;
        confused.operation = protocol::OperationId::purchase_lookup;
        confused.payload = payload({});
        confused.idempotency_key = "a-read-is-not-a-write";
        const modules::Outcome keyed_read = shop.registry.run(
            *shop.database, confused, reader, engine::ConnectionState::Online);
        check(keyed_read.ok, "an irrelevant key does not turn a local read into a write");
        check(!keyed_read.queued && !keyed_read.replayed,
              "the keyed local read still neither queues nor replays");

        modules::Call create;
        create.operation = protocol::OperationId::supplier_create;
        create.record_id = kSupplierB;
        create.payload = payload({{"supplies", "Paper"}});
        create.idempotency_key = "same-supplier-attempt";
        const modules::Outcome first = shop.registry.run(
            *shop.database, create, session, engine::ConnectionState::Online);
        const modules::Outcome replay = shop.registry.run(
            *shop.database, create, session, engine::ConnectionState::Online);
        check(first.ok && first.queued && !first.replayed, "the first synchronized create runs");
        check(replay.ok && replay.replayed && !replay.queued,
              "the exact retry is recognized and not queued twice");
        shop.read([](const engine::Store& store) {
            check(store.count(sourcing::tables::kSupplier) == 2,
                  "the replay did not duplicate the supplier profile");
            check(store.count(sourcing::tables::kMaterial) == 1,
                  "refused calls did not alter material memory");
            check(store.count(sourcing::tables::kPurchase) == 1,
                  "refused calls did not alter purchase history");
        });
    }

    return squiflow::testing::report();
}
