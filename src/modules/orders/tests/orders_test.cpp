// Orders module - arithmetic and invariants first, then the wire.
//
// The central claim under test is that an order line carries a price snapshot.
// Pricing is asked exactly once, when the line is added. A later rate change
// must affect a later line and must never rewrite an earlier agreement.

#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/module.hpp"
#include "modules/pricing/data/repository.hpp"
#include "modules/pricing/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace engine   = squiflow::engine;
namespace modules  = squiflow::modules;
namespace orders   = squiflow::modules::orders;
namespace pricing  = squiflow::modules::pricing;
namespace protocol = squiflow::protocol;

namespace {

std::atomic<std::int64_t> g_now{1'700'000'000'000};
std::int64_t now() {
    return g_now.fetch_add(1000, std::memory_order_relaxed) + 1000;
}

std::atomic<int> g_key{0};
std::string next_key() {
    return "k-orders-" +
           std::to_string(g_key.fetch_add(1, std::memory_order_relaxed) + 1);
}

const std::string kOrderA   = "40000000000000000000000000000001";
const std::string kOrderB   = "40000000000000000000000000000002";
const std::string kOrderC   = "40000000000000000000000000000003";
const std::string kLineA    = "41000000000000000000000000000001";
const std::string kLineB    = "41000000000000000000000000000002";
const std::string kLineC    = "41000000000000000000000000000003";
const std::string kLineD    = "41000000000000000000000000000004";
const std::string kProductA = "42000000000000000000000000000001";
const std::string kProductB = "42000000000000000000000000000002";
const std::string kPartyA   = "43000000000000000000000000000001";
const std::string kPartyB   = "43000000000000000000000000000002";
const std::string kRateA    = "44000000000000000000000000000001";
const std::string kOverride = "45000000000000000000000000000001";

std::string concurrent_line_id(int index) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string id = "46000000000000000000000000000000";
    id[30] = digits[(index / 16) % 16];
    id[31] = digits[index % 16];
    return id;
}

engine::Blob fields(
    std::initializer_list<std::pair<std::string, std::string>> text_pairs,
    std::initializer_list<std::pair<std::string, std::int64_t>> int_pairs = {}) {
    engine::Row row;
    for (const auto& [key, value] : text_pairs) {
        row.set(key, engine::Value::text(value));
    }
    for (const auto& [key, value] : int_pairs) {
        row.set(key, engine::Value::integer(value));
    }
    return engine::encode_payload(row);
}

template <typename Fn>
bool refused(Fn&& fn) {
    try {
        fn();
        return false;
    } catch (const modules::RuleViolation&) {
        return true;
    }
}

orders::Order make_order(const std::string& id = kOrderA) {
    orders::Order order;
    order.id = id;
    order.party_id = kPartyA;
    order.created_at = 1000;
    order.created_by = "tester";
    return order;
}

orders::OrderLine make_line(const std::string& id = kLineA) {
    orders::OrderLine line;
    line.id = id;
    line.order_id = kOrderA;
    line.product_id = kProductA;
    line.description = "Printed banner";
    line.quantity_scaled = engine::Quantity::kScale;
    line.unit_price_minor = 50'000;
    line.price_source = pricing::RateSource::Default;
    line.added_at = 1000;
    line.added_by = "tester";
    return line;
}

// A reader for impossible storage states. Normal repository writes validate
// rows before they land, but a future build, damaged file or manual repair can
// still hand this build a position it could never have written itself.
struct RowReader {
    std::vector<engine::Row> rows;

    std::vector<engine::Row> select(const engine::Query&) const {
        return rows;
    }
};

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(pricing::make_module(now));
        registry.add(orders::make_module(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
    }

    modules::Outcome run(protocol::OperationId operation,
                         const std::string& record,
                         const engine::Blob& payload,
                         const engine::Session& session,
                         const std::string& key,
                         engine::ConnectionState connection = engine::ConnectionState::Online) {
        modules::Call call;
        call.operation = operation;
        call.record_id = record;
        call.payload = payload;
        call.idempotency_key = key;
        return registry.run(*database, call, session, connection);
    }

    modules::Outcome run(protocol::OperationId operation,
                         const std::string& record,
                         const engine::Session& session,
                         const std::string& key,
                         engine::ConnectionState connection = engine::ConnectionState::Online) {
        return run(operation, record, {}, session, key, connection);
    }

    template <typename Fn>
    void look(Fn&& fn) const {
        database->read([&](const engine::Store& store) { fn(store); });
    }
};

engine::Session owner_session() {
    engine::Session session;
    session.person = engine::record_id_from_string("00000000000000000000000000000001");
    session.device = engine::RecordId{1, 1};
    session.display_name = "Shopkeeper";
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

engine::Session reader_session() {
    engine::Session session;
    session.person = engine::record_id_from_string("00000000000000000000000000000002");
    session.device = engine::RecordId{1, 2};
    session.display_name = "Reader";
    session.rights.grant(protocol::RightId::right_order_read);
    return session;
}

engine::Session writer_session() {
    engine::Session session;
    session.person = engine::record_id_from_string("00000000000000000000000000000003");
    session.device = engine::RecordId{1, 3};
    session.display_name = "Counter staff";
    session.rights.grant(protocol::RightId::right_order_read);
    session.rights.grant(protocol::RightId::right_order_write);
    return session;
}

modules::Outcome set_default(Shop& shop, const engine::Session& owner,
                             const std::string& product, std::int64_t amount) {
    return shop.run(protocol::OperationId::rate_default_set, product,
                    fields({}, {{"amount_minor", amount}}), owner, next_key());
}

modules::Outcome create_order(Shop& shop, const engine::Session& session,
                              const std::string& id, const std::string& party = kPartyA,
                              engine::ConnectionState connection = engine::ConnectionState::Online) {
    return shop.run(protocol::OperationId::order_create, id,
                    fields({{"party_id", party}, {"note", "Counter order"}},
                           {{"promised_at", 1'800'000'000'000}}),
                    session, next_key(), connection);
}

modules::Outcome add_line(Shop& shop, const engine::Session& session,
                          const std::string& line_id, const std::string& order_id,
                          const std::string& product, std::int64_t quantity,
                          const std::string& description = "Printed banner") {
    return shop.run(protocol::OperationId::order_line_add, line_id,
                    fields({{"order_id", order_id}, {"product_id", product},
                            {"description", description}},
                           {{"quantity_scaled", quantity}}),
                    session, next_key());
}

}  // namespace

int main() {
    // ===================================================================
    // Part one: domain rules with no database involved.
    // ===================================================================

    section("order states are small and explicit");
    {
        check(std::string{orders::to_string(orders::OrderState::Open)} == "open",
              "Open has stable text");
        check(std::string{orders::to_string(orders::OrderState::Cancelled)} == "cancelled",
              "Cancelled has stable text");
        check(std::string{orders::to_string(static_cast<orders::OrderState>(99))} == "?",
              "a state from another build is not given a plausible name");
        check(orders::can_change(orders::OrderState::Open), "an open order may change");
        check(!orders::can_change(orders::OrderState::Cancelled),
              "a cancelled order is frozen");
    }

    section("line and order arithmetic is checked");
    {
        orders::OrderLine one = make_line(kLineA);
        one.quantity_scaled = 1'500;
        one.unit_price_minor = 20'001;
        const engine::MoneyResult amount = orders::line_amount(one);
        check(amount.ok, "a representable line amount succeeds");
        check(amount.value.minor == 30'002,
              "half a paisa is rounded away from zero by the shared money rule");

        orders::OrderLine two = make_line(kLineB);
        two.quantity_scaled = 2'000;
        two.unit_price_minor = 25'000;
        const engine::MoneyResult total = orders::order_total({one, two});
        check(total.ok, "representable lines total successfully");
        check(total.value.minor == 80'002, "the total is the sum of line snapshots");

        const engine::MoneyResult empty = orders::order_total({});
        check(empty.ok && empty.value.minor == 0, "an empty order totals zero");

        orders::OrderLine huge = make_line(kLineC);
        huge.quantity_scaled = std::numeric_limits<std::int64_t>::max();
        huge.unit_price_minor = pricing::kMaxAmountMinor;
        check(!orders::line_amount(huge).ok, "line multiplication refuses overflow");

        orders::OrderLine max_a = make_line(kLineA);
        max_a.quantity_scaled = engine::Quantity::kScale;
        max_a.unit_price_minor = pricing::kMaxAmountMinor;
        orders::OrderLine max_b = max_a;
        max_b.id = kLineB;
        max_b.quantity_scaled = 10'000'000;
        check(!orders::order_total({max_a, max_b}).ok,
              "an order total refuses overflow rather than wrapping");
    }

    section("order validation rejects impossible records");
    {
        orders::Order valid = make_order();
        check(!refused([&] { orders::validate(valid); }), "a valid order is accepted");

        valid.party_id.clear();
        check(!refused([&] { orders::validate(valid); }), "a walk-in order is valid");

        orders::Order bad = make_order();
        bad.id.clear();
        check(refused([&] { orders::validate(bad); }), "an order needs an id");

        bad = make_order();
        bad.created_at = -1;
        check(refused([&] { orders::validate(bad); }), "negative creation time is refused");

        bad = make_order();
        bad.promised_at = -1;
        check(refused([&] { orders::validate(bad); }), "negative promised time is refused");

        bad = make_order();
        bad.created_at = 0;
        check(refused([&] { orders::validate(bad); }), "zero creation time is refused");
        bad = make_order();
        bad.created_by = "  ";
        check(refused([&] { orders::validate(bad); }), "a blank creator is refused");

        bad = make_order();
        bad.cancel_reason = "contradiction";
        check(refused([&] { orders::validate(bad); }),
              "an open order cannot carry cancellation evidence");

        bad = make_order();
        bad.state = orders::OrderState::Cancelled;
        bad.cancel_reason = "   \t";
        check(refused([&] { orders::validate(bad); }),
              "a whitespace-only cancellation reason is refused");
        bad.cancel_reason = "Customer called it off";
        check(refused([&] { orders::validate(bad); }),
              "a reason without cancellation time and person is incomplete");
        bad.cancelled_at = 2000;
        bad.cancelled_by = "tester";
        check(!refused([&] { orders::validate(bad); }),
              "complete cancellation evidence is valid");

        bad = make_order();
        bad.state = static_cast<orders::OrderState>(99);
        check(refused([&] { orders::validate(bad); }),
              "an unknown type-legal state is refused");
    }

    section("line validation closes refund and overflow doors");
    {
        orders::OrderLine valid = make_line();
        check(!refused([&] { orders::validate(valid); }), "a valid line is accepted");

        orders::OrderLine bad = make_line();
        bad.id.clear();
        check(refused([&] { orders::validate(bad); }), "a line needs an id");
        bad = make_line();
        bad.order_id.clear();
        check(refused([&] { orders::validate(bad); }), "a line needs an order");
        bad = make_line();
        bad.product_id.clear();
        bad.description = " \n";
        check(refused([&] { orders::validate(bad); }),
              "an off-catalog line needs a description");
        bad = make_line();
        bad.position = -1;
        check(refused([&] { orders::validate(bad); }), "negative position is refused");
        bad.position = std::numeric_limits<std::int64_t>::max();
        check(refused([&] { orders::validate(bad); }),
              "the position with no valid successor is refused");
        bad = make_line();
        bad.quantity_scaled = 0;
        check(refused([&] { orders::validate(bad); }), "zero quantity is refused");
        bad.quantity_scaled = -1;
        check(refused([&] { orders::validate(bad); }),
              "negative quantity cannot disguise a refund");
        bad = make_line();
        bad.unit_price_minor = -1;
        check(refused([&] { orders::validate(bad); }),
              "negative price cannot disguise a refund");
        bad.unit_price_minor = pricing::kMaxAmountMinor + 1;
        check(refused([&] { orders::validate(bad); }), "absurd price is refused");
        bad.unit_price_minor = pricing::kMaxAmountMinor;
        check(!refused([&] { orders::validate(bad); }),
              "the documented maximum is accepted exactly");
        bad = make_line();
        bad.added_at = 0;
        check(refused([&] { orders::validate(bad); }), "zero added time is refused");
        bad = make_line();
        bad.added_by = " \t";
        check(refused([&] { orders::validate(bad); }), "a blank line creator is refused");
        bad = make_line();
        bad.price_source = static_cast<pricing::RateSource>(99);
        check(refused([&] { orders::validate(bad); }),
              "an unknown type-legal price source is refused");
        bad = make_line();
        bad.price_source = pricing::RateSource::None;
        check(refused([&] { orders::validate(bad); }),
              "a line cannot have a price with no provenance");
        bad.price_overridden = true;
        check(refused([&] { orders::validate(bad); }),
              "an override with no reason is refused");
        bad.price_reason = "One-off work";
        check(!refused([&] { orders::validate(bad); }),
              "a standalone override with a reason is valid");
        bad = make_line();
        bad.price_reason = "reason without override";
        check(refused([&] { orders::validate(bad); }),
              "a reason without an override is contradictory");
        bad = make_line();
        bad.quantity_scaled = std::numeric_limits<std::int64_t>::max();
        bad.unit_price_minor = pricing::kMaxAmountMinor;
        check(refused([&] { orders::validate(bad); }),
              "a type-legal overflowing product is refused");
    }

    section("quotation provenance is complete, optional, and frozen in the order row");
    {
        orders::Order direct = make_order();
        check(!refused([&] { orders::validate(direct); }),
              "a direct order deliberately has no quotation source");

        orders::Order converted = make_order();
        converted.source_quotation_id = "47000000000000000000000000000001";
        converted.source_revision_id = "47000000000000000000000000000002";
        converted.source_revision = 7;
        check(!refused([&] { orders::validate(converted); }),
              "one exact quotation revision is valid provenance");

        const orders::Order restored = orders::order_from_row(orders::to_row(converted));
        check(restored.source_quotation_id == converted.source_quotation_id,
              "the quotation identity survives storage");
        check(restored.source_revision_id == converted.source_revision_id,
              "the revision identity survives storage");
        check(restored.source_revision == 7,
              "the accepted revision number survives storage");

        orders::Order partial = make_order();
        partial.source_quotation_id = converted.source_quotation_id;
        check(refused([&] { orders::validate(partial); }),
              "a quotation without a revision is refused");
        partial.source_revision_id = converted.source_revision_id;
        check(refused([&] { orders::validate(partial); }),
              "source identities without a positive revision number are refused");
        partial.source_revision = -1;
        check(refused([&] { orders::validate(partial); }),
              "a negative source revision is refused");
    }

    section("row mappings preserve snapshots and tame unknown enum values");
    {
        orders::Order order = make_order();
        order.promised_at = 8000;
        order.note = "Use blue eyelets";
        const orders::Order restored = orders::order_from_row(orders::to_row(order));
        check(restored.id == order.id, "order id round-trips");
        check(restored.party_id == order.party_id, "order party round-trips");
        check(restored.promised_at == 8000, "promise round-trips");
        check(restored.note == order.note, "note round-trips");

        orders::OrderLine line = make_line();
        line.position = 7;
        line.price_overridden = true;
        line.price_reason = "Customer supplied material";
        const orders::OrderLine line_back = orders::line_from_row(orders::to_row(line));
        check(line_back.position == 7, "line position round-trips");
        check(line_back.unit_price_minor == line.unit_price_minor,
              "price snapshot round-trips");
        check(line_back.price_source == pricing::RateSource::Default,
              "price source round-trips");
        check(line_back.price_overridden, "override flag round-trips");
        check(line_back.price_reason == line.price_reason, "override reason round-trips");

        engine::Row damaged = orders::to_row(line);
        damaged.set("price_source", engine::Value::integer(99));
        check(orders::line_from_row(damaged).price_source == pricing::RateSource::None,
              "an unknown stored source becomes None, not a plausible rate");

        engine::Row future_order = orders::to_row(order);
        future_order.set("state", engine::Value::integer(99));
        const orders::Order fail_closed = orders::order_from_row(future_order);
        check(fail_closed.state == orders::OrderState::Cancelled,
              "an unknown stored order state fails closed");
        check(!orders::can_change(fail_closed.state),
              "a future state cannot be edited by this older build");
    }

    section("next position refuses the signed-integer boundary");
    {
        RowReader empty;
        const auto first = orders::data::next_position(empty, kOrderA);
        check(first && *first == 0, "an empty order starts at position zero");

        orders::OrderLine line = make_line();
        line.position = 7;
        RowReader normal{{orders::to_row(line)}};
        const auto eighth = orders::data::next_position(normal, kOrderA);
        check(eighth && *eighth == 8, "a normal position advances by one");

        line.position = std::numeric_limits<std::int64_t>::max();
        RowReader exhausted{{orders::to_row(line)}};
        check(!orders::data::next_position(exhausted, kOrderA),
              "INT64_MAX returns no position instead of overflowing");
    }

    section("equal positions have deterministic id order");
    {
        engine::MemoryStore store;
        store.define_table(orders::tables::kOrderLine, "id");
        auto transaction = store.begin();
        orders::OrderLine later_id = make_line(kLineB);
        later_id.position = 4;
        orders::OrderLine earlier_id = make_line(kLineA);
        earlier_id.position = 4;
        transaction->insert(orders::tables::kOrderLine, orders::to_row(later_id));
        transaction->insert(orders::tables::kOrderLine, orders::to_row(earlier_id));
        transaction->commit();
        const std::vector<orders::OrderLine> lines =
            orders::data::lines_for_order(store, kOrderA);
        check(lines.size() == 2, "both same-position lines are returned");
        check(lines.size() == 2 && lines[0].id == kLineA && lines[1].id == kLineB,
              "id breaks the tie identically on every device");
    }

    // ===================================================================
    // Part two: registry, permissions, transactions, pricing and outbox.
    // ===================================================================

    section("migration 14 creates both order tables");
    {
        Shop shop;
        shop.look([&](const engine::Store& store) {
            check(store.has_table(orders::tables::kOrder), "customer_order exists");
            check(store.has_table(orders::tables::kOrderLine), "order_line exists");
        });
    }

    section("an order is created once and keeps its creation evidence");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const modules::Outcome made = create_order(shop, owner, kOrderA);
        check(made.ok, "order_create succeeds");
        check(made.queued, "an offline-capable create is queued");

        shop.look([&](const engine::Store& store) {
            const auto order = orders::data::find_order(store, kOrderA);
            check(order.has_value(), "the order persisted");
            if (order) {
                check(order->party_id == kPartyA, "the customer persisted");
                check(order->state == orders::OrderState::Open, "the order starts open");
                check(order->note == "Counter order", "the note persisted");
                check(order->promised_at == 1'800'000'000'000,
                      "the promise persisted exactly");
                check(!order->created_by.empty(), "the creator was recorded");
                check(order->created_at > 0, "the creation time was recorded");
            }
        });

        const modules::Outcome duplicate = create_order(shop, owner, kOrderA);
        check(!duplicate.ok, "creating over an existing order is refused");
    }

    section("optional create fields are optional, not weakly typed");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const modules::Outcome numeric_party = shop.run(
            protocol::OperationId::order_create, kOrderA,
            fields({}, {{"party_id", 7}}), owner, next_key());
        check(!numeric_party.ok, "a numeric customer is not silently treated as a walk-in");

        const modules::Outcome text_promise = shop.run(
            protocol::OperationId::order_create, kOrderB,
            fields({{"promised_at", "tomorrow"}}), owner, next_key());
        check(!text_promise.ok, "a text promise is not silently treated as no promise");

        const modules::Outcome numeric_note = shop.run(
            protocol::OperationId::order_create, kOrderC,
            fields({}, {{"note", 9}}), owner, next_key());
        check(!numeric_note.ok, "a numeric note is not silently cleared");

        shop.look([&](const engine::Store& store) {
            check(orders::data::orders_for_party(store, "").empty(),
                  "none of the malformed creates wrote a walk-in order");
        });
    }

    section("explicit null is not silently treated as absent");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        engine::Row null_customer;
        null_customer.set("party_id", engine::Value::null());
        check(!shop.run(protocol::OperationId::order_create, kOrderA,
                        engine::encode_payload(null_customer), owner, next_key()).ok,
              "an explicit null customer is refused");
        check(set_default(shop, owner, kProductA, 20'000).ok, "a price exists");
        check(create_order(shop, owner, kOrderB).ok, "a valid order exists");
        engine::Row null_quantity;
        null_quantity.set("order_id", engine::Value::text(kOrderB));
        null_quantity.set("product_id", engine::Value::text(kProductA));
        null_quantity.set("quantity_scaled", engine::Value::null());
        check(!shop.run(protocol::OperationId::order_line_add, kLineA,
                        engine::encode_payload(null_quantity), owner, next_key()).ok,
              "an explicit null quantity is refused");
        engine::Row missing_order;
        missing_order.set("product_id", engine::Value::text(kProductA));
        missing_order.set("quantity_scaled", engine::Value::integer(1000));
        check(!shop.run(protocol::OperationId::order_line_add, kLineB,
                        engine::encode_payload(missing_order), owner, next_key()).ok,
              "a missing order_id is refused");
        engine::Row missing_quantity;
        missing_quantity.set("order_id", engine::Value::text(kOrderB));
        missing_quantity.set("product_id", engine::Value::text(kProductA));
        check(!shop.run(protocol::OperationId::order_line_add, kLineC,
                        engine::encode_payload(missing_quantity), owner, next_key()).ok,
              "a missing quantity is refused");
        shop.look([&](const engine::Store& store) {
            check(!orders::data::find_order(store, kOrderA),
                  "the null-customer create wrote nothing");
            check(orders::data::lines_for_order(store, kOrderB).empty(),
                  "all malformed lines rolled back");
        });
    }

    section("Unicode, special characters and long text round-trip exactly");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        std::string note(64 * 1024, 'x');
        note += "\nquotes: \" ' <tag> & ";
        note += "\xE0\xA4\xA8\xE0\xA4\xAE\xE0\xA4\xB8\xE0\xA5\x8D\xE0\xA4\xA4\xE0\xA5\x87";
        check(shop.run(protocol::OperationId::order_create, kOrderA,
                       fields({{"party_id", kPartyA}, {"note", note}}),
                       owner, next_key()).ok,
              "a long Unicode and special-character note is accepted");
        shop.look([&](const engine::Store& store) {
            const auto order = orders::data::find_order(store, kOrderA);
            check(order && order->note.size() == note.size(), "the long note length is exact");
            check(order && order->note == note, "the note bytes round-trip exactly");
        });
    }

    section("walk-in orders are first-class, not fake customers");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(create_order(shop, owner, kOrderA, "").ok, "a walk-in order is accepted");
        shop.look([&](const engine::Store& store) {
            const auto order = orders::data::find_order(store, kOrderA);
            check(order && order->party_id.empty(), "the absent customer stays absent");
            check(orders::data::orders_for_party(store, "").size() == 1,
                  "walk-in orders remain queryable without inventing a person");
        });
    }

    section("updates are partial and cannot move customer money");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(create_order(shop, owner, kOrderA).ok, "the order exists");

        std::int64_t created_at = 0;
        std::string created_by;
        shop.look([&](const engine::Store& store) {
            const auto before = orders::data::find_order(store, kOrderA);
            if (before) {
                created_at = before->created_at;
                created_by = before->created_by;
            }
        });

        check(shop.run(protocol::OperationId::order_update, kOrderA,
                       fields({{"note", "Phone before printing"}}), owner, next_key()).ok,
              "a note-only update succeeds");
        shop.look([&](const engine::Store& store) {
            const auto after = orders::data::find_order(store, kOrderA);
            check(after && after->note == "Phone before printing", "the note changed");
            check(after && after->promised_at == 1'800'000'000'000,
                  "an absent promised_at was not cleared");
            check(after && after->created_at == created_at, "created_at was preserved");
            check(after && after->created_by == created_by, "created_by was preserved");
        });

        const modules::Outcome moved = shop.run(
            protocol::OperationId::order_update, kOrderA,
            fields({{"party_id", kPartyB}}), owner, next_key());
        check(!moved.ok, "an order cannot move to another customer");

        const modules::Outcome text_date = shop.run(
            protocol::OperationId::order_update, kOrderA,
            fields({{"promised_at", "tomorrow"}}), owner, next_key());
        check(!text_date.ok, "a promised date sent as text is refused, not changed to zero");
    }

    section("a line snapshots the resolved price once");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(set_default(shop, owner, kProductA, 20'000).ok, "the first price exists");
        check(create_order(shop, owner, kOrderA).ok, "the order exists");
        check(add_line(shop, owner, kLineA, kOrderA, kProductA, 1'500).ok,
              "the first line is added");

        check(set_default(shop, owner, kProductA, 30'000).ok, "the rate changes later");
        check(add_line(shop, owner, kLineB, kOrderA, kProductA, 2'000).ok,
              "a second line is added under the new rate");

        shop.look([&](const engine::Store& store) {
            const auto first = orders::data::find_line(store, kLineA);
            const auto second = orders::data::find_line(store, kLineB);
            check(first && first->unit_price_minor == 20'000,
                  "the earlier line kept its agreed price");
            check(second && second->unit_price_minor == 30'000,
                  "the later line used the later price");
            check(first && first->price_source == pricing::RateSource::Default,
                  "the reason for the first price was snapshotted");
            check(first && first->position == 0, "the first line is at position zero");
            check(second && second->position == 1, "the next line follows it");
            const engine::MoneyResult total = orders::data::total_for_order(store, kOrderA);
            check(total.ok && total.value.minor == 90'000,
                  "the total uses both snapshots and fractional quantities");
        });
    }

    section("a party rate is resolved against the order customer");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(set_default(shop, owner, kProductA, 50'000).ok, "the standard price exists");
        check(shop.run(protocol::OperationId::rate_set, kRateA,
                       fields({{"product_id", kProductA}, {"party_id", kPartyA}},
                              {{"amount_minor", 40'000}}),
                       owner, next_key()).ok,
              "the customer's agreed rate exists");
        check(create_order(shop, owner, kOrderA, kPartyA).ok, "the customer order exists");
        check(create_order(shop, owner, kOrderB, kPartyB).ok, "another customer order exists");
        check(add_line(shop, owner, kLineA, kOrderA, kProductA, 1000).ok,
              "the agreed-rate line is added");
        check(add_line(shop, owner, kLineB, kOrderB, kProductA, 1000).ok,
              "the standard-rate line is added");

        shop.look([&](const engine::Store& store) {
            const auto agreed = orders::data::find_line(store, kLineA);
            const auto standard = orders::data::find_line(store, kLineB);
            check(agreed && agreed->unit_price_minor == 40'000,
                  "the named customer received the agreed rate");
            check(agreed && agreed->price_source == pricing::RateSource::PartyRate,
                  "the party-rate source was frozen");
            check(standard && standard->unit_price_minor == 50'000,
                  "another customer did not receive somebody else's rate");
        });
    }

    section("no price means no line, never a free line");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(create_order(shop, owner, kOrderA).ok, "the order exists");
        const modules::Outcome missing =
            add_line(shop, owner, kLineA, kOrderA, kProductB, 1000);
        check(!missing.ok, "a product with no price is refused");
        check(!missing.error.empty(), "the refusal explains the missing price");
        shop.look([&](const engine::Store& store) {
            check(!orders::data::find_line(store, kLineA), "no zero-priced line was written");
            check(orders::data::lines_for_order(store, kOrderA).empty(),
                  "the order remains untouched");
        });
    }

    section("a recorded override prices an off-catalog line");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(create_order(shop, owner, kOrderA).ok, "the order exists");
        check(shop.run(protocol::OperationId::rate_override, kOverride,
                       fields({{"line_id", kLineA}, {"reason", "One-off hand lettering"}},
                              {{"overridden_minor", 75'000}}),
                       owner, next_key()).ok,
              "the one-off price is recorded before the line");
        check(add_line(shop, owner, kLineA, kOrderA, "", 1000, "Hand-lettered board").ok,
              "the off-catalog line is added");

        shop.look([&](const engine::Store& store) {
            const auto line = orders::data::find_line(store, kLineA);
            check(line && line->product_id.empty(), "the line remains off-catalog");
            check(line && line->unit_price_minor == 75'000, "the override became the snapshot");
            check(line && line->price_overridden, "the deviation is marked");
            check(line && line->price_reason == "One-off hand lettering",
                  "the reason travels with the price");
            check(line && line->price_source == pricing::RateSource::None,
                  "no imaginary base price was invented");
        });
    }

    section("bad and duplicate lines roll back completely");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(set_default(shop, owner, kProductA, 20'000).ok, "a price exists");
        check(create_order(shop, owner, kOrderA).ok, "the order exists");

        const modules::Outcome text_quantity = shop.run(
            protocol::OperationId::order_line_add, kLineA,
            fields({{"order_id", kOrderA}, {"product_id", kProductA},
                    {"description", "Banner"}, {"quantity_scaled", "1000"}}),
            owner, next_key());
        check(!text_quantity.ok, "a quantity sent as text is refused, not read as zero");

        check(add_line(shop, owner, kLineA, kOrderA, kProductA, 1000).ok,
              "the valid line is added once");
        const modules::Outcome duplicate =
            add_line(shop, owner, kLineA, kOrderA, kProductA, 2000);
        check(!duplicate.ok, "the same line id cannot be recorded twice");

        const modules::Outcome no_order =
            add_line(shop, owner, kLineB, kOrderB, kProductA, 1000);
        check(!no_order.ok, "a line for a missing order is refused");

        const modules::Outcome numeric_product = shop.run(
            protocol::OperationId::order_line_add, kLineC,
            fields({{"order_id", kOrderA}, {"description", "Banner"}},
                   {{"product_id", 7}, {"quantity_scaled", 1000}}),
            owner, next_key());
        check(!numeric_product.ok,
              "a numeric product is not silently turned into an off-catalog line");

        const modules::Outcome numeric_description = shop.run(
            protocol::OperationId::order_line_add, kLineD,
            fields({{"order_id", kOrderA}, {"product_id", kProductA}},
                   {{"description", 7}, {"quantity_scaled", 1000}}),
            owner, next_key());
        check(!numeric_description.ok, "a numeric description is refused");

        shop.look([&](const engine::Store& store) {
            const std::vector<orders::OrderLine> lines =
                orders::data::lines_for_order(store, kOrderA);
            check(lines.size() == 1, "only the one valid line exists");
            check(lines.front().quantity_scaled == 1000,
                  "the duplicate did not overwrite the original");
            check(!orders::data::find_line(store, kLineB),
                  "the missing-order line was rolled back");
            check(!orders::data::find_line(store, kLineC),
                  "the malformed product line was rolled back");
            check(!orders::data::find_line(store, kLineD),
                  "the malformed description line was rolled back");
        });
    }

    section("cancellation freezes the order and preserves its first reason");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(set_default(shop, owner, kProductA, 20'000).ok, "a price exists");
        check(create_order(shop, owner, kOrderA).ok, "the order exists");
        check(add_line(shop, owner, kLineA, kOrderA, kProductA, 1000).ok,
              "the original line exists");

        const modules::Outcome blank_reason = shop.run(
            protocol::OperationId::order_cancel, kOrderA,
            fields({{"reason", "   "}}), owner, next_key());
        check(!blank_reason.ok, "a blank cancellation reason is refused");

        check(shop.run(protocol::OperationId::order_cancel, kOrderA,
                       fields({{"reason", "Customer no longer needs it"}}),
                       owner, next_key()).ok,
              "the explained cancellation succeeds");
        check(!shop.run(protocol::OperationId::order_update, kOrderA,
                        fields({{"note", "rewrite history"}}), owner, next_key()).ok,
              "a cancelled order cannot be edited");
        check(!add_line(shop, owner, kLineB, kOrderA, kProductA, 1000).ok,
              "a cancelled order cannot take another line");
        check(!shop.run(protocol::OperationId::order_cancel, kOrderA,
                        fields({{"reason", "different story"}}), owner, next_key()).ok,
              "a second cancellation cannot rewrite the reason");

        shop.look([&](const engine::Store& store) {
            const auto order = orders::data::find_order(store, kOrderA);
            check(order && order->state == orders::OrderState::Cancelled,
                  "the cancelled state persisted");
            check(order && order->cancel_reason == "Customer no longer needs it",
                  "the first reason was preserved");
            check(order && order->cancelled_at > 0, "the cancellation time was recorded");
            check(order && !order->cancelled_by.empty(), "the cancelling person was recorded");
            check(orders::data::lines_for_order(store, kOrderA).size() == 1,
                  "the original line remains as evidence");
        });
    }

    section("concurrent line additions serialize without duplicates or gaps");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(set_default(shop, owner, kProductA, 20'000).ok, "a price exists");
        check(create_order(shop, owner, kOrderA).ok, "the shared order exists");
        constexpr int kConcurrent = 16;
        std::vector<modules::Outcome> outcomes(static_cast<std::size_t>(kConcurrent));
        std::vector<std::thread> workers;
        workers.reserve(static_cast<std::size_t>(kConcurrent));
        for (int i = 0; i < kConcurrent; ++i) {
            workers.emplace_back([&, i] {
                outcomes[static_cast<std::size_t>(i)] =
                    add_line(shop, owner, concurrent_line_id(i), kOrderA,
                             kProductA, 1000, "Concurrent banner");
            });
        }
        for (std::thread& worker : workers) worker.join();
        bool all_ok = true;
        for (const modules::Outcome& outcome : outcomes) all_ok = all_ok && outcome.ok;
        check(all_ok, "all concurrent writes completed successfully");
        shop.look([&](const engine::Store& store) {
            const std::vector<orders::OrderLine> lines =
                orders::data::lines_for_order(store, kOrderA);
            check(lines.size() == static_cast<std::size_t>(kConcurrent),
                  "every concurrent line persisted exactly once");
            bool contiguous = lines.size() == static_cast<std::size_t>(kConcurrent);
            for (std::size_t i = 0; i < lines.size(); ++i) {
                contiguous = contiguous &&
                             lines[i].position == static_cast<std::int64_t>(i);
            }
            check(contiguous, "serialized writes assigned contiguous unique positions");
        });
    }

    section("write and cancel are separate permissions");
    {
        Shop shop;
        const engine::Session reader = reader_session();
        const engine::Session writer = writer_session();

        check(!create_order(shop, reader, kOrderA).ok,
              "order_create is refused without right_order_write");
        check(create_order(shop, writer, kOrderA).ok,
              "a writer may create an order");
        check(!shop.run(protocol::OperationId::order_cancel, kOrderA,
                        fields({{"reason", "not authorised"}}), writer, next_key()).ok,
              "a writer may not cancel without right_order_cancel");
        shop.look([&](const engine::Store& store) {
            const auto order = orders::data::find_order(store, kOrderA);
            check(order && order->state == orders::OrderState::Open,
                  "the refused cancellation changed nothing");
        });
    }

    section("an unsigned session is refused even with every right bit");
    {
        Shop shop;
        engine::Session unsigned_session;
        unsigned_session.rights.grant_all();
        check(!create_order(shop, unsigned_session, kOrderA).ok,
              "rights do not replace authentication");
        shop.look([&](const engine::Store& store) {
            check(!orders::data::find_order(store, kOrderA),
                  "the unsigned request wrote nothing");
        });
    }

    section("offline rules are enforced before the handler");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        check(create_order(shop, owner, kOrderA, kPartyA,
                           engine::ConnectionState::Offline).ok,
              "order_create is allowed offline");
        const modules::Outcome cancel = shop.run(
            protocol::OperationId::order_cancel, kOrderA,
            fields({{"reason", "offline attempt"}}), owner, next_key(),
            engine::ConnectionState::Offline);
        check(!cancel.ok, "order_cancel is refused offline");
        shop.look([&](const engine::Store& store) {
            const auto order = orders::data::find_order(store, kOrderA);
            check(order && order->state == orders::OrderState::Open,
                  "the offline refusal happened before any write");
        });
    }

    section("an idempotency replay does not create a second order");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const std::string key = next_key();
        const engine::Blob payload = fields({{"party_id", kPartyA}});
        const modules::Outcome first = shop.run(
            protocol::OperationId::order_create, kOrderA, payload, owner, key);
        const modules::Outcome replay = shop.run(
            protocol::OperationId::order_create, kOrderA, payload, owner, key);
        check(first.ok, "the first call succeeds");
        check(replay.ok && replay.replayed, "the same key is reported as a replay");
        shop.look([&](const engine::Store& store) {
            check(orders::data::orders_for_party(store, kPartyA).size() == 1,
                  "the replay did not run the create handler twice");
        });
    }

    section("damaged payloads fail without partial rows");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const engine::Blob garbage = {0x01, 0x02, 0x03, 0x04, 0x05};
        const modules::Outcome bad = shop.run(
            protocol::OperationId::order_create, kOrderA, garbage, owner, next_key());
        check(!bad.ok, "a damaged create payload is refused");
        check(bad.error == "This request could not be read. Please try it again.",
              "the refusal does not leak payload-decoder internals");

        bool corpus_refused = true;
        for (std::size_t length = 1; length <= 128; ++length) {
            engine::Blob hostile(length);
            std::uint32_t state = static_cast<std::uint32_t>(length * 2654435761U);
            for (unsigned char& byte : hostile) {
                state = state * 1664525U + 1013904223U;
                byte = static_cast<unsigned char>((state >> 24U) & 0xffU);
            }
            const modules::Outcome result = shop.run(
                protocol::OperationId::order_create, kOrderC,
                hostile, owner, next_key());
            corpus_refused = corpus_refused && !result.ok &&
                result.error == "This request could not be read. Please try it again.";
        }
        check(corpus_refused,
              "128 malformed payload shapes are refused without internal details");

        shop.look([&](const engine::Store& store) {
            check(!orders::data::find_order(store, kOrderA), "no partial order was written");
            check(!orders::data::find_order(store, kOrderC),
                  "the malformed corpus wrote no order");
        });
    }

    section("a missing record id is a loud wiring error");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        bool loud = false;
        try {
            shop.run(protocol::OperationId::order_create, "",
                     fields({{"party_id", kPartyA}}), owner, next_key());
        } catch (const modules::RegistryError&) {
            loud = true;
        }
        check(loud, "the registry rejects a synchronisable call with no ordering record");
        shop.look([&](const engine::Store& store) {
            check(orders::data::orders_for_party(store, kPartyA).empty(),
                  "and nothing was written");
        });
    }

    return squiflow::testing::report();
}
