#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "engine/audit/audit_log.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/jobs/data/repository.hpp"
#include "modules/jobs/module.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"
#include "workflows/order_to_jobs.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace e = squiflow::engine;
namespace m = squiflow::modules;
namespace j = squiflow::modules::jobs;
namespace o = squiflow::modules::orders;
namespace p = squiflow::protocol;
namespace w = squiflow::workflows;

namespace {
constexpr std::int64_t kNow = 1'900'000'000'000LL;
std::int64_t now() { return kNow; }
const std::string Person = "71000000000000000000000000000001";
const std::string Device = "71000000000000000000000000000002";
const std::string Order = "72000000000000000000000000000001";
const std::string Order2 = "72000000000000000000000000000002";
const std::string Line1 = "73000000000000000000000000000001";
const std::string Line2 = "73000000000000000000000000000002";
const std::string Line3 = "73000000000000000000000000000003";
const std::string Root1 = "74000000000000000000000000000001";
const std::string Root2 = "74000000000000000000000000000002";
const std::string DirectJob = "75000000000000000000000000000001";
const std::string Party = "76000000000000000000000000000001";
const std::string Quote = "77000000000000000000000000000001";
const std::string Revision = "77000000000000000000000000000002";

e::Session owner() {
    e::Session session;
    session.person = e::record_id_from_string(Person);
    session.device = e::record_id_from_string(Device);
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

e::Blob payload(
    std::initializer_list<std::pair<std::string, std::string>> texts = {},
    std::initializer_list<std::pair<std::string, std::int64_t>> numbers = {}) {
    e::Row row;
    for (const auto& [name, value] : texts) row.set(name, e::Value::text(value));
    for (const auto& [name, value] : numbers) row.set(name, e::Value::integer(value));
    return e::encode_payload(row);
}

struct Shop {
    m::Registry registry{now};
    std::unique_ptr<e::Database> database;

    Shop() {
        registry.add(o::make_module(now));
        registry.add(j::make_module(now));
        registry.install_workflow(w::make_order_to_jobs(now));
        e::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<e::Database>(
            std::make_unique<e::MemoryStore>(), std::move(runner));
        database->open();
    }

    void seed_order(const std::string& id, const std::vector<std::string>& line_ids,
                    bool cancelled = false, bool from_quote = false,
                    const std::string& description = "Printed banner") {
        database->write([&](e::Transaction& transaction) {
            o::Order order;
            order.id = id;
            order.party_id = Party;
            order.promised_at = kNow + 20'000;
            order.note = "order note";
            order.created_at = kNow - 20'000;
            order.created_by = Person;
            if (from_quote) {
                order.source_quotation_id = Quote;
                order.source_revision_id = Revision;
                order.source_revision = 3;
            }
            if (cancelled) {
                order.state = o::OrderState::Cancelled;
                order.cancelled_at = kNow - 10'000;
                order.cancelled_by = Person;
                order.cancel_reason = "customer stopped";
            }
            o::data::save_order(transaction, order);
            std::int64_t position = 0;
            for (const std::string& line_id : line_ids) {
                o::OrderLine line;
                line.id = line_id;
                line.order_id = id;
                line.position = position++;
                line.product_id = "78000000000000000000000000000001";
                line.description = description + " #" + std::to_string(position);
                line.quantity_scaled = position * 1000;
                line.unit_price_minor = position * 1250;
                line.price_source = position == 1
                    ? m::pricing::RateSource::PartyRate
                    : m::pricing::RateSource::Default;
                line.added_at = kNow - 15'000;
                line.added_by = Person;
                o::data::save_line(transaction, line);
            }
        });
    }

    m::Outcome convert(const std::string& root, const std::string& key,
                       const e::Blob& body,
                       e::ConnectionState connection = e::ConnectionState::Online) {
        m::Call call;
        call.operation = p::OperationId::order_to_jobs;
        call.record_id = root;
        call.idempotency_key = key;
        call.payload = body;
        return registry.run(*database, call, owner(), connection);
    }

    m::Outcome direct_job() {
        m::Call call;
        call.operation = p::OperationId::job_create;
        call.record_id = DirectJob;
        call.idempotency_key = "direct-job";
        call.payload = payload({{"title", "Direct walk-in"},
                                {"description", "No order exists"}},
                               {{"quantity_scaled", 1000},
                                {"unit_price_minor", 500}});
        return registry.run(*database, call, owner(), e::ConnectionState::Offline);
    }
};

std::size_t count(const Shop& shop, const std::string& table) {
    std::size_t result = 0;
    shop.database->read([&](const e::Store& store) { result = store.count(table); });
    return result;
}

}  // namespace

int main() {
    section("one line becomes one draft job offline without repricing");
    {
        Shop shop;
        shop.seed_order(Order, {Line1});
        const auto before = [&] {
            std::vector<o::OrderLine> lines;
            shop.database->read([&](const e::Store& store) {
                lines = o::data::lines_for_order(store, Order);
            });
            return lines;
        }();
        const m::Outcome made = shop.convert(
            Root1, "one-line", payload({{"order_id", Order}}),
            e::ConnectionState::Offline);
        check(made.ok && made.queued && !made.replayed,
              "offline-allowed conversion succeeds and queues");
        const std::string job_id = w::order_job_id(Root1, Line1);
        shop.database->read([&](const e::Store& store) {
            const auto job = j::data::find_job(store, job_id);
            check(job.has_value(), "derived job exists");
            check(job && job->state == j::JobState::Draft && job->ticket_number == 0,
                  "workflow creates an unnumbered draft");
            check(job && job->source_order_id == Order &&
                          job->source_order_line_id == Line1,
                  "exact order and line provenance retained");
            check(job && job->source_quotation_id.empty(),
                  "direct order invents no quotation provenance");
            check(job && job->party_id == Party && job->quantity_scaled == 1000,
                  "customer and quantity snapshot copied");
            check(job && job->unit_price_minor == 1250 &&
                          job->total_price_minor == 1250 &&
                          job->rate_origin == e::RateOrigin::PartySpecific,
                  "frozen price and origin copy without repricing");
            const auto after = o::data::lines_for_order(store, Order);
            check(after.size() == before.size() &&
                          after.front().unit_price_minor == before.front().unit_price_minor,
                  "source order remains unchanged");
            check(store.find(e::Outbox::table_name(), "one-line").has_value(),
                  "one outbox row recorded");
            check(store.find(e::AuditLog::table_name(), "one-line").has_value(),
                  "one audit row recorded");
        });
        check(count(shop, j::tables::kJob) == 1, "exactly one job created");

        const m::Outcome replay = shop.convert(
            Root1, "one-line", payload({{"order_id", Order}}));
        check(replay.ok && replay.replayed && !replay.queued,
              "same idempotency key replays before handler");
        check(count(shop, j::tables::kJob) == 1 &&
                      count(shop, e::Outbox::table_name()) == 1 &&
                      count(shop, e::AuditLog::table_name()) == 1,
              "replay duplicates no business audit or outbox rows");

        const m::Outcome duplicate = shop.convert(
            Root2, "other-key", payload({{"order_id", Order}}));
        check(!duplicate.ok, "different key cannot derive the same source line twice");
        check(count(shop, j::tables::kJob) == 1,
              "different-key duplicate adds no job");

        check(shop.direct_job().ok, "ordinary job creation still works without an order");
        shop.database->read([&](const e::Store& store) {
            const auto direct = j::data::find_job(store, DirectJob);
            check(direct && direct->source_order_id.empty() &&
                            direct->source_order_line_id.empty(),
                  "direct job remains independent of orders");
        });
    }

    section("several lines become several jobs and preserve upstream quotation evidence");
    {
        Shop shop;
        const std::string unicode = "Unicode \xE0\xA4\x9B\xE0\xA4\xAA\xE0\xA4\xBE\xE0\xA4\x88 \xE2\x9C\x93 ";
        shop.seed_order(Order, {Line1, Line2, Line3}, false, true,
                        unicode + std::string(16'384, 'x'));
        const m::Outcome made = shop.convert(
            Root1, "many", payload({{"order_id", Order},
                                     {"title_prefix", "Ticket"},
                                     {"note", "override note"},
                                     {"specifications", "matte / exact"}},
                                    {{"promised_at", kNow + 30'000},
                                     {"deadline_at", kNow + 40'000}}));
        check(made.ok, "three-line conversion succeeds");
        check(count(shop, j::tables::kJob) == 3, "one job per order line");
        shop.database->read([&](const e::Store& store) {
            for (const std::string& line : {Line1, Line2, Line3}) {
                const auto job = j::data::job_for_order_line(store, Order, line);
                check(job.has_value(), "each exact source line has a job");
                check(job && job->source_quotation_id == Quote,
                      "quotation provenance passes through the order");
                check(job && job->title.starts_with("Ticket Unicode") &&
                              job->description.size() > 16'000,
                      "Unicode and large text copy intact");
                check(job && job->promised_at == kNow + 30'000 &&
                              job->deadline_at == kNow + 40'000 &&
                              job->note == "override note" &&
                              job->specifications == "matte / exact",
                      "explicit workflow overrides apply consistently");
            }
        });
    }

    section("an intentional selected subset can be completed later");
    {
        Shop shop;
        shop.seed_order(Order, {Line1, Line2, Line3});
        check(shop.convert(Root1, "subset-a",
                           payload({{"order_id", Order},
                                    {"mode", "selected_lines"},
                                    {"line_ids", Line2}})).ok,
              "one selected line converts");
        check(count(shop, j::tables::kJob) == 1, "only selected line converted");
        check(shop.convert(Root2, "subset-b",
                           payload({{"order_id", Order},
                                    {"mode", "selected_lines"},
                                    {"line_ids", Line1 + "," + Line3}})).ok,
              "remaining explicit subset converts with another identity");
        check(count(shop, j::tables::kJob) == 3, "all three lines now have one job");
        check(!shop.convert("74000000000000000000000000000003", "subset-repeat",
                            payload({{"order_id", Order},
                                     {"mode", "selected_lines"},
                                     {"line_ids", Line2}})).ok,
              "an already-derived selected line is refused");
    }

    section("missing empty cancelled and malformed sources fail before writes");
    {
        Shop missing;
        check(!missing.convert(Root1, "missing",
                               payload({{"order_id", Order}})).ok,
              "nonexistent order refused");
        check(count(missing, j::tables::kJob) == 0 &&
                      count(missing, e::Outbox::table_name()) == 0,
              "missing source writes nothing");

        Shop empty;
        empty.seed_order(Order, {});
        check(!empty.convert(Root1, "empty",
                             payload({{"order_id", Order}})).ok,
              "empty order refused");

        Shop cancelled;
        cancelled.seed_order(Order, {Line1}, true);
        check(!cancelled.convert(Root1, "cancelled",
                                 payload({{"order_id", Order}})).ok,
              "cancelled order refused");

        Shop malformed;
        check(!malformed.convert(Root1, "bad-wire", e::Blob{1, 2, 3}).ok,
              "malformed wire payload refused");
        check(!malformed.convert(Root1, "unknown",
                                 payload({{"order_id", Order}, {"surprise", "x"}})).ok,
              "unknown payload field refused");
        malformed.seed_order(Order, {Line1, Line2});
        check(!malformed.convert(Root1, "bad-mode",
                                 payload({{"order_id", Order}, {"mode", "split_magic"}})).ok,
              "unknown creation mode refused");
        check(!malformed.convert(Root1, "dupe-selection",
                                 payload({{"order_id", Order},
                                          {"mode", "selected_lines"},
                                          {"line_ids", Line1 + "," + Line1}})).ok,
              "duplicate line selection refused");
        check(!malformed.convert(Root1, "foreign-selection",
                                 payload({{"order_id", Order},
                                          {"mode", "selected_lines"},
                                          {"line_ids", Line3}})).ok,
              "line not belonging to order refused");
        check(count(malformed, j::tables::kJob) == 0 &&
                      count(malformed, e::Outbox::table_name()) == 0 &&
                      count(malformed, e::AuditLog::table_name()) == 0,
              "all malformed attempts leave no workflow residue");
    }

    section("a later target collision rolls back earlier derived rows");
    {
        Shop shop;
        shop.seed_order(Order, {Line1, Line2});
        const std::string first_id = w::order_job_id(Root1, Line1);
        const std::string second_id = w::order_job_id(Root1, Line2);
        shop.database->write([&](e::Transaction& transaction) {
            j::Job collision;
            collision.id = second_id;
            collision.title = "pre-existing collision";
            collision.description = "must survive rollback";
            collision.quantity_scaled = 1000;
            collision.unit_price_minor = 1;
            collision.total_price_minor = 1;
            collision.created_at = kNow - 1;
            collision.created_by = Person;
            j::data::save_job(transaction, collision);
        });
        const m::Outcome result = shop.convert(
            Root1, "collision", payload({{"order_id", Order}}));
        check(!result.ok, "second target identity collision refuses conversion");
        shop.database->read([&](const e::Store& store) {
            check(!j::data::find_job(store, first_id),
                  "first inserted job rolled back after later collision");
            const auto existing = j::data::find_job(store, second_id);
            check(existing && existing->title == "pre-existing collision",
                  "pre-existing collision row survives unchanged");
            check(!store.find(e::Outbox::table_name(), "collision"),
                  "failed conversion leaves no outbox row");
            check(!store.find(e::AuditLog::table_name(), "collision"),
                  "failed conversion leaves no audit row");
        });
    }

    section("module and job invariants remain enforced after conversion");
    {
        Shop shop;
        shop.seed_order(Order, {Line1});
        check(shop.convert(Root1, "invariant", payload({{"order_id", Order}})).ok,
              "fixture conversion succeeds");
        const std::string job_id = w::order_job_id(Root1, Line1);
        m::Call update;
        update.operation = p::OperationId::job_update;
        update.record_id = job_id;
        update.idempotency_key = "workflow-job-update";
        update.payload = payload({{"note", "still an ordinary job"}});
        check(shop.registry.run(*shop.database, update, owner(),
                                e::ConnectionState::Offline).ok,
              "workflow-created job remains editable through jobs module");
        shop.registry.set_disabled({p::ModuleId::jobs});
        check(!shop.convert(Root2, "inactive",
                            payload({{"order_id", Order}})).ok,
              "inactive required jobs module makes workflow unavailable");
    }

    return squiflow::testing::report();
}
