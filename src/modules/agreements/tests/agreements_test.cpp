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
#include "modules/agreements/data/repository.hpp"
#include "modules/agreements/module.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;
namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace agreements = squiflow::modules::agreements;
namespace protocol = squiflow::protocol;

namespace {
std::atomic<std::int64_t> g_now{1'800'000'000'000};
std::int64_t now() { return g_now.fetch_add(1000) + 1000; }
std::atomic<int> g_key{0};
std::string key() { return "agr-key-" + std::to_string(g_key.fetch_add(1) + 1); }

const std::string kPerson = "71000000000000000000000000000001";
const std::string kAgreeA = "72000000000000000000000000000001";
const std::string kAgreeB = "72000000000000000000000000000002";
const std::string kAgreeC = "72000000000000000000000000000003";
const std::string kLineA = "74000000000000000000000000000001";
const std::string kLineB = "74000000000000000000000000000002";
const std::string kLineC = "74000000000000000000000000000003";
const std::string kProduct = "75000000000000000000000000000001";
const std::string kParty = "76000000000000000000000000000001";
const std::string kOtherParty = "76000000000000000000000000000002";
const std::string kQuote = "77000000000000000000000000000001";

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

// One agreed rate: 12.00 a card, capped at 5,000 cards. Every cap test below
// works against this shape rather than inventing its own.
engine::Blob one_rate(std::int64_t rate_minor = 1200, std::int64_t cap_scaled = 5'000'000) {
    return payload({{"party_id", kParty},
                    {"line.0.id", kLineA},
                    {"line.0.product_id", kProduct},
                    {"line.0.agreed_name", "Business card, 350gsm"}},
                   {{"line_count", 1},
                    {"line.0.rate_minor", rate_minor},
                    {"line.0.cap_scaled", cap_scaled}});
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

agreements::AgreementLine sample_line(std::int64_t cap, std::int64_t consumed) {
    agreements::AgreementLine line;
    line.id = kLineA;
    line.agreement_id = kAgreeA;
    line.product_id = kProduct;
    line.agreed_name = "Business card, 350gsm";
    line.rate_minor = 1200;
    line.cap_scaled = cap;
    line.consumed_scaled = consumed;
    return line;
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(agreements::make_module(now));
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

// The two calls every later section starts from: strike the bargain, then
// bring it into force. Kept here so a section testing closing is not also
// silently testing opening.
void strike_and_open(Shop& shop, const engine::Session& session, const std::string& id,
                     const engine::Blob& body) {
    shop.run(protocol::OperationId::agreement_create, id, body, session);
    shop.run(protocol::OperationId::agreement_update, id,
             payload({{"action", "open"}}), session);
}

}  // namespace

int main() {
    const engine::Session session = owner();

    section("migration 18 and the exact agreement operation surface");
    {
        Shop shop;
        check(shop.registry.handled(protocol::OperationId::agreement_create),
              "agreement create handled");
        check(shop.registry.handled(protocol::OperationId::agreement_update),
              "agreement update handled");
        check(shop.registry.handled(protocol::OperationId::agreement_close),
              "agreement close handled");
        check(shop.registry.handled(protocol::OperationId::agreement_reopen),
              "agreement reopen handled");
        check(!shop.registry.handled(protocol::OperationId::quotation_create),
              "this module claims nothing that belongs to another");
        check(agreements::tables::kFirstMigration == 18, "agreements owns migration 18");

        // The wire contract, asserted rather than assumed. Closing changes what
        // every later invoice may charge, so it must require a live connection
        // and not merely queue itself for later.
        const auto& create = protocol::operation(protocol::OperationId::agreement_create);
        const auto& update = protocol::operation(protocol::OperationId::agreement_update);
        const auto& closing = protocol::operation(protocol::OperationId::agreement_close);
        const auto& reopening = protocol::operation(protocol::OperationId::agreement_reopen);
        check(create.sync_class == protocol::OperationClass::Synchronizable,
              "creating an agreement is synchronizable");
        check(update.sync_class == protocol::OperationClass::Synchronizable,
              "updating an agreement is synchronizable");
        check(closing.sync_class == protocol::OperationClass::OnlineRequired,
              "closing an agreement must reach the server as it happens");
        check(reopening.sync_class == protocol::OperationClass::OnlineRequired,
              "reopening an agreement must reach the server as it happens");
        check(create.offline == protocol::OfflineRule::OnlineOnly &&
                  update.offline == protocol::OfflineRule::OnlineOnly &&
                  closing.offline == protocol::OfflineRule::OnlineOnly &&
                  reopening.offline == protocol::OfflineRule::OnlineOnly,
              "no agreement operation may be done with the connection down");
        check(create.right == protocol::RightId::right_agreement_write &&
                  update.right == protocol::RightId::right_agreement_write,
              "writing an agreement needs the write right");
        check(closing.right == protocol::RightId::right_agreement_close &&
                  reopening.right == protocol::RightId::right_agreement_close,
              "closing and reopening need a right of their own");
        check(create.module == protocol::ModuleId::agreements, "create belongs to agreements");

        shop.read([](const engine::Store& store) {
            check(store.has_table(agreements::tables::kAgreement), "agreement table exists");
            check(store.has_table(agreements::tables::kLine), "agreement line table exists");
            check(store.count(agreements::tables::kAgreement) == 0, "a new shop has no agreements");
        });
    }

    section("a bargain is struck as a draft and prices nothing until it is opened");
    {
        Shop shop;
        const modules::Outcome created = shop.run(
            protocol::OperationId::agreement_create, kAgreeA,
            payload({{"party_id", kParty},
                     {"source_quotation_id", kQuote},
                     {"customer_reference", "PO-8891"},
                     {"terms", "Net 30, collection from the shop."},
                     {"line.0.id", kLineA},
                     {"line.0.product_id", kProduct},
                     {"line.0.agreed_name", "Business card, 350gsm"}},
                    {{"line_count", 1},
                     {"line.0.rate_minor", 1200},
                     {"line.0.cap_scaled", 5'000'000}}),
            session);
        check(created.ok, "an agreement is struck");
        check(created.queued, "and is queued to reach the server");

        shop.read([](const engine::Store& store) {
            const auto agreement = agreements::data::find_agreement(store, kAgreeA);
            check(agreement.has_value(), "the agreement is on file");
            check(agreement->state == agreements::AgreementState::Draft,
                  "a new agreement is a draft, not yet in force");
            check(agreement->party_id == kParty, "it names the customer it is with");
            check(agreement->source_quotation_id == kQuote, "it remembers the offer it came from");
            check(agreement->terms == "Net 30, collection from the shop.",
                  "the terms are kept with the agreement, not looked up later");
            check(agreement->created_at > 0 && !agreement->created_by.empty(),
                  "it records when and by whom it was struck");
            check(agreement->opened_at == 0, "a draft has no moment of coming into force");
            check(!agreements::has_expiry(*agreement),
                  "an agreement with no end date given is open-ended");

            const auto lines = agreements::data::lines_for_agreement(store, kAgreeA);
            check(lines.size() == 1, "the agreed rate is on file");
            check(lines.front().rate_minor == 1200, "at the rate that was agreed");
            check(lines.front().cap_scaled == 5'000'000, "with the cap that was agreed");
            check(lines.front().consumed_scaled == 0, "and nothing consumed against it yet");
            check(lines.front().agreed_name == "Business card, 350gsm",
                  "under the name the customer agreed to");
        });

        const modules::Outcome offline = shop.run(
            protocol::OperationId::agreement_create, kAgreeB, one_rate(), session,
            engine::ConnectionState::Offline);
        check(!offline.ok, "an agreement cannot be struck with the connection down");
        check(offline.reason == engine::DenialReason::RequiresConnection,
              "and the refusal says why");
        check(!shop.run(protocol::OperationId::agreement_create, kAgreeA, one_rate(), session).ok,
              "the same agreement cannot be struck twice");
    }

    section("the same product under two names is deliberate, the same name twice is not");
    {
        Shop shop;
        check(shop.run(protocol::OperationId::agreement_create, kAgreeA,
                       payload({{"party_id", kParty},
                                {"line.0.id", kLineA},
                                {"line.0.product_id", kProduct},
                                {"line.0.agreed_name", "Business card, 350gsm"},
                                {"line.1.id", kLineB},
                                {"line.1.product_id", kProduct},
                                {"line.1.agreed_name", "Loyalty card"}},
                               {{"line_count", 2},
                                {"line.0.rate_minor", 1200},
                                {"line.1.rate_minor", 800}}),
                       session)
                  .ok,
              "one product may be listed twice under two names at two rates");

        shop.read([](const engine::Store& store) {
            const auto lines = agreements::data::lines_for_agreement(store, kAgreeA);
            check(lines.size() == 2, "both names are kept");
            check(lines[0].rate_minor != lines[1].rate_minor,
                  "each name keeps its own rate; nothing merged them");
            check(agreements::data::lines_for_product(store, kAgreeA, kProduct).size() == 2,
                  "asking by product returns both, and does not choose one");
            check(lines[0].cap_scaled == 0 && lines[1].cap_scaled == 0,
                  "a rate with no cap given is uncapped");
        });

        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB,
                        payload({{"party_id", kParty},
                                 {"line.0.id", kLineA},
                                 {"line.0.product_id", kProduct},
                                 {"line.0.agreed_name", "Business card, 350gsm"},
                                 {"line.1.id", kLineB},
                                 {"line.1.product_id", kProduct},
                                 {"line.1.agreed_name", "Business card, 350gsm"}},
                                {{"line_count", 2},
                                 {"line.0.rate_minor", 1200},
                                 {"line.1.rate_minor", 800}}),
                        session)
                   .ok,
              "but one name twice for one product has no readable meaning");
        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB,
                        payload({{"party_id", kParty},
                                 {"line.0.id", kLineA},
                                 {"line.0.product_id", kProduct},
                                 {"line.0.agreed_name", "One"},
                                 {"line.1.id", kLineA},
                                 {"line.1.product_id", kProduct},
                                 {"line.1.agreed_name", "Two"}},
                                {{"line_count", 2},
                                 {"line.0.rate_minor", 100},
                                 {"line.1.rate_minor", 200}}),
                        session)
                   .ok,
              "the same line record cannot appear twice");
        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB,
                        payload({{"party_id", kParty},
                                 {"line.0.id", kLineC},
                                 {"line.0.agreed_name", "Something off the books"}},
                                {{"line_count", 1}, {"line.0.rate_minor", 100}}),
                        session)
                   .ok,
              "an agreement cannot price something that is not in the catalog");
        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB,
                        payload({{"party_id", kParty},
                                 {"line.0.id", kLineC},
                                 {"line.0.product_id", kProduct}},
                                {{"line_count", 1}, {"line.0.rate_minor", 100}}),
                        session)
                   .ok,
              "an agreed rate must carry the name the customer agreed to");

        shop.read([](const engine::Store& store) {
            check(!agreements::data::find_agreement(store, kAgreeB).has_value(),
                  "not one refused attempt left an agreement behind");
            check(store.count(agreements::tables::kLine) == 2,
                  "and not one left a stray line behind");
        });
    }

    section("the arithmetic of a cap holds at every edge");
    {
        const agreements::CapState uncapped = agreements::cap_state(
            sample_line(0, 9'000'000'000'000'000));
        check(!uncapped.capped, "a rate with no cap is uncapped");
        check(!uncapped.nearing, "an uncapped rate never nears a limit it does not have");
        check(!uncapped.exceeded, "and can never exceed one");
        check(uncapped.remaining_scaled == 0, "remaining is meaningless without a cap, not negative");

        check(!agreements::cap_state(sample_line(1000, 899)).nearing,
              "just under nine tenths is not yet worth an attention item");
        check(agreements::cap_state(sample_line(1000, 900)).nearing,
              "nine tenths exactly is where the warning starts");

        const agreements::CapState nearly = agreements::cap_state(sample_line(1000, 999));
        check(nearly.nearing && !nearly.exceeded, "one short of the cap warns but does not refuse");
        check(nearly.remaining_scaled == 1, "and says exactly how much is left");

        const agreements::CapState exactly = agreements::cap_state(sample_line(1000, 1000));
        check(!exactly.exceeded, "landing exactly on the cap has not passed it");
        check(exactly.remaining_scaled == 0, "there is nothing left");
        check(exactly.nearing, "which is certainly worth saying out loud");

        const agreements::CapState over = agreements::cap_state(sample_line(1000, 1001));
        check(over.exceeded, "one past the cap has passed it");
        check(over.remaining_scaled == 0, "remaining stops at nothing and does not go negative");
        check(!over.nearing, "nearing is not still claimed once it is behind us");
        check(over.agreed_scaled == 1000 && over.consumed_scaled == 1001,
              "agreed and consumed are both reported, so a screen need not recompute them");

        const agreements::ConsumeResult to_cap =
            agreements::consume_quantity(sample_line(1000, 0), 1000);
        check(to_cap.ok && to_cap.consumed_scaled == 1000, "consuming exactly the cap is allowed");
        check(!to_cap.exceeds_cap, "and does not report an overrun");
        check(agreements::consume_quantity(sample_line(1000, 0), 1001).exceeds_cap,
              "consuming one more than the cap says so");
        check(agreements::consume_quantity(sample_line(1000, 0), 1001).ok,
              "without refusing outright, because an override with a reason is allowed");
        check(!agreements::consume_quantity(sample_line(1000, 0), 0).ok,
              "consuming nothing is not a thing that happens");
        check(!agreements::consume_quantity(sample_line(1000, 0), -5).ok,
              "and consuming a negative quantity is a bug, not a credit");
        check(!agreements::consume_quantity(
                   sample_line(1000, std::numeric_limits<std::int64_t>::max()), 1)
                   .ok,
              "consumption that would overflow is refused, not wrapped into a negative");
        const agreements::ConsumeResult unbounded =
            agreements::consume_quantity(sample_line(0, 0), 9'000'000'000'000);
        check(unbounded.ok && !unbounded.exceeds_cap,
              "an uncapped rate never reports an overrun however much is taken");

        const agreements::ConsumeResult released =
            agreements::release_quantity(sample_line(1000, 1000), 400);
        check(released.ok && released.consumed_scaled == 600,
              "cancelled work gives its quantity back");
        check(!agreements::release_quantity(sample_line(1000, 1000), 1001).ok,
              "more cannot be given back than was ever taken");
        check(!agreements::release_quantity(sample_line(1000, 1000), 0).ok,
              "releasing nothing is not a thing that happens");
        check(!agreements::release_quantity(sample_line(1000, 1000), -1).ok,
              "and releasing a negative quantity is a bug, not a consumption");
        check(agreements::release_quantity(sample_line(1000, 1000), 1000).consumed_scaled == 0,
              "releasing everything lands exactly on nothing");
        const agreements::ConsumeResult back_under =
            agreements::release_quantity(sample_line(1000, 1200), 300);
        check(back_under.ok && back_under.consumed_scaled == 900,
              "an overrun can be brought back under the cap");
        check(!back_under.exceeds_cap, "and stops reporting an overrun when it is");

        const engine::MoneyResult worth = agreements::capped_value(sample_line(5'000'000, 0));
        check(worth.ok && worth.value.minor == 6'000'000,
              "5,000 cards at 12.00 is what the cap is worth at most");
        const engine::MoneyResult nothing = agreements::capped_value(sample_line(0, 0));
        check(nothing.ok && nothing.value.minor == 0,
              "an uncapped rate has no maximum value, reported as zero rather than guessed");
        agreements::AgreementLine absurd = sample_line(9'000'000'000'000'000, 0);
        absurd.rate_minor = 9'000'000'000'000;
        check(!agreements::capped_value(absurd).ok,
              "a cap worth more than money can hold is refused, not wrapped");
    }

    section("a period is a promise, and an open-ended one never nags");
    {
        agreements::Agreement forever;
        forever.valid_from = 1000;
        forever.valid_until = 0;
        check(!agreements::has_expiry(forever), "no end date means no end");
        check(!agreements::lapsed_at_moment(forever, 9'000'000'000'000),
              "an open-ended agreement has not lapsed, however late it gets");
        check(!agreements::expiring_at_moment(forever, 9'000'000'000'000, 9'000'000'000'000),
              "and never appears on a list of things about to end");

        agreements::Agreement bounded;
        bounded.valid_from = 1000;
        bounded.valid_until = 10'000;
        check(agreements::has_expiry(bounded), "an end date is an end date");
        check(!agreements::lapsed_at_moment(bounded, 9'999), "not lapsed the moment before");
        check(!agreements::lapsed_at_moment(bounded, 10'000),
              "not lapsed on the last day; the date is inclusive");
        check(agreements::lapsed_at_moment(bounded, 10'001), "lapsed the moment after");
        check(agreements::expiring_at_moment(bounded, 9'000, 1000),
              "inside the warning window it asks for attention");
        check(agreements::expiring_at_moment(bounded, 9'000, 1'000),
              "exactly on the edge of the window counts as inside it");
        check(!agreements::expiring_at_moment(bounded, 8'999, 1000),
              "outside the window it stays quiet");
        check(!agreements::expiring_at_moment(bounded, 10'001, 1000),
              "and once lapsed it is no longer expiring; it has expired");
        check(!agreements::expiring_at_moment(bounded, 9'000, -1),
              "a negative warning window warns about nothing");

        check(agreements::transition_allowed(agreements::AgreementState::Draft,
                                             agreements::AgreementState::Open),
              "a draft may come into force");
        check(!agreements::transition_allowed(agreements::AgreementState::Draft,
                                              agreements::AgreementState::Superseded),
              "a draft cannot be superseded; nothing was ever in force");
        check(!agreements::transition_allowed(agreements::AgreementState::Draft,
                                              agreements::AgreementState::Closed),
              "and a draft cannot be closed");
        check(agreements::transition_allowed(agreements::AgreementState::Open,
                                             agreements::AgreementState::Closed),
              "what is in force may be closed");
        check(agreements::transition_allowed(agreements::AgreementState::Open,
                                             agreements::AgreementState::Superseded),
              "or replaced");
        check(agreements::transition_allowed(agreements::AgreementState::Closed,
                                             agreements::AgreementState::Open),
              "a closed agreement may be reopened");
        check(!agreements::transition_allowed(agreements::AgreementState::Superseded,
                                              agreements::AgreementState::Open),
              "but a superseded one is final");
        check(!agreements::transition_allowed(agreements::AgreementState::Superseded,
                                              agreements::AgreementState::Closed),
              "in every direction");
        check(agreements::can_amend(agreements::AgreementState::Draft) &&
                  agreements::can_amend(agreements::AgreementState::Open),
              "a draft and an agreement in force can both be amended");
        check(!agreements::can_amend(agreements::AgreementState::Closed) &&
                  !agreements::can_amend(agreements::AgreementState::Superseded),
              "a closed or superseded one cannot");
    }

    section("coming into force happens once, deliberately, and never after the end date");
    {
        Shop shop;
        check(shop.run(protocol::OperationId::agreement_create, kAgreeA, one_rate(), session).ok,
              "a draft is struck");
        check(shop.run(protocol::OperationId::agreement_update, kAgreeA,
                       payload({{"action", "open"}}), session)
                  .ok,
              "and brought into force");
        shop.read([](const engine::Store& store) {
            const auto agreement = agreements::data::find_agreement(store, kAgreeA);
            check(agreement->state == agreements::AgreementState::Open, "it is now in force");
            check(agreement->opened_at > 0 && !agreement->opened_by.empty(),
                  "and records the moment and the person");
            check(agreements::data::open_agreements_for_party(store, kParty).size() == 1,
                  "it appears on the list of what is in force for this customer");
            check(agreements::data::open_agreements_for_party(store, kOtherParty).empty(),
                  "and on nobody else's");
        });
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeA,
                        payload({{"action", "open"}}), session)
                   .ok,
              "it cannot be brought into force a second time");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeA,
                        payload({{"party_id", kOtherParty}}), session)
                   .ok,
              "and it cannot be moved to a different customer");

        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB,
                        payload({{"party_id", kParty},
                                 {"line.0.id", kLineB},
                                 {"line.0.product_id", kProduct},
                                 {"line.0.agreed_name", "Backwards"}},
                                {{"line_count", 1},
                                 {"line.0.rate_minor", 100},
                                 {"valid_from", 9000},
                                 {"valid_until", 8000}}),
                        session)
                   .ok,
              "an agreement cannot end before it begins");
        check(shop.run(protocol::OperationId::agreement_create, kAgreeB,
                       payload({{"party_id", kParty},
                                {"line.0.id", kLineB},
                                {"line.0.product_id", kProduct},
                                {"line.0.agreed_name", "One day only"}},
                               {{"line_count", 1},
                                {"line.0.rate_minor", 100},
                                {"valid_from", 8000},
                                {"valid_until", 8000}}),
                       session)
                  .ok,
              "but one that begins and ends at the same moment is a real agreement");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeB,
                        payload({{"action", "open"}}), session)
                   .ok,
              "and it cannot be brought into force once that moment has passed");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeB,
                        payload({{"action", "dance"}}), session)
                   .ok,
              "an action nobody wrote is refused rather than ignored");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeC,
                        payload({{"action", "open"}}), session)
                   .ok,
              "an agreement that is not on file cannot be amended");
    }

    section("closing states a reason and an answer for the work still running");
    {
        Shop shop;
        shop.run(protocol::OperationId::agreement_create, kAgreeA, one_rate(), session);
        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"reason", "Never mind"}, {"close_effect", "catalog"}}), session)
                   .ok,
              "a draft cannot be closed; it was never in force");
        shop.run(protocol::OperationId::agreement_update, kAgreeA, payload({{"action", "open"}}),
                 session);

        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"close_effect", "catalog"}}), session)
                   .ok,
              "closing without a reason is refused");
        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"reason", "Customer moved away"}}), session)
                   .ok,
              "and closing without saying what happens to running work is refused");
        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"reason", "Customer moved away"}, {"close_effect", "maybe"}}),
                        session)
                   .ok,
              "an answer nobody defined is not an answer");
        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"reason", "   "}, {"close_effect", "catalog"}}), session)
                   .ok,
              "and neither is a reason made of spaces");

        const modules::Outcome offline = shop.run(
            protocol::OperationId::agreement_close, kAgreeA,
            payload({{"reason", "Customer moved away"}, {"close_effect", "keep"}}), session,
            engine::ConnectionState::Offline);
        check(!offline.ok, "an agreement cannot be closed with the connection down");
        check(offline.reason == engine::DenialReason::RequiresConnection,
              "because a closed agreement changes what every later invoice may charge");
        check(!offline.queued, "and it is not quietly queued to happen later");

        shop.read([](const engine::Store& store) {
            check(agreements::data::find_agreement(store, kAgreeA)->state ==
                      agreements::AgreementState::Open,
                  "not one refused close changed the agreement");
        });

        check(shop.run(protocol::OperationId::agreement_close, kAgreeA,
                       payload({{"reason", "Customer moved away"}, {"close_effect", "keep"}}),
                       session)
                  .ok,
              "closed, with a reason and an answer");
        shop.read([](const engine::Store& store) {
            const auto agreement = agreements::data::find_agreement(store, kAgreeA);
            check(agreement->state == agreements::AgreementState::Closed, "it is closed");
            check(agreement->close_effect == agreements::CloseEffect::KeepAgreedRate,
                  "work already running keeps the rate it was promised");
            check(agreement->close_reason == "Customer moved away", "the reason is kept");
            check(agreement->closed_at > 0 && !agreement->closed_by.empty(),
                  "with the moment and the person");
            check(agreements::data::open_agreements_for_party(store, kParty).empty(),
                  "and it no longer prices anything new");
        });
        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"reason", "Again"}, {"close_effect", "catalog"}}), session)
                   .ok,
              "it cannot be closed twice");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeA,
                        payload({{"note", "A quiet edit"}}), session)
                   .ok,
              "and a closed agreement cannot be amended behind the customer's back");
    }

    section("reopening is allowed and honest; superseding is final");
    {
        Shop shop;
        strike_and_open(shop, session, kAgreeA, one_rate());
        check(!shop.run(protocol::OperationId::agreement_reopen, kAgreeA,
                        payload({{"reason", "Why not"}}), session)
                   .ok,
              "an agreement in force cannot be reopened; it never closed");
        shop.run(protocol::OperationId::agreement_close, kAgreeA,
                 payload({{"reason", "Closed in error"}, {"close_effect", "catalog"}}), session);
        check(!shop.run(protocol::OperationId::agreement_reopen, kAgreeA, payload({}), session).ok,
              "reopening without a reason is refused");
        check(shop.run(protocol::OperationId::agreement_reopen, kAgreeA,
                       payload({{"reason", "Closed in error"}}), session)
                  .ok,
              "reopening with one is allowed");
        shop.read([](const engine::Store& store) {
            const auto agreement = agreements::data::find_agreement(store, kAgreeA);
            check(agreement->state == agreements::AgreementState::Open, "it is in force again");
            check(agreement->reopen_reason == "Closed in error", "and says why");
            check(agreement->reopened_at > 0 && !agreement->reopened_by.empty(),
                  "with the moment and the person");
            check(agreement->closed_at > 0,
                  "the closing is not erased; the history stays readable");
        });

        // A closed agreement whose date has since passed must not come back to
        // life at a rate that is no longer promised to anyone.
        const std::int64_t moment = g_now.load();
        shop.run(protocol::OperationId::agreement_create, kAgreeB,
                 payload({{"party_id", kParty},
                          {"line.0.id", kLineB},
                          {"line.0.product_id", kProduct},
                          {"line.0.agreed_name", "Short season"}},
                         {{"line_count", 1},
                          {"line.0.rate_minor", 500},
                          {"valid_until", moment + 100'000}}),
                 session);
        shop.run(protocol::OperationId::agreement_update, kAgreeB, payload({{"action", "open"}}),
                 session);
        shop.run(protocol::OperationId::agreement_close, kAgreeB,
                 payload({{"reason", "Season over"}, {"close_effect", "catalog"}}), session);
        g_now.fetch_add(500'000);
        check(!shop.run(protocol::OperationId::agreement_reopen, kAgreeB,
                        payload({{"reason", "One more run"}}), session)
                   .ok,
              "a lapsed agreement cannot be reopened as if its date still stood");
        check(shop.run(protocol::OperationId::agreement_reopen, kAgreeB,
                       payload({{"reason", "One more run"}},
                               {{"valid_until", g_now.load() + 1'000'000}}),
                       session)
                  .ok,
              "reopening it needs a new end date stated in the same breath");
    }

    section("the chain of agreements is readable end to end and never loops");
    {
        Shop shop;
        strike_and_open(shop, session, kAgreeA, one_rate());
        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB,
                        payload({{"party_id", kParty},
                                 {"supersedes", kAgreeC},
                                 {"line.0.id", kLineB},
                                 {"line.0.product_id", kProduct},
                                 {"line.0.agreed_name", "Renewed"}},
                                {{"line_count", 1}, {"line.0.rate_minor", 1100}}),
                        session)
                   .ok,
              "an agreement cannot replace one that is not on file");
        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB,
                        payload({{"party_id", kParty},
                                 {"supersedes", kAgreeB},
                                 {"line.0.id", kLineB},
                                 {"line.0.product_id", kProduct},
                                 {"line.0.agreed_name", "Itself"}},
                                {{"line_count", 1}, {"line.0.rate_minor", 1100}}),
                        session)
                   .ok,
              "nor replace itself");

        check(shop.run(protocol::OperationId::agreement_create, kAgreeB,
                       payload({{"party_id", kParty},
                                {"supersedes", kAgreeA},
                                {"renewed_from", kAgreeA},
                                {"line.0.id", kLineB},
                                {"line.0.product_id", kProduct},
                                {"line.0.agreed_name", "Business card, 350gsm"}},
                               {{"line_count", 1}, {"line.0.rate_minor", 1100}}),
                       session)
                  .ok,
              "a successor is drafted alongside the agreement it will replace");
        shop.read([](const engine::Store& store) {
            check(agreements::data::find_agreement(store, kAgreeA)->state ==
                      agreements::AgreementState::Open,
                  "which does not stop the old one while the new one is still a draft");
        });

        check(shop.run(protocol::OperationId::agreement_update, kAgreeB,
                       payload({{"action", "open"}}), session)
                  .ok,
              "bringing the successor into force");
        shop.read([](const engine::Store& store) {
            const auto old_one = agreements::data::find_agreement(store, kAgreeA);
            const auto new_one = agreements::data::find_agreement(store, kAgreeB);
            check(old_one->state == agreements::AgreementState::Superseded,
                  "stops the old one in the same breath");
            check(old_one->superseded_by == kAgreeB, "and says what replaced it");
            check(new_one->state == agreements::AgreementState::Open,
                  "so there is never a moment with neither in force");
            check(new_one->supersedes == kAgreeA, "the link reads in both directions");
            check(new_one->renewed_from == kAgreeA, "and a renewal remembers what it renewed");

            const auto chain = agreements::data::agreement_chain(store, kAgreeA);
            check(chain.size() == 2, "the chain reads end to end");
            check(chain.front().id == kAgreeA && chain.back().id == kAgreeB,
                  "in the order it happened");
        });

        check(!shop.run(protocol::OperationId::agreement_update, kAgreeA,
                        payload({{"note", "Sneak"}}), session)
                   .ok,
              "a superseded agreement cannot be amended");
        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"reason", "Tidy up"}, {"close_effect", "catalog"}}), session)
                   .ok,
              "nor closed");
        check(!shop.run(protocol::OperationId::agreement_reopen, kAgreeA,
                        payload({{"reason", "Bring it back"}}), session)
                   .ok,
              "nor reopened");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeB,
                        payload({{"action", "supersede"}, {"superseded_by", kAgreeB}}), session)
                   .ok,
              "an agreement cannot supersede itself");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeB,
                        payload({{"action", "supersede"}}), session)
                   .ok,
              "superseding must name the replacement");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeB,
                        payload({{"action", "supersede"}, {"superseded_by", kAgreeC}}), session)
                   .ok,
              "and the replacement must be on file");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeB,
                        payload({{"action", "supersede"}, {"superseded_by", kAgreeA}}), session)
                   .ok,
              "a chain that loops back on itself is refused, however it is written");

        shop.read([](const engine::Store& store) {
            check(agreements::data::find_agreement(store, kAgreeB)->state ==
                      agreements::AgreementState::Open,
                  "and the refused loop left the successor exactly as it was");
            check(agreements::data::agreement_chain(store, kAgreeA).size() == 2,
                  "the chain is still two long and still terminates");
        });
    }

    section("amending what is in force needs a reason and never resets what was consumed");
    {
        Shop shop;
        shop.run(protocol::OperationId::agreement_create, kAgreeA,
                 payload({{"party_id", kParty},
                          {"line.0.id", kLineA},
                          {"line.0.product_id", kProduct},
                          {"line.0.agreed_name", "Business card, 350gsm"},
                          {"line.1.id", kLineB},
                          {"line.1.product_id", kProduct},
                          {"line.1.agreed_name", "Loyalty card"}},
                         {{"line_count", 2},
                          {"line.0.rate_minor", 1200},
                          {"line.0.cap_scaled", 5'000'000},
                          {"line.1.rate_minor", 800}}),
                 session);
        check(shop.run(protocol::OperationId::agreement_update, kAgreeA,
                       payload({{"note", "Still being negotiated"}}), session)
                  .ok,
              "a draft is freely amended");
        shop.run(protocol::OperationId::agreement_update, kAgreeA, payload({{"action", "open"}}),
                 session);

        // 1,000 cards have been run against the cap.
        shop.database->write([&](engine::Transaction& transaction) {
            auto line = agreements::data::find_line(transaction, kLineA);
            check(line.has_value(), "the agreed rate is on file before work runs against it");
            line->consumed_scaled = 1'000'000;
            agreements::data::save_line(transaction, *line);
        });

        check(shop.run(protocol::OperationId::agreement_update, kAgreeA,
                       payload({{"note", "A harmless note"}}), session)
                  .ok,
              "a note on an agreement in force needs no ceremony");
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeA,
                        payload({{"party_id", kParty},
                                 {"line.0.id", kLineA},
                                 {"line.0.product_id", kProduct},
                                 {"line.0.agreed_name", "Business card, 350gsm"}},
                                {{"line_count", 1},
                                 {"line.0.rate_minor", 1400},
                                 {"line.0.cap_scaled", 5'000'000}}),
                        session)
                   .ok,
              "but changing the rates of an agreement in force does");
        shop.read([](const engine::Store& store) {
            const auto lines = agreements::data::lines_for_agreement(store, kAgreeA);
            check(lines.size() == 2, "the refused amendment left both rates alone");
            check(lines.front().rate_minor == 1200, "at the rate that was actually agreed");
        });

        check(shop.run(protocol::OperationId::agreement_update, kAgreeA,
                       payload({{"reason", "Paper cost rose; customer agreed by phone"},
                                {"line.0.id", kLineA},
                                {"line.0.product_id", kProduct},
                                {"line.0.agreed_name", "Business card, 350gsm"}},
                               {{"line_count", 1},
                                {"line.0.rate_minor", 1400},
                                {"line.0.cap_scaled", 5'000'000}}),
                       session)
                  .ok,
              "with a reason, the rate changes");
        shop.read([](const engine::Store& store) {
            const auto lines = agreements::data::lines_for_agreement(store, kAgreeA);
            check(lines.size() == 1, "a rate dropped from the restated set is gone");
            check(lines.front().rate_minor == 1400, "the new rate is in force");
            check(lines.front().consumed_scaled == 1'000'000,
                  "and the 1,000 cards already run are still counted against the cap");
            const agreements::CapState state = agreements::cap_state(lines.front());
            check(state.remaining_scaled == 4'000'000, "4,000 cards remain");
            check(!state.nearing && !state.exceeded, "which is nowhere near the cap");
            check(agreements::data::lines_needing_attention(store, kAgreeA).empty(),
                  "so nothing asks for attention yet");
        });

        check(shop.run(protocol::OperationId::agreement_update, kAgreeA,
                       payload({{"reason", "Cap cut to what is left"},
                                {"line.0.id", kLineA},
                                {"line.0.product_id", kProduct},
                                {"line.0.agreed_name", "Business card, 350gsm"}},
                               {{"line_count", 1},
                                {"line.0.rate_minor", 1400},
                                {"line.0.cap_scaled", 1'000'000}}),
                       session)
                  .ok,
              "the cap can be cut to exactly what has already been run");
        shop.read([](const engine::Store& store) {
            const auto attention = agreements::data::lines_needing_attention(store, kAgreeA);
            check(attention.size() == 1, "and that immediately asks for attention");
            check(agreements::cap_state(attention.front()).remaining_scaled == 0,
                  "with nothing left to run");
        });
    }

    section("bad rates are refused whole and never half written");
    {
        Shop shop;
        const auto refused = [&](const engine::Blob& body, const char* what) {
            check(!shop.run(protocol::OperationId::agreement_create, kAgreeA, body, session).ok,
                  what);
        };
        refused(payload({{"line.0.id", kLineA},
                         {"line.0.product_id", kProduct},
                         {"line.0.agreed_name", "Nameless customer"}},
                        {{"line_count", 1}, {"line.0.rate_minor", 100}}),
                "an agreement with no customer is refused");
        refused(payload({{"party_id", "  "},
                         {"line.0.id", kLineA},
                         {"line.0.product_id", kProduct},
                         {"line.0.agreed_name", "Blank customer"}},
                        {{"line_count", 1}, {"line.0.rate_minor", 100}}),
                "and so is one whose customer is made of spaces");
        refused(payload({{"party_id", kParty}}, {}),
                "an agreement that does not say how many rates it sets is refused");
        refused(payload({{"party_id", kParty}}, {{"line_count", 0}}),
                "an agreement that agrees no rates at all is not an agreement");
        refused(payload({{"party_id", kParty}}, {{"line_count", -1}}),
                "a negative number of rates is refused before anything is read");
        refused(payload({{"party_id", kParty}}, {{"line_count", 501}}),
                "more rates than this shop can record is refused");
        refused(payload({{"party_id", kParty}}, {{"line_count", 1'000'000'000}}),
                "an absurd count is refused before anything is allocated");
        refused(payload({{"party_id", kParty},
                         {"line.0.id", kLineA},
                         {"line.0.product_id", kProduct},
                         {"line.0.agreed_name", "Owed money"}},
                        {{"line_count", 1}, {"line.0.rate_minor", -100}}),
                "a negative agreed rate is refused");
        refused(payload({{"party_id", kParty},
                         {"line.0.id", kLineA},
                         {"line.0.product_id", kProduct},
                         {"line.0.agreed_name", "Negative cap"}},
                        {{"line_count", 1},
                         {"line.0.rate_minor", 100},
                         {"line.0.cap_scaled", -5}}),
                "a negative cap is refused");
        refused(payload({{"party_id", kParty},
                         {"line.0.id", kLineA},
                         {"line.0.product_id", kProduct},
                         {"line.0.agreed_name", "Too much money"}},
                        {{"line_count", 1},
                         {"line.0.rate_minor", 9'000'000'000'000},
                         {"line.0.cap_scaled", 9'000'000'000'000'000}}),
                "a cap worth more than money can hold is refused, not wrapped");
        refused(payload({{"party_id", kParty},
                         {"line.0.id", kLineA},
                         {"line.0.product_id", kProduct},
                         {"line.0.agreed_name", "Good"},
                         {"line.1.id", kLineB},
                         {"line.1.product_id", kProduct}},
                        {{"line_count", 2},
                         {"line.0.rate_minor", 100},
                         {"line.1.rate_minor", 200}}),
                "a bad second rate takes the good first one down with it");
        refused(payload({{"party_id", kParty},
                         {"line.0.id", ""},
                         {"line.0.product_id", kProduct},
                         {"line.0.agreed_name", "No record"}},
                        {{"line_count", 1}, {"line.0.rate_minor", 100}}),
                "a rate with no record of its own is refused");

        shop.read([](const engine::Store& store) {
            check(!agreements::data::find_agreement(store, kAgreeA).has_value(),
                  "not one refused attempt left an agreement behind");
            check(store.count(agreements::tables::kLine) == 0,
                  "not one refused attempt left a rate behind");
        });
    }

    section("missing records, rights, malformed payloads and idempotency fail safely");
    {
        Shop shop;
        check(!shop.run(protocol::OperationId::agreement_update, kAgreeA, payload({}), session).ok,
              "an agreement that is not on file cannot be amended");
        check(!shop.run(protocol::OperationId::agreement_close, kAgreeA,
                        payload({{"reason", "Gone"}, {"close_effect", "catalog"}}), session)
                   .ok,
              "nor closed");
        check(!shop.run(protocol::OperationId::agreement_reopen, kAgreeA,
                        payload({{"reason", "Gone"}}), session)
                   .ok,
              "nor reopened");

        // Creating and amending are synchronizable, so the registry refuses a
        // call with no record to order it against before the module is reached.
        // That is a wiring error, not a refusal: nobody at the counter can
        // cause it or fix it, so it is loud.
        bool loud = false;
        try {
            shop.run(protocol::OperationId::agreement_create, "", one_rate(), session);
        } catch (const modules::RegistryError&) {
            loud = true;
        }
        check(loud, "a synchronizable call with no record id is a wiring error");

        // Closing is not synchronizable, so it reaches the module, and the
        // module's own guard is what holds. This records which layer catches it.
        const modules::Outcome nameless = shop.run(
            protocol::OperationId::agreement_close, "",
            payload({{"reason", "Gone"}, {"close_effect", "catalog"}}), session);
        check(!nameless.ok, "a close with no record id is refused");
        check(nameless.error == "This request does not identify its record.",
              "by the module's own guard, in words");

        engine::Session unsigned_session;
        unsigned_session.rights.grant_all();
        const modules::Outcome nobody = shop.run(
            protocol::OperationId::agreement_create, kAgreeA, one_rate(), unsigned_session);
        check(!nobody.ok, "rights do not replace authentication");
        check(nobody.reason == engine::DenialReason::NotSignedIn, "and the refusal says so");

        const engine::Session writer = staff({protocol::RightId::right_agreement_write});
        const engine::Session closer = staff({protocol::RightId::right_agreement_close});
        const engine::Session reader = staff({protocol::RightId::right_agreement_read});

        check(!shop.run(protocol::OperationId::agreement_create, kAgreeA, one_rate(), reader).ok,
              "reading agreements does not let you write one");
        check(shop.run(protocol::OperationId::agreement_create, kAgreeA, one_rate(), writer).ok,
              "the write right is enough to strike a bargain");
        check(shop.run(protocol::OperationId::agreement_update, kAgreeA,
                       payload({{"action", "open"}}), writer)
                  .ok,
              "and to bring it into force");

        const modules::Outcome not_yours = shop.run(
            protocol::OperationId::agreement_close, kAgreeA,
            payload({{"reason", "Not mine to close"}, {"close_effect", "catalog"}}), writer);
        check(!not_yours.ok, "but not to close it");
        check(not_yours.reason == engine::DenialReason::NoRight,
              "closing is a right of its own, and is checked as one");
        check(!shop.run(protocol::OperationId::agreement_create, kAgreeB, one_rate(), closer).ok,
              "and the right to close does not let you write a new agreement");
        check(shop.run(protocol::OperationId::agreement_close, kAgreeA,
                       payload({{"reason", "Season over"}, {"close_effect", "catalog"}}), closer)
                  .ok,
              "the closing right closes it");

        const modules::Outcome malformed = shop.run(
            protocol::OperationId::agreement_create, kAgreeB, engine::Blob{1, 2, 3, 4}, session);
        check(!malformed.ok &&
                  malformed.error == "This request could not be read. Please try it again.",
              "a malformed payload is refused in words a person can act on");
        const modules::Outcome malformed_close = shop.run(
            protocol::OperationId::agreement_close, kAgreeA, engine::Blob{9, 9, 9}, session);
        check(!malformed_close.ok &&
                  malformed_close.error == "This request could not be read. Please try it again.",
              "and so is a malformed close");

        modules::Call call;
        call.operation = protocol::OperationId::agreement_create;
        call.record_id = kAgreeC;
        call.payload = payload({{"party_id", kParty},
                                {"line.0.id", kLineC},
                                {"line.0.product_id", kProduct},
                                {"line.0.agreed_name", "Replayed"}},
                               {{"line_count", 1}, {"line.0.rate_minor", 700}});
        call.idempotency_key = "the-same-key";
        const modules::Outcome first = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Online);
        const modules::Outcome replay = shop.registry.run(
            *shop.database, call, session, engine::ConnectionState::Online);
        check(first.ok && !first.replayed, "the first attempt goes in");
        check(replay.ok && replay.replayed, "an interrupted retry is flagged and harmless");
        shop.read([](const engine::Store& store) {
            check(agreements::data::lines_for_agreement(store, kAgreeC).size() == 1,
                  "and did not agree the same rate twice");
            check(agreements::data::agreements_for_party(store, kParty).size() == 2,
                  "the customer has exactly the agreements that were actually struck");
        });
    }

    return squiflow::testing::report();
}
