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
#include "modules/quotations/data/repository.hpp"
#include "modules/quotations/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace quotations = squiflow::modules::quotations;
namespace protocol = squiflow::protocol;

namespace {
std::atomic<std::int64_t> g_now{1'800'000'000'000};
std::int64_t now() { return g_now.fetch_add(1000) + 1000; }
std::atomic<int> g_key{0};
std::string key() { return "quo-key-" + std::to_string(g_key.fetch_add(1) + 1); }

const std::string kPerson = "71000000000000000000000000000001";
const std::string kQuote = "72000000000000000000000000000001";
const std::string kOther = "72000000000000000000000000000002";
const std::string kRev1 = "73000000000000000000000000000001";
const std::string kRev2 = "73000000000000000000000000000002";
const std::string kRev3 = "73000000000000000000000000000003";
const std::string kLineA = "74000000000000000000000000000001";
const std::string kLineB = "74000000000000000000000000000002";
const std::string kLineC = "74000000000000000000000000000003";
const std::string kProduct = "75000000000000000000000000000001";

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

// Two lines: one catalog product at 2 x 15.00, one off-catalog at 1 x 25.00.
// Totals 55.00, which every test below checks against rather than trusting.
engine::Blob two_lines(const std::string& revision_id) {
    return payload(
        {{"revision_id", revision_id},
         {"line.0.id", kLineA},
         {"line.0.product_id", kProduct},
         {"line.0.description", "Vinyl banner"},
         {"line.1.id", kLineB},
         {"line.1.description", "One-off die cut"}},
        {{"line_count", 2},
         {"line.0.quantity_scaled", 2000},
         {"line.0.unit_price_minor", 1500},
         {"line.1.quantity_scaled", 1000},
         {"line.1.unit_price_minor", 2500}});
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
        registry.add(quotations::make_module(now));
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

    section("migration 17 and exact quotation operation surface");
    {
        Shop shop;
        check(shop.registry.handled(protocol::OperationId::quotation_create),
              "quotation create handled");
        check(shop.registry.handled(protocol::OperationId::quotation_revise),
              "quotation revise handled");
        check(shop.registry.handled(protocol::OperationId::quotation_issue),
              "quotation issue handled");
        check(shop.registry.handled(protocol::OperationId::quotation_accept),
              "quotation accept handled");
        check(shop.registry.handled(protocol::OperationId::quotation_expire),
              "quotation expire handled");
        check(quotations::tables::kFirstMigration == 17, "quotations owns migration 17");
        shop.read([](const engine::Store& store) {
            check(store.has_table(quotations::tables::kQuotation), "quotation table exists");
            check(store.has_table(quotations::tables::kRevision), "revision table exists");
            check(store.has_table(quotations::tables::kLine), "line table exists");
        });
    }

    section("a draft is created offline, priced, and carries no number");
    {
        Shop shop;
        check(shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1),
                       session, engine::ConnectionState::Offline)
                  .ok,
              "a quotation is drafted with no connection");

        shop.read([](const engine::Store& store) {
            const auto quote = quotations::data::find_quotation(store, kQuote);
            check(quote.has_value(), "the quotation is on file");
            check(quote->state == quotations::QuotationState::Draft, "it starts as a draft");
            check(quote->current_revision == 1, "it starts at revision one");
            check(quote->accepted_revision == 0, "nothing is accepted yet");

            const auto revision = quotations::data::latest_revision(store, kQuote);
            check(revision.has_value(), "the first revision exists");
            check(!revision->issued, "a draft revision is not issued");
            check(revision->series.empty() && revision->number == 0,
                  "a draft carries no number");
            check(revision->total_minor == 5500, "the revision totals its lines exactly");

            const auto lines = quotations::data::lines_for_revision(store, kRev1);
            check(lines.size() == 2, "both lines were written");
            check(lines[0].amount_minor == 3000, "a catalog line prices two at fifteen");
            check(lines[0].rate_origin == engine::RateOrigin::CatalogDefault,
                  "a line naming a product is a catalog line");
            check(lines[1].amount_minor == 2500, "an off-catalog line prices one at twenty-five");
            check(lines[1].rate_origin == engine::RateOrigin::OffCatalog,
                  "a line with no product is recorded as off-catalog");
        });
    }

    section("a draft is freely editable in place and does not stack revisions");
    {
        Shop shop;
        shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1), session);
        check(shop.run(protocol::OperationId::quotation_revise, kQuote,
                       payload({{"line.0.id", kLineC},
                                {"line.0.description", "Reworked banner"},
                                {"note", "customer wanted it smaller"}},
                               {{"line_count", 1},
                                {"line.0.quantity_scaled", 1000},
                                {"line.0.unit_price_minor", 900}}),
                       session)
                  .ok,
              "a draft is revised without a new revision record");

        shop.read([](const engine::Store& store) {
            const auto all = quotations::data::revisions_for_quotation(store, kQuote);
            check(all.size() == 1, "editing a draft does not stack a revision");
            check(all[0].total_minor == 900, "the draft was repriced");
            const auto lines = quotations::data::lines_for_revision(store, kRev1);
            check(lines.size() == 1, "the replaced lines are gone, not accumulated");
            check(lines[0].id == kLineC, "the new line replaced the old ones");
            check(!quotations::data::find_line(store, kLineA).has_value(),
                  "an edited-away line is removed");
        });
    }

    section("issuing locks the revision and assigns its number");
    {
        Shop shop;
        shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1), session);
        check(shop.run(protocol::OperationId::quotation_issue, kQuote,
                       payload({{"series", "QUO"}}, {{"number", 42}}), session,
                       engine::ConnectionState::Offline)
                  .ok,
              "a quotation is issued offline from this device's block");

        shop.read([](const engine::Store& store) {
            const auto quote = quotations::data::find_quotation(store, kQuote);
            check(quote->state == quotations::QuotationState::Issued, "the quotation is issued");
            const auto revision = quotations::data::latest_revision(store, kQuote);
            check(revision->issued, "the revision is locked");
            check(revision->series == "QUO" && revision->number == 42,
                  "the revision carries its number");
            check(revision->issued_at > 0 && !revision->issued_by.empty(),
                  "issue evidence is recorded");
        });

        check(!shop.run(protocol::OperationId::quotation_issue, kQuote,
                        payload({{"series", "QUO"}}, {{"number", 43}}), session)
                   .ok,
              "the same revision cannot be issued twice");
    }

    section("revising an issued quotation stacks a revision and leaves the old one alone");
    {
        Shop shop;
        shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1), session);
        shop.run(protocol::OperationId::quotation_issue, kQuote,
                 payload({{"series", "QUO"}}, {{"number", 42}}), session);

        check(!shop.run(protocol::OperationId::quotation_revise, kQuote,
                        payload({{"line.0.id", kLineC}, {"line.0.description", "No record"}},
                                {{"line_count", 1},
                                 {"line.0.quantity_scaled", 1000},
                                 {"line.0.unit_price_minor", 900}}),
                        session)
                   .ok,
              "stacking a revision needs its own record");

        check(shop.run(protocol::OperationId::quotation_revise, kQuote,
                       payload({{"revision_id", kRev2},
                                {"line.0.id", kLineC},
                                {"line.0.description", "Cheaper banner"}},
                               {{"line_count", 1},
                                {"line.0.quantity_scaled", 1000},
                                {"line.0.unit_price_minor", 900}}),
                       session)
                  .ok,
              "an issued quotation is revised onto a new revision");

        shop.read([](const engine::Store& store) {
            const auto all = quotations::data::revisions_for_quotation(store, kQuote);
            check(all.size() == 2, "revisions stack rather than overwrite");
            check(all[0].revision == 1 && all[1].revision == 2, "they are numbered in order");
            check(all[0].issued && all[0].number == 42,
                  "the issued revision is untouched by the revision after it");
            check(all[0].total_minor == 5500, "the paper the customer holds still totals 55.00");
            check(!all[1].issued && all[1].number == 0, "the new revision is a draft again");
            check(all[1].total_minor == 900, "the new revision carries the new price");

            check(quotations::data::lines_for_revision(store, kRev1).size() == 2,
                  "the issued revision keeps its own lines");
            check(quotations::data::lines_for_revision(store, kRev2).size() == 1,
                  "the new revision has its own lines");

            const auto quote = quotations::data::find_quotation(store, kQuote);
            check(quote->current_revision == 2, "the head points at the live revision");
            check(quote->state == quotations::QuotationState::Draft,
                  "a revised quotation is an open draft until issued again");
        });
    }

    section("a number is never used twice and an empty offer is never numbered");
    {
        Shop shop;
        shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1), session);
        shop.run(protocol::OperationId::quotation_issue, kQuote,
                 payload({{"series", "QUO"}}, {{"number", 42}}), session);
        shop.run(protocol::OperationId::quotation_create, kOther, two_lines(kRev3), session);

        check(!shop.run(protocol::OperationId::quotation_issue, kOther,
                        payload({{"series", "QUO"}}, {{"number", 42}}), session)
                   .ok,
              "a burned number cannot be handed to a second document");
        check(shop.run(protocol::OperationId::quotation_issue, kOther,
                       payload({{"series", "JOB"}}, {{"number", 42}}), session)
                  .ok,
              "the same number in another series is a different document");
        check(!shop.run(protocol::OperationId::quotation_issue, kQuote,
                        payload({{"series", "QUO"}}, {{"number", 0}}), session)
                   .ok,
              "zero is not a document number");
    }

    section("acceptance pins exactly one issued revision");
    {
        Shop shop;
        shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1), session);

        check(!shop.run(protocol::OperationId::quotation_accept, kQuote,
                        payload({}, {{"revision", 1}}), session)
                   .ok,
              "a quotation that was never issued cannot be accepted");

        shop.run(protocol::OperationId::quotation_issue, kQuote,
                 payload({{"series", "QUO"}}, {{"number", 42}}), session);
        shop.run(protocol::OperationId::quotation_revise, kQuote,
                 payload({{"revision_id", kRev2},
                          {"line.0.id", kLineC},
                          {"line.0.description", "Cheaper banner"}},
                         {{"line_count", 1},
                          {"line.0.quantity_scaled", 1000},
                          {"line.0.unit_price_minor", 900}}),
                 session);
        shop.run(protocol::OperationId::quotation_issue, kQuote,
                 payload({{"series", "QUO"}}, {{"number", 43}}), session);

        check(!shop.run(protocol::OperationId::quotation_accept, kQuote,
                        payload({}, {{"revision", 9}}), session)
                   .ok,
              "a revision that does not exist cannot be accepted");
        check(!shop.run(protocol::OperationId::quotation_accept, kQuote, payload(), session).ok,
              "acceptance must say which revision");

        check(shop.run(protocol::OperationId::quotation_accept, kQuote,
                       payload({}, {{"revision", 1}}), session)
                  .ok,
              "an earlier issued revision may be the one accepted");

        shop.read([](const engine::Store& store) {
            const auto quote = quotations::data::find_quotation(store, kQuote);
            check(quote->state == quotations::QuotationState::Accepted, "the quotation is accepted");
            check(quote->accepted_revision == 1, "exactly revision one was pinned");
            check(quote->accepted_at > 0 && !quote->accepted_by.empty(),
                  "acceptance evidence is recorded");
            const auto pinned = quotations::data::revision_numbered(store, kQuote, 1);
            check(pinned->total_minor == 5500,
                  "the accepted revision keeps its own prices, not the newer ones");
        });

        check(!shop.run(protocol::OperationId::quotation_accept, kQuote,
                        payload({}, {{"revision", 2}}), session)
                   .ok,
              "a quotation cannot be accepted twice");
        check(!shop.run(protocol::OperationId::quotation_revise, kQuote,
                        payload({{"revision_id", kRev3}, {"line.0.id", kLineB},
                                 {"line.0.description", "Too late"}},
                                {{"line_count", 1},
                                 {"line.0.quantity_scaled", 1000},
                                 {"line.0.unit_price_minor", 100}}),
                        session)
                   .ok,
              "an accepted quotation is frozen");
        check(!shop.run(protocol::OperationId::quotation_expire, kQuote, payload(), session).ok,
              "an accepted quotation cannot be expired");
    }

    section("validity is enforced and an expired offer cannot be accepted");
    {
        Shop shop;
        const std::int64_t moment = g_now.load();
        shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1), session);

        check(!shop.run(protocol::OperationId::quotation_issue, kQuote,
                        payload({{"series", "QUO"}},
                                {{"number", 42}, {"valid_until", moment - 500'000}}),
                        session)
                   .ok,
              "a quotation cannot be issued already out of date");

        check(shop.run(protocol::OperationId::quotation_issue, kQuote,
                       payload({{"series", "QUO"}},
                               {{"number", 42}, {"valid_until", moment + 50'000}}),
                       session)
                  .ok,
              "a quotation is issued with a validity date");

        // The offer sits on the counter until its date has passed.
        g_now.fetch_add(500'000);

        check(!shop.run(protocol::OperationId::quotation_accept, kQuote,
                        payload({}, {{"revision", 1}}), session)
                   .ok,
              "an offer past its validity date cannot be accepted");

        shop.read([](const engine::Store& store) {
            const auto lapsed = quotations::data::revisions_lapsed_by(store, g_now.load());
            check(lapsed.size() == 1, "a lapsed offer surfaces as an attention item");
            check(lapsed[0].number == 42, "and it is the right one");
        });

        check(shop.run(protocol::OperationId::quotation_expire, kQuote,
                       payload({{"reason", "customer went elsewhere"}}), session)
                  .ok,
              "a lapsed quotation is expired deliberately");

        shop.read([](const engine::Store& store) {
            const auto quote = quotations::data::find_quotation(store, kQuote);
            check(quote->state == quotations::QuotationState::Expired, "it is expired");
            check(quote->expired_at > 0 && !quote->expired_by.empty(),
                  "expiry evidence is recorded");
            check(quote->expiry_reason == "customer went elsewhere", "the reason is kept");
        });

        check(!shop.run(protocol::OperationId::quotation_expire, kQuote, payload(), session).ok,
              "a quotation cannot expire twice");
        check(!shop.run(protocol::OperationId::quotation_accept, kQuote,
                        payload({}, {{"revision", 1}}), session)
                   .ok,
              "an expired quotation cannot be accepted");
    }

    section("bad lines are refused whole and never half-written");
    {
        Shop shop;
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1}}, {{"line_count", 0}}), session)
                   .ok,
              "a quotation with no lines is not an offer");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1}}, {{"line_count", 100000}}), session)
                   .ok,
              "an absurd line count is refused before anything is allocated");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1},
                                 {"line.0.id", kLineA}, {"line.0.description", "One"},
                                 {"line.1.id", kLineA}, {"line.1.description", "Same again"}},
                                {{"line_count", 2},
                                 {"line.0.quantity_scaled", 1000},
                                 {"line.0.unit_price_minor", 100},
                                 {"line.1.quantity_scaled", 1000},
                                 {"line.1.unit_price_minor", 100}}),
                        session)
                   .ok,
              "the same line cannot appear twice");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1},
                                 {"line.0.id", kLineA}, {"line.0.description", "Free"}},
                                {{"line_count", 1},
                                 {"line.0.quantity_scaled", 0},
                                 {"line.0.unit_price_minor", 100}}),
                        session)
                   .ok,
              "a line with no quantity is refused");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1}, {"line.0.id", kLineA}},
                                {{"line_count", 1},
                                 {"line.0.quantity_scaled", 1000},
                                 {"line.0.unit_price_minor", 100}}),
                        session)
                   .ok,
              "a line must say what is being offered");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1},
                                 {"line.0.id", kLineA}, {"line.0.description", "Overflow"}},
                                {{"line_count", 1},
                                 {"line.0.quantity_scaled", 9'000'000'000'000'000},
                                 {"line.0.unit_price_minor", 9'000'000'000'000}}),
                        session)
                   .ok,
              "an amount too large to store is refused, not wrapped");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1},
                                 {"line.0.id", kLineA},
                                 {"line.0.product_id", kProduct},
                                 {"line.0.description", "Mislabelled"}},
                                {{"line_count", 1},
                                 {"line.0.quantity_scaled", 1000},
                                 {"line.0.unit_price_minor", 100},
                                 {"line.0.rate_origin", 4}}),
                        session)
                   .ok,
              "an off-catalog line cannot also name a catalog product");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote,
                        payload({{"revision_id", kRev1},
                                 {"line.0.id", kLineA}, {"line.0.description", "No reason"}},
                                {{"line_count", 1},
                                 {"line.0.quantity_scaled", 1000},
                                 {"line.0.unit_price_minor", 100},
                                 {"line.0.rate_origin", 3}}),
                        session)
                   .ok,
              "a manually changed rate must keep its reason");

        shop.read([](const engine::Store& store) {
            check(!quotations::data::find_quotation(store, kQuote).has_value(),
                  "not one refused attempt left a quotation behind");
            check(store.count(quotations::tables::kLine) == 0,
                  "not one refused attempt left a line behind");
        });
    }

    section("missing records, rights, malformed payloads and idempotency fail safely");
    {
        Shop shop;
        check(!shop.run(protocol::OperationId::quotation_revise, kQuote, two_lines(kRev2),
                        session)
                   .ok,
              "a quotation that is not on file cannot be revised");
        // Every quotation operation is synchronizable, so the registry refuses a
        // call with no record to order it against before the module is reached.
        // That is a RegistryError, not a refusal: nobody at the counter can cause
        // it or fix it, so it is loud. The service's own subject() guard is a
        // second line of defence, and this records which layer actually holds.
        bool loud = false;
        try {
            shop.run(protocol::OperationId::quotation_create, "", two_lines(kRev1), session);
        } catch (const modules::RegistryError&) {
            loud = true;
        }
        check(loud, "a call with no record id is a wiring error, not a refusal");

        engine::Session unsigned_session;
        unsigned_session.rights.grant_all();
        check(!shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1),
                        unsigned_session)
                   .ok,
              "rights do not replace authentication");

        const modules::Outcome malformed = shop.run(
            protocol::OperationId::quotation_create, kQuote, engine::Blob{1, 2, 3, 4}, session);
        check(!malformed.ok &&
                  malformed.error == "This request could not be read. Please try it again.",
              "malformed payload is refused safely");

        check(shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev1), session)
                  .ok,
              "a good quotation still goes in after all that");
        check(!shop.run(protocol::OperationId::quotation_create, kQuote, two_lines(kRev2), session)
                   .ok,
              "the same quotation cannot be created twice");

        modules::Call call;
        call.operation = protocol::OperationId::quotation_create;
        call.record_id = kOther;
        call.payload = two_lines(kRev3);
        call.idempotency_key = "same-key";
        const modules::Outcome first = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Offline);
        const modules::Outcome replay = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Offline);
        check(first.ok, "first synchronizable write succeeds");
        check(replay.ok && replay.replayed, "idempotent replay is flagged and harmless");

        shop.read([](const engine::Store& store) {
            check(quotations::data::revisions_for_quotation(store, kOther).size() == 1,
                  "a replay did not stack a second revision");
        });
    }

    return squiflow::testing::report();
}
