#include "engine/audit/audit_log.hpp"
#include "engine/identity/session.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/module.hpp"
#include "modules/pricing/data/repository.hpp"
#include "modules/pricing/module.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/receivables/data/tables.hpp"
#include "modules/receivables/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"
#include "workflows/counter_sale.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace orders = squiflow::modules::orders;
namespace pricing = squiflow::modules::pricing;
namespace protocol = squiflow::protocol;
namespace receivables = squiflow::modules::receivables;
namespace workflows = squiflow::workflows;
using squiflow::testing::check;
using squiflow::testing::section;

namespace {

constexpr std::int64_t kNow = 1'800'000'000'000;
const std::string kPerson = "b1000000000000000000000000000001";
const std::string kDevice = "b1000000000000000000000000000002";
const std::string kProduct = "b2000000000000000000000000000001";
const std::string kSale = "b3000000000000000000000000000001";
const std::string kLine = "b4000000000000000000000000000001";
const std::string kPayment = "b5000000000000000000000000000001";

std::int64_t now() { return kNow; }

engine::Session owner() {
    engine::Session session;
    session.person = engine::record_id_from_string(kPerson);
    session.device = engine::record_id_from_string(kDevice);
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

engine::Blob request(std::string product = kProduct,
                     std::string line = kLine,
                     std::string payment = kPayment,
                     bool explicit_price = false) {
    engine::Row row;
    row.set("line_id", engine::Value::text(std::move(line)));
    row.set("payment_id", engine::Value::text(std::move(payment)));
    row.set("product_id", engine::Value::text(std::move(product)));
    row.set("description", engine::Value::text("Walk-in print"));
    row.set("quantity_scaled", engine::Value::integer(2'000));
    row.set("paid_at", engine::Value::integer(kNow - 100));
    row.set("method", engine::Value::text("cash"));
    row.set("receipt_series", engine::Value::text("RCPT"));
    row.set("receipt_number", engine::Value::integer(7));
    if (explicit_price) {
        row.set("unit_price_minor", engine::Value::integer(600));
        row.set("price_reason", engine::Value::text("Counter agreed price"));
    }
    return engine::encode_payload(row);
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database{};

    Shop() {
        registry.add(pricing::make_module(now));
        registry.add(orders::make_module(now));
        registry.add(receivables::make_module(now));
        registry.install_workflow(workflows::make_counter_sale(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
        database->write([](engine::Transaction& transaction) {
            pricing::DefaultRate rate;
            rate.product_id = kProduct;
            rate.amount_minor = 500;
            rate.updated_at = kNow - 1'000;
            rate.updated_by = kPerson;
            pricing::data::save_default_rate(transaction, rate);
        });
    }

    modules::Outcome run(const std::string& sale, const std::string& key,
                         engine::Blob body, const engine::Session& session = owner()) {
        modules::Call call;
        call.operation = protocol::OperationId::counter_sale;
        call.record_id = sale;
        call.idempotency_key = key;
        call.payload = std::move(body);
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
    section("walk-in sale payment and receipt are atomic");
    Shop shop;
    check(shop.registry.workflow_available(protocol::OperationId::counter_sale),
          "production definition is available");
    const auto sold = shop.run(kSale, "counter-sale", request());
    check(sold.ok && sold.queued && !sold.replayed,
          "counter sale commits and enters the outbox");
    shop.database->read([](const engine::Store& store) {
        const auto order = orders::data::find_order(store, kSale);
        const auto line = orders::data::find_line(store, kLine);
        const auto payment = receivables::data::find_payment(store, kPayment);
        check(order && order->party_id.empty(), "walk-in needs no customer record");
        check(line && line->unit_price_minor == 500 &&
                          line->price_source == pricing::RateSource::Default,
              "remembered default price is frozen");
        check(payment && payment->amount_minor == 1'000,
              "payment equals checked quantity-times-price total");
        check(payment && payment->receipt_series == "RCPT" &&
                           payment->receipt_number == 7,
              "receipt evidence is retained");
    });
    check(shop.count(receivables::tables::kInvoice) == 0,
          "counter sale opens no invoice cycle");
    check(shop.count(engine::AuditLog::table_name()) == 1 &&
              shop.count(engine::Outbox::table_name()) == 1,
          "one audit and one outbox row are written");

    section("replay and explicit off-catalog price");
    const auto replay = shop.run(kSale, "counter-sale", request());
    check(replay.ok && replay.replayed && !replay.queued,
          "same idempotency key replays");
    const std::string second_sale = "b3000000000000000000000000000002";
    const std::string second_line = "b4000000000000000000000000000002";
    const std::string second_payment = "b5000000000000000000000000000002";
    const auto explicit_sale = shop.run(
        second_sale, "counter-explicit",
        request("", second_line, second_payment, true));
    check(explicit_sale.ok, "off-catalog explicit price is accepted");
    shop.database->read([&](const engine::Store& store) {
        const auto line = orders::data::find_line(store, second_line);
        const auto payment = receivables::data::find_payment(store, second_payment);
        check(line && line->price_overridden &&
                          line->price_source == pricing::RateSource::None &&
                          line->price_reason == "Counter agreed price",
              "explicit price keeps its reason and honest origin");
        check(payment && payment->amount_minor == 1'200,
              "explicit price determines exact payment");
    });

    section("refusals leave no partial money or order");
    const std::string bad_sale = "b3000000000000000000000000000003";
    const std::string bad_line = "b4000000000000000000000000000003";
    const std::string bad_payment = "b5000000000000000000000000000003";
    check(!shop.run(bad_sale, "missing-price",
                    request("b2000000000000000000000000000099", bad_line,
                            bad_payment, false)).ok,
          "missing remembered and explicit price is refused");
    engine::Session denied = owner();
    denied.rights.revoke(protocol::RightId::right_order_write);
    check(!shop.run(bad_sale, "denied", request(kProduct, bad_line, bad_payment),
                    denied).ok,
          "workflow right is enforced before the handler");
    check(!shop.run(bad_sale, "damaged", {1, 2, 3}).ok,
          "damaged request is refused");
    check(shop.count(orders::tables::kOrder) == 2 &&
              shop.count(orders::tables::kOrderLine) == 2 &&
              shop.count(receivables::tables::kPayment) == 2,
          "refusals add no partial business rows");
    check(shop.count(engine::AuditLog::table_name()) == 2 &&
              shop.count(engine::Outbox::table_name()) == 2,
          "refusals add no audit or outbox rows");

    return squiflow::testing::report();
}
