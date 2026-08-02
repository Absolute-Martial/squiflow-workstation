#include <atomic>
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/jobs/data/repository.hpp"
#include "modules/jobs/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace jobs = squiflow::modules::jobs;
namespace protocol = squiflow::protocol;

namespace {
std::atomic<std::int64_t> g_now{1'800'000'000'000};
std::int64_t now() { return g_now.fetch_add(1000) + 1000; }
std::atomic<int> g_key{0};
std::string key() { return "jobs-key-" + std::to_string(g_key.fetch_add(1) + 1); }

const std::string kPerson = "61000000000000000000000000000001";
const std::string kJob = "62000000000000000000000000000001";
const std::string kThin = "62000000000000000000000000000002";

engine::Blob payload(
    std::initializer_list<std::pair<std::string, std::string>> texts = {},
    std::initializer_list<std::pair<std::string, std::int64_t>> numbers = {},
    std::initializer_list<std::pair<std::string, bool>> booleans = {}) {
    engine::Row row;
    for (const auto& [name, value] : texts) {
        row.set(name, engine::Value::text(value));
    }
    for (const auto& [name, value] : numbers) {
        row.set(name, engine::Value::integer(value));
    }
    for (const auto& [name, value] : booleans) {
        row.set(name, engine::Value::boolean(value));
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

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(jobs::make_module(now));
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

}  // namespace

int main() {
    const engine::Session session = owner();

    section("migration 16 and exact job operation surface");
    {
        Shop shop;
        check(shop.registry.handled(protocol::OperationId::job_create),
              "job create handled");
        check(shop.registry.handled(protocol::OperationId::job_update),
              "job update handled");
        check(shop.registry.handled(protocol::OperationId::job_state_change),
              "job state change handled");
        check(shop.registry.handled(protocol::OperationId::job_cancel),
              "job cancel handled");
        shop.read([](const engine::Store& store) {
            check(store.has_table(jobs::tables::kJob), "job table exists");
        });
    }

    section("create and update a draft job offline");
    {
        Shop shop;
        const modules::Outcome made = shop.run(
            protocol::OperationId::job_create,
            kJob,
            payload({{"party_id", "party-a"},
                     {"title", "Banner"},
                     {"description", "Printed banner"},
                     {"rate_reason", "Agreement 2026"},
                     {"note", "rush"}},
                    {{"quantity_scaled", 2500},
                     {"unit_price_minor", 10001},
                     {"rate_origin", 2},
                     {"promised_at", 1'800'000'100'000},
                     {"deadline_at", 1'800'000'200'000}}),
            session,
            engine::ConnectionState::Offline);
        check(made.ok && made.queued, "job create works offline and queues");

        const modules::Outcome updated = shop.run(
            protocol::OperationId::job_update,
            kJob,
            payload({{"specifications", "13oz matte"},
                     {"material_reference", "banner roll"}},
                    {{"commercial", 1}, {"payment", 1}}),
            session,
            engine::ConnectionState::Offline);
        check(updated.ok && updated.queued, "job update works offline and queues");

        shop.read([](const engine::Store& store) {
            const auto job = jobs::data::find_job(store, kJob);
            check(job.has_value(), "job persisted");
            check(job && job->state == jobs::JobState::Draft, "job starts as draft");
            check(job && job->total_price_minor == 25003, "job total is derived and rounded");
            check(job && job->commercial == jobs::CommercialProgress::Approved,
                  "commercial axis updated");
            check(job && job->payment == jobs::PaymentProgress::Invoiced,
                  "payment axis updated");
        });
    }

    section("thin jobs stay visibly thin");
    {
        Shop shop;
        const modules::Outcome made = shop.run(
            protocol::OperationId::job_create,
            kThin,
            payload({{"title", "Walk-in print"}},
                    {{"quantity_scaled", 1000}, {"unit_price_minor", 5000}},
                    {{"thin", true}}),
            session,
            engine::ConnectionState::Offline);
        check(made.ok, "thin job can be created");
        check(!shop.run(protocol::OperationId::job_update, kThin,
                        payload({{"specifications", "pretend complete"}}), session).ok,
              "thin job cannot silently pretend to be complete");
    }

    section("state progression requires ticket and delivery evidence");
    {
        Shop shop;
        check(shop.run(
                  protocol::OperationId::job_create,
                  kJob,
                  payload({{"title", "Cards"}, {"description", "Business cards"}},
                          {{"quantity_scaled", 1000}, {"unit_price_minor", 2000}}),
                  session)
                  .ok,
              "fixture draft created");
        check(!shop.run(protocol::OperationId::job_state_change, kJob,
                        payload({}, {{"state", 1}}), session).ok,
              "starting needs ticket evidence");
        check(shop.run(protocol::OperationId::job_state_change, kJob,
                       payload({{"ticket_series", "JOB"}},
                              {{"state", 1}, {"ticket_number", 12}}),
                       session, engine::ConnectionState::Offline)
                  .ok,
              "job starts offline with ticket evidence");
        check(!shop.run(protocol::OperationId::job_state_change, kJob,
                        payload({}, {{"state", 2}}), session).ok,
              "done state needs delivery evidence");
        check(shop.run(protocol::OperationId::job_state_change, kJob,
                       payload({{"delivered_by", "owner"},
                                {"received_by", "customer"},
                                {"delivery_signature_ref", "sig-1"}},
                              {{"state", 2}}),
                       session, engine::ConnectionState::Offline)
                  .ok,
              "done state keeps delivery evidence");
        shop.read([](const engine::Store& store) {
            const auto job = jobs::data::find_job(store, kJob);
            check(job && job->state == jobs::JobState::Done, "job ended done");
            check(job && job->production == jobs::ProductionProgress::Produced,
                  "production axis closed on completion");
            check(job && job->fulfilment == jobs::FulfilmentProgress::Delivered,
                  "fulfilment axis closed on completion");
            check(job && job->ticket_series == "JOB" && job->ticket_number == 12,
                  "ticket identity persists");
        });
    }

    section("cancel requires reason and is online-only");
    {
        Shop shop;
        check(shop.run(
                  protocol::OperationId::job_create,
                  kJob,
                  payload({{"title", "Poster"}, {"description", "Large poster"}},
                          {{"quantity_scaled", 1000}, {"unit_price_minor", 5000}}),
                  session)
                  .ok,
              "cancel fixture created");
        check(!shop.run(protocol::OperationId::job_cancel, kJob,
                        payload({{"reason", "customer changed mind"},
                                 {"ticket_series", "JOB"}},
                                {{"ticket_number", 20}}),
                        session, engine::ConnectionState::Offline)
                   .ok,
              "job cancel is online-only");
        check(!shop.run(protocol::OperationId::job_cancel, kJob,
                        payload({{"ticket_series", "JOB"}}, {{"ticket_number", 20}}),
                        session)
                   .ok,
              "job cancel needs a reason");
        check(shop.run(protocol::OperationId::job_cancel, kJob,
                       payload({{"reason", "customer changed mind"},
                                {"ticket_series", "JOB"}},
                              {{"ticket_number", 20}}),
                       session)
                  .ok,
              "job cancel keeps burned ticket evidence");
        check(!shop.run(protocol::OperationId::job_update, kJob,
                        payload({{"note", "rewrite cancelled"}}), session).ok,
              "cancelled job is frozen");
    }

    section("rights malformed payloads and idempotency fail safely");
    {
        Shop shop;
        engine::Session unsigned_session;
        unsigned_session.rights.grant_all();
        check(!shop.run(protocol::OperationId::job_create, kJob,
                        payload({{"title", "Unsigned"}, {"description", "Bad"}},
                                {{"quantity_scaled", 1000}, {"unit_price_minor", 1000}}),
                        unsigned_session)
                   .ok,
              "rights do not replace authentication");
        const modules::Outcome malformed = shop.run(
            protocol::OperationId::job_create, kJob, engine::Blob{1, 2, 3, 4}, session);
        check(!malformed.ok &&
                  malformed.error == "This request could not be read. Please try it again.",
              "malformed payload is refused safely");

        modules::Call call;
        call.operation = protocol::OperationId::job_create;
        call.record_id = kJob;
        call.payload = payload({{"title", "Repeat"}, {"description", "Repeat"}},
                               {{"quantity_scaled", 1000}, {"unit_price_minor", 3000}});
        call.idempotency_key = "same-key";
        const modules::Outcome first = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Offline);
        const modules::Outcome replay = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Offline);
        check(first.ok, "first synchronizable write succeeds");
        check(replay.ok && replay.replayed, "idempotent replay is flagged and harmless");
    }

    return squiflow::testing::report();
}
