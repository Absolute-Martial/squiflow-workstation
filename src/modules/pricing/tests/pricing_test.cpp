// Pricing module - the resolution rules first, then the wire.
//
// Two halves, deliberately:
//
//   1. choose_rate and the validators are pure. They are tested directly, with
//      hand-built candidate lists, because the interesting decisions live
//      there and a decision only reachable through a database is a decision
//      that gets tested lightly. This is where the boundary and tie-break
//      cases are, including ones no screen would ever produce.
//
//   2. The four operations are driven through the registry, so the gate, the
//      transaction and the outbox are all in the path.
//
// The rule the whole module exists to protect: a price that cannot be found is
// reported as not found. It never becomes zero, because a zero on an invoice
// is a free item nobody authorised.

#include <initializer_list>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/pricing/data/repository.hpp"
#include "modules/pricing/module.hpp"
#include "modules/pricing/service/pricing_service.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace engine   = squiflow::engine;
namespace modules  = squiflow::modules;
namespace pricing  = squiflow::modules::pricing;
namespace protocol = squiflow::protocol;

namespace {

std::int64_t g_now = 1700000000000;
std::int64_t now() { return g_now += 1000; }

int g_key = 0;
std::string next_key() { return "k-pricing-" + std::to_string(++g_key); }

// Ids are 32 hex characters, as the engine requires of a record id.
const std::string kBanner   = "30000000000000000000000000000001";
const std::string kFlex     = "30000000000000000000000000000002";
const std::string kRateA    = "31000000000000000000000000000001";
const std::string kRateB    = "31000000000000000000000000000002";
const std::string kRateC    = "31000000000000000000000000000003";
const std::string kOverride = "32000000000000000000000000000001";
const std::string kOverrid2 = "32000000000000000000000000000002";
const std::string kSchool   = "33000000000000000000000000000001";
const std::string kHospital = "33000000000000000000000000000002";
const std::string kLine     = "34000000000000000000000000000001";
const std::string kLine2    = "34000000000000000000000000000002";

// A payload of text and integer fields. Amounts must travel as integers; the
// service refuses a number sent as text rather than defaulting it, and one of
// the tests below proves that.
engine::Blob fields(
    std::initializer_list<std::pair<std::string, std::string>> text_pairs,
    std::initializer_list<std::pair<std::string, std::int64_t>> int_pairs = {}) {
    engine::Row row;
    for (const auto& [k, v] : text_pairs) row.set(k, engine::Value::text(v));
    for (const auto& [k, v] : int_pairs) row.set(k, engine::Value::integer(v));
    return engine::encode_payload(row);
}

// Did this throw a rule violation? Used for the validators, which are called
// directly rather than through the gate.
template <typename Fn>
bool refused(Fn&& fn) {
    try {
        fn();
        return false;
    } catch (const modules::RuleViolation&) {
        return true;
    }
}

pricing::Rate make_rate(const std::string& id,
                        const std::string& product,
                        const std::string& party,
                        std::int64_t amount,
                        std::int64_t from = 0,
                        std::int64_t until = 0,
                        std::int64_t created = 0) {
    pricing::Rate rate;
    rate.id = id;
    rate.product_id = product;
    rate.party_id = party;
    rate.amount_minor = amount;
    rate.valid_from = from;
    rate.valid_until = until;
    rate.created_at = created;
    rate.created_by = "tester";
    return rate;
}

struct Shop {
    modules::Registry registry{now};
    std::unique_ptr<engine::Database> database;

    Shop() {
        registry.add(pricing::make_module(now));
        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
    }

    modules::Outcome run(protocol::OperationId op, const std::string& record,
                         const engine::Blob& payload, const engine::Session& session,
                         const std::string& key) {
        modules::Call call;
        call.operation = op;
        call.record_id = record;
        call.payload = payload;
        call.idempotency_key = key;
        return registry.run(*database, call, session, engine::ConnectionState::Online);
    }

    modules::Outcome run(protocol::OperationId op, const std::string& record,
                         const engine::Session& session, const std::string& key) {
        return run(op, record, {}, session, key);
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

// Can read prices, cannot set them.
engine::Session reader_session() {
    engine::Session session;
    session.person = engine::record_id_from_string("00000000000000000000000000000002");
    session.device = engine::RecordId{1, 2};
    session.display_name = "Staff";
    session.is_owner = false;
    session.rights.grant(protocol::RightId::right_rate_read);
    return session;
}

// Can set the price list but cannot deviate from it. Overriding a price is a
// separate right precisely so that this person exists.
engine::Session setter_session() {
    engine::Session session;
    session.person = engine::record_id_from_string("00000000000000000000000000000003");
    session.device = engine::RecordId{1, 3};
    session.display_name = "Assistant";
    session.is_owner = false;
    session.rights.grant(protocol::RightId::right_rate_read);
    session.rights.grant(protocol::RightId::right_rate_write);
    return session;
}

}  // namespace

int main() {
    // ===================================================================
    // Part one: resolution, with no database anywhere near it.
    // ===================================================================

    section("nothing found stays nothing found");
    {
        const std::vector<pricing::Rate> none;
        const pricing::ResolvedRate empty = pricing::choose_rate(none, kSchool, 5000);
        check(!empty.found(), "an empty candidate list finds nothing");
        check(empty.source == pricing::RateSource::None, "and reports None");
        check(empty.amount_minor == 0, "the amount is zero but found() is false");

        // The distinction this module exists for: no default must not become a
        // price of zero.
        const pricing::DefaultRate absent{};
        const pricing::ResolvedRate still_none = pricing::with_default(empty, absent);
        check(!still_none.found(), "an absent default does not become a free item");
        check(still_none.source == pricing::RateSource::None, "it stays None");
    }

    section("the more specific rate wins");
    {
        std::vector<pricing::Rate> candidates;
        candidates.push_back(make_rate(kRateA, kBanner, "", 50000));
        candidates.push_back(make_rate(kRateB, kBanner, kSchool, 40000));

        const pricing::ResolvedRate for_school = pricing::choose_rate(candidates, kSchool, 5000);
        check(for_school.source == pricing::RateSource::PartyRate, "the school gets its agreed rate");
        check(for_school.amount_minor == 40000, "at the agreed amount");
        check(for_school.rate_id == kRateB, "and the winning rate is named");

        const pricing::ResolvedRate for_other = pricing::choose_rate(candidates, kHospital, 5000);
        check(for_other.source == pricing::RateSource::CatchAllRate,
              "a different party falls back to the list rate");
        check(for_other.amount_minor == 50000, "at the list amount");

        // The leak this guards against: a walk-in with no party must not be
        // handed a price negotiated for somebody else.
        const pricing::ResolvedRate walk_in = pricing::choose_rate(candidates, "", 5000);
        check(walk_in.source == pricing::RateSource::CatchAllRate,
              "a walk-in does not inherit a party's agreed rate");
        check(walk_in.amount_minor == 50000, "the walk-in pays the list price");
    }

    section("a party rate alone does not serve a walk-in at all");
    {
        std::vector<pricing::Rate> only_party;
        only_party.push_back(make_rate(kRateB, kBanner, kSchool, 40000));

        const pricing::ResolvedRate walk_in = pricing::choose_rate(only_party, "", 5000);
        check(!walk_in.found(), "with only a party rate, a walk-in has no price");

        const pricing::ResolvedRate stranger = pricing::choose_rate(only_party, kHospital, 5000);
        check(!stranger.found(), "and neither does a different party");
    }

    section("the window is half-open");
    {
        std::vector<pricing::Rate> windowed;
        windowed.push_back(make_rate(kRateA, kBanner, "", 50000, 1000, 2000));

        check(!pricing::covers(windowed[0], 999), "the instant before the start is outside");
        check(pricing::covers(windowed[0], 1000), "the start instant itself is inside");
        check(pricing::covers(windowed[0], 1999), "the instant before the end is inside");
        check(!pricing::covers(windowed[0], 2000), "the end instant itself is outside");
        check(!pricing::covers(windowed[0], 2001), "and after the end is outside");

        // Two consecutive windows sharing a boundary: exactly one may match, or
        // the price at that instant depends on which row was read first.
        std::vector<pricing::Rate> consecutive;
        consecutive.push_back(make_rate(kRateA, kBanner, "", 50000, 1000, 2000));
        consecutive.push_back(make_rate(kRateB, kBanner, "", 60000, 2000, 3000));

        const pricing::ResolvedRate at_boundary = pricing::choose_rate(consecutive, "", 2000);
        check(at_boundary.amount_minor == 60000,
              "at a shared boundary the later window owns the instant");
        check(at_boundary.rate_id == kRateB, "and it is unambiguous which rate that is");
    }

    section("open-ended bounds mean what they say");
    {
        const pricing::Rate always = make_rate(kRateA, kBanner, "", 50000, 0, 0);
        check(pricing::covers(always, 0), "a rate with no bounds covers the epoch");
        check(pricing::covers(always, 9000000000000), "and covers a distant future");

        const pricing::Rate no_expiry = make_rate(kRateB, kBanner, "", 50000, 1000, 0);
        check(!pricing::covers(no_expiry, 999), "a start with no expiry still has a start");
        check(pricing::covers(no_expiry, 9000000000000), "but never ends");

        const pricing::Rate always_until = make_rate(kRateC, kBanner, "", 50000, 0, 2000);
        check(pricing::covers(always_until, 0), "an expiry with no start has always applied");
        check(!pricing::covers(always_until, 2000), "until it expires");
    }

    section("two rates of equal specificity resolve the same way every time");
    {
        // Bad data - two agreed rates for one party both covering this instant.
        // The answer must still not depend on the order the rows arrived in, so
        // the same set is resolved in both orders and must agree.
        std::vector<pricing::Rate> forward;
        forward.push_back(make_rate(kRateA, kBanner, kSchool, 40000, 1000, 0, 10));
        forward.push_back(make_rate(kRateB, kBanner, kSchool, 35000, 1500, 0, 10));

        std::vector<pricing::Rate> reversed;
        reversed.push_back(forward[1]);
        reversed.push_back(forward[0]);

        const pricing::ResolvedRate a = pricing::choose_rate(forward, kSchool, 5000);
        const pricing::ResolvedRate b = pricing::choose_rate(reversed, kSchool, 5000);
        check(a.rate_id == b.rate_id, "row order does not change the winner");
        check(a.amount_minor == 35000, "the later start date wins");

        // Same start: the later creation wins.
        std::vector<pricing::Rate> same_start;
        same_start.push_back(make_rate(kRateA, kBanner, kSchool, 40000, 1000, 0, 10));
        same_start.push_back(make_rate(kRateB, kBanner, kSchool, 35000, 1000, 0, 20));
        const pricing::ResolvedRate newer = pricing::choose_rate(same_start, kSchool, 5000);
        check(newer.rate_id == kRateB, "with equal starts the later record wins");

        // Same start and same creation instant - two devices, same second. The
        // id breaks the tie, so two machines offline still agree.
        std::vector<pricing::Rate> same_everything;
        same_everything.push_back(make_rate(kRateA, kBanner, kSchool, 40000, 1000, 0, 10));
        same_everything.push_back(make_rate(kRateB, kBanner, kSchool, 35000, 1000, 0, 10));
        const pricing::ResolvedRate by_id = pricing::choose_rate(same_everything, kSchool, 5000);
        check(by_id.rate_id == kRateB, "a dead heat is broken by id, not by luck");

        std::vector<pricing::Rate> flipped;
        flipped.push_back(same_everything[1]);
        flipped.push_back(same_everything[0]);
        check(pricing::choose_rate(flipped, kSchool, 5000).rate_id == kRateB,
              "and the same id wins from the other direction");
    }

    section("candidates that do not apply are ignored, not resolved");
    {
        // Inapplicable for the two reasons choose_rate actually judges: the
        // rate belongs to somebody else, or its window does not hold this
        // instant. Product is deliberately not one of them, as the next
        // assertion records.
        std::vector<pricing::Rate> mixed;
        mixed.push_back(make_rate(kRateA, kBanner, kHospital, 10000));       // other party
        mixed.push_back(make_rate(kRateB, kBanner, kSchool, 40000, 8000, 9000));  // wrong window
        mixed.push_back(make_rate(kRateC, kBanner, "", 50000, 8000, 9000));  // wrong window

        const pricing::ResolvedRate resolved = pricing::choose_rate(mixed, kSchool, 5000);
        check(!resolved.found(), "nothing applicable means nothing found");

        // choose_rate does not filter by product - that is the repository's
        // job. Proven here so the division of labour is not accidental.
        std::vector<pricing::Rate> other_product;
        other_product.push_back(make_rate(kRateA, kFlex, "", 99999));
        check(pricing::choose_rate(other_product, kSchool, 5000).found(),
              "choose_rate judges applicability, not product identity");
    }

    section("the default sits behind everything");
    {
        pricing::DefaultRate standard;
        standard.product_id = kBanner;
        standard.amount_minor = 45000;

        const pricing::ResolvedRate nothing{};
        const pricing::ResolvedRate fell_back = pricing::with_default(nothing, standard);
        check(fell_back.source == pricing::RateSource::Default, "with no rate, the default applies");
        check(fell_back.amount_minor == 45000, "at the standard amount");
        check(fell_back.rate_id.empty(), "a default has no rate row to name");

        pricing::ResolvedRate won;
        won.source = pricing::RateSource::PartyRate;
        won.amount_minor = 40000;
        won.rate_id = kRateB;
        const pricing::ResolvedRate untouched = pricing::with_default(won, standard);
        check(untouched.amount_minor == 40000, "a found rate is not overwritten by the default");
        check(untouched.source == pricing::RateSource::PartyRate, "and keeps its source");
    }

    section("every source can be named, including one from outside this build");
    {
        check(std::string(pricing::to_string(pricing::RateSource::None)) == "none", "None");
        check(std::string(pricing::to_string(pricing::RateSource::PartyRate)) == "agreed rate", "PartyRate");
        check(std::string(pricing::to_string(pricing::RateSource::CatchAllRate)) == "list rate", "CatchAllRate");
        check(std::string(pricing::to_string(pricing::RateSource::Default)) == "standard price", "Default");

        // A value no enumerator has. It cannot arrive from this code, but it
        // can arrive from a corrupt row or a newer peer, and the answer must be
        // a marker rather than a read past the end of a table.
        const auto foreign = static_cast<pricing::RateSource>(99);
        check(std::string(pricing::to_string(foreign)) == "?", "an unknown source is named safely");
    }

    // ===================================================================
    // Part two: the validators, called directly with data no screen makes.
    // ===================================================================

    section("a rate must be a price somebody could actually charge");
    {
        check(refused([] { pricing::validate(make_rate("", kBanner, "", 100)); }),
              "a rate with no id is refused");
        check(refused([] { pricing::validate(make_rate(kRateA, "", "", 100)); }),
              "a rate with no product is refused");
        check(refused([] { pricing::validate(make_rate(kRateA, kBanner, "", -1)); }),
              "a negative rate is refused");

        // The boundary itself is legal; one beyond it is not.
        check(!refused([] {
                  pricing::validate(make_rate(kRateA, kBanner, "", pricing::kMaxAmountMinor));
              }),
              "the largest sane amount is accepted");
        check(refused([] {
                  pricing::validate(make_rate(kRateA, kBanner, "", pricing::kMaxAmountMinor + 1));
              }),
              "one unit beyond it is refused");

        // A misplaced decimal point, which is what the ceiling is really for.
        check(refused([] {
                  pricing::validate(make_rate(kRateA, kBanner, "", 9223372036854775807LL));
              }),
              "an amount near the limit of the type is refused");

        check(!refused([] { pricing::validate(make_rate(kRateA, kBanner, "", 0)); }),
              "a rate of zero is allowed - a genuinely free item is a decision, not an error");
    }

    section("a rate window must be able to contain a moment");
    {
        check(refused([] { pricing::validate(make_rate(kRateA, kBanner, "", 100, 2000, 1000)); }),
              "an end before the start is refused");
        check(refused([] { pricing::validate(make_rate(kRateA, kBanner, "", 100, 2000, 2000)); }),
              "an end equal to the start is refused - half-open, it holds nothing");
        check(refused([] { pricing::validate(make_rate(kRateA, kBanner, "", 100, -1, 0)); }),
              "a negative start is refused");
        check(refused([] { pricing::validate(make_rate(kRateA, kBanner, "", 100, 0, -1)); }),
              "a negative end is refused");
        check(!refused([] { pricing::validate(make_rate(kRateA, kBanner, "", 100, 2000, 0)); }),
              "a start with no expiry is fine");
    }

    section("a price change must be explainable");
    {
        auto override_with = [](const std::string& id, const std::string& line,
                                std::int64_t amount, const std::string& reason) {
            pricing::RateOverride o;
            o.id = id;
            o.line_id = line;
            o.overridden_minor = amount;
            o.reason = reason;
            return o;
        };

        check(refused([&] { pricing::validate(override_with("", kLine, 100, "goodwill")); }),
              "an override with no id is refused");
        check(refused([&] { pricing::validate(override_with(kOverride, "", 100, "goodwill")); }),
              "an override with no line is refused");
        check(refused([&] { pricing::validate(override_with(kOverride, kLine, -1, "goodwill")); }),
              "a negative changed price is refused");
        check(refused([&] { pricing::validate(override_with(kOverride, kLine, 100, "")); }),
              "an override with no reason is refused");

        // The one that matters: a reason that looks filled in but is not.
        check(refused([&] { pricing::validate(override_with(kOverride, kLine, 100, "   ")); }),
              "a reason of spaces is not a reason");
        check(refused([&] { pricing::validate(override_with(kOverride, kLine, 100, "\t\n ")); }),
              "nor is a reason of whitespace characters");
        check(!refused([&] { pricing::validate(override_with(kOverride, kLine, 100, "x")); }),
              "a single character is accepted - brevity is the shopkeeper's business");
    }

    section("a standard price is checked too");
    {
        auto default_of = [](const std::string& product, std::int64_t amount) {
            pricing::DefaultRate d;
            d.product_id = product;
            d.amount_minor = amount;
            return d;
        };
        check(refused([&] { pricing::validate(default_of("", 100)); }),
              "a standard price with no product is refused");
        check(refused([&] { pricing::validate(default_of(kBanner, -1)); }),
              "a negative standard price is refused");
        check(refused([&] { pricing::validate(default_of(kBanner, pricing::kMaxAmountMinor + 1)); }),
              "an absurd standard price is refused");
    }

    // ===================================================================
    // Part three: through the registry.
    // ===================================================================

    section("a rate is set and can then be resolved");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const modules::Outcome set = shop.run(
            protocol::OperationId::rate_set, kRateA,
            fields({{"product_id", kBanner}, {"party_id", kSchool}}, {{"amount_minor", 40000}}),
            owner, next_key());
        check(set.ok, "the rate is accepted");

        shop.look([&](const engine::Store& store) {
            const auto stored = pricing::data::find_rate(store, kRateA);
            check(stored.has_value(), "the rate is in the store");
            check(stored->amount_minor == 40000, "with the amount that was sent");
            check(stored->created_at != 0, "and a recorded creation time");
            check(!stored->created_by.empty(), "and the person who set it");

            const pricing::ResolvedRate resolved =
                pricing::resolve_rate(store, kBanner, kSchool, 5000);
            check(resolved.source == pricing::RateSource::PartyRate, "and it resolves for that party");
            check(resolved.amount_minor == 40000, "at the right amount");

            const pricing::ResolvedRate other =
                pricing::resolve_rate(store, kBanner, kHospital, 5000);
            check(!other.found(), "and does not resolve for anybody else");
        });
    }

    section("an unknown product has no price rather than a price of zero");
    {
        Shop shop;
        shop.look([&](const engine::Store& store) {
            const pricing::ResolvedRate nothing =
                pricing::resolve_rate(store, kFlex, kSchool, 5000);
            check(!nothing.found(), "a product nobody priced has no price");
            check(nothing.source == pricing::RateSource::None, "and says so");

            const pricing::ResolvedRate blank_product =
                pricing::resolve_rate(store, "", kSchool, 5000);
            check(!blank_product.found(), "and neither does an empty product id");

            const pricing::EffectivePrice line =
                pricing::effective_price(store, kLine, kFlex, kSchool, 5000);
            check(!line.found, "a line for an unpriced product is not priced");
            check(!line.overridden, "and was not overridden");
        });
    }

    section("a price sent as text is refused, not quietly turned into zero");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const modules::Outcome as_text = shop.run(
            protocol::OperationId::rate_set, kRateA,
            fields({{"product_id", kBanner}, {"amount_minor", "40000"}}),
            owner, next_key());
        check(!as_text.ok, "an amount sent as text is refused");
        check(!as_text.error.empty(), "with something a person can read");

        const modules::Outcome missing = shop.run(
            protocol::OperationId::rate_set, kRateB,
            fields({{"product_id", kBanner}}),
            owner, next_key());
        check(!missing.ok, "a missing amount is refused");

        shop.look([&](const engine::Store& store) {
            check(!pricing::data::find_rate(store, kRateA).has_value(),
                  "nothing was written for the text amount");
            check(!pricing::data::find_rate(store, kRateB).has_value(),
                  "nor for the missing one");
        });
    }

    section("correcting a rate keeps its history; moving it does not happen");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        shop.run(protocol::OperationId::rate_set, kRateA,
                 fields({{"product_id", kBanner}}, {{"amount_minor", 40000}}),
                 owner, next_key());

        std::int64_t first_created = 0;
        std::string first_author;
        shop.look([&](const engine::Store& store) {
            const auto stored = pricing::data::find_rate(store, kRateA);
            first_created = stored->created_at;
            first_author = stored->created_by;
        });

        const modules::Outcome corrected = shop.run(
            protocol::OperationId::rate_set, kRateA,
            fields({{"product_id", kBanner}}, {{"amount_minor", 42000}}),
            owner, next_key());
        check(corrected.ok, "a rate can be corrected");

        shop.look([&](const engine::Store& store) {
            const auto stored = pricing::data::find_rate(store, kRateA);
            check(stored->amount_minor == 42000, "the amount changed");
            check(stored->created_at == first_created, "but the creation time did not");
            check(stored->created_by == first_author, "nor did the person who created it");
        });

        // Re-pointing a rate at another product would silently re-price
        // everything that already referred to it.
        const modules::Outcome moved = shop.run(
            protocol::OperationId::rate_set, kRateA,
            fields({{"product_id", kFlex}}, {{"amount_minor", 42000}}),
            owner, next_key());
        check(!moved.ok, "a rate cannot be moved to a different product");

        shop.look([&](const engine::Store& store) {
            check(pricing::data::find_rate(store, kRateA)->product_id == kBanner,
                  "and the rate still belongs to the original product");
        });
    }

    section("removing a rate that is not there is not a complaint");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const modules::Outcome ghost = shop.run(
            protocol::OperationId::rate_remove, kRateC, owner, next_key());
        check(ghost.ok, "removing an absent rate succeeds - the caller wanted it gone");

        shop.run(protocol::OperationId::rate_set, kRateA,
                 fields({{"product_id", kBanner}}, {{"amount_minor", 40000}}),
                 owner, next_key());
        const modules::Outcome removed = shop.run(
            protocol::OperationId::rate_remove, kRateA, owner, next_key());
        check(removed.ok, "an existing rate is removed");

        shop.look([&](const engine::Store& store) {
            check(!pricing::data::find_rate(store, kRateA).has_value(), "and is gone");
            check(!pricing::resolve_rate(store, kBanner, "", 5000).found(),
                  "so the product has no price again");
        });

        const modules::Outcome twice = shop.run(
            protocol::OperationId::rate_remove, kRateA, owner, next_key());
        check(twice.ok, "removing it a second time is still not an error");
    }

    section("the standard price is the last resort");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        shop.run(protocol::OperationId::rate_default_set, kBanner,
                 fields({}, {{"amount_minor", 45000}}), owner, next_key());

        shop.look([&](const engine::Store& store) {
            const pricing::ResolvedRate resolved =
                pricing::resolve_rate(store, kBanner, kSchool, 5000);
            check(resolved.source == pricing::RateSource::Default, "with no rate, the default answers");
            check(resolved.amount_minor == 45000, "at the standard amount");
        });

        shop.run(protocol::OperationId::rate_set, kRateB,
                 fields({{"product_id", kBanner}, {"party_id", kSchool}}, {{"amount_minor", 40000}}),
                 owner, next_key());

        shop.look([&](const engine::Store& store) {
            check(pricing::resolve_rate(store, kBanner, kSchool, 5000).amount_minor == 40000,
                  "an agreed rate takes precedence over the standard price");
            check(pricing::resolve_rate(store, kBanner, kHospital, 5000).source ==
                      pricing::RateSource::Default,
                  "while everybody else still gets the standard price");
        });

        // There is one standard price per product, by key, so setting it again
        // replaces rather than accumulates.
        shop.run(protocol::OperationId::rate_default_set, kBanner,
                 fields({}, {{"amount_minor", 47000}}), owner, next_key());
        shop.look([&](const engine::Store& store) {
            check(pricing::resolve_rate(store, kBanner, kHospital, 5000).amount_minor == 47000,
                  "the standard price is replaced, not duplicated");
        });
    }

    section("an override prices a line, even one with no rate at all");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        // A one-off job that is not in the catalogue and has no rate. If an
        // override could not stand alone, this could never be billed.
        const modules::Outcome one_off = shop.run(
            protocol::OperationId::rate_override, kOverride,
            fields({{"line_id", kLine}, {"reason", "one-off job, agreed at the counter"}},
                   {{"overridden_minor", 12500}}),
            owner, next_key());
        check(one_off.ok, "a line can be priced by hand");

        shop.look([&](const engine::Store& store) {
            const pricing::EffectivePrice price =
                pricing::effective_price(store, kLine, kFlex, kSchool, 5000);
            check(price.found, "the line has a price");
            check(price.amount_minor == 12500, "the one that was agreed");
            check(price.overridden, "marked as a deviation");
            check(!price.reason.empty(), "carrying the reason");
            check(price.source == pricing::RateSource::None,
                  "and honest that no rate underlies it");
        });
    }

    section("an override sits on top of the rate it replaces");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        shop.run(protocol::OperationId::rate_set, kRateA,
                 fields({{"product_id", kBanner}}, {{"amount_minor", 50000}}),
                 owner, next_key());
        shop.run(protocol::OperationId::rate_override, kOverride,
                 fields({{"line_id", kLine}, {"reason", "customer supplied their own paper"}},
                        {{"overridden_minor", 45000}}),
                 owner, next_key());

        shop.look([&](const engine::Store& store) {
            const pricing::EffectivePrice price =
                pricing::effective_price(store, kLine, kBanner, "", 5000);
            check(price.amount_minor == 45000, "the line is charged the changed price");
            check(price.overridden, "and is marked as changed");
            check(price.source == pricing::RateSource::CatchAllRate,
                  "while still reporting what the price would have been based on");

            // A different line is untouched by another line's deviation.
            const pricing::EffectivePrice other =
                pricing::effective_price(store, kLine2, kBanner, "", 5000);
            check(other.amount_minor == 50000, "another line still pays the list price");
            check(!other.overridden, "and is not marked as changed");

            // No line at all - the product's price, with no override lookup.
            const pricing::EffectivePrice bare =
                pricing::effective_price(store, "", kBanner, "", 5000);
            check(bare.amount_minor == 50000, "with no line there is nothing to override");
            check(!bare.overridden, "so nothing is marked");
        });
    }

    section("the last override wins, and the earlier ones are kept");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        shop.run(protocol::OperationId::rate_override, kOverride,
                 fields({{"line_id", kLine}, {"reason", "first agreement"}},
                        {{"overridden_minor", 45000}}),
                 owner, next_key());
        shop.run(protocol::OperationId::rate_override, kOverrid2,
                 fields({{"line_id", kLine}, {"reason", "customer renegotiated"}},
                        {{"overridden_minor", 43000}}),
                 owner, next_key());

        shop.look([&](const engine::Store& store) {
            const auto all = pricing::data::overrides_for_line(store, kLine);
            check(all.size() == 2, "both price changes are kept");
            check(all.front().overridden_minor == 45000, "oldest first");
            check(all.back().overridden_minor == 43000, "newest last");

            const auto latest = pricing::data::latest_override_for_line(store, kLine);
            check(latest.has_value(), "there is a governing change");
            check(latest->overridden_minor == 43000, "and it is the most recent one");

            check(pricing::effective_price(store, kLine, kBanner, "", 5000).amount_minor == 43000,
                  "the line is charged the most recent agreement");

            check(pricing::data::overrides_for_line(store, kLine2).empty(),
                  "a line with no changes has none");
            check(!pricing::data::latest_override_for_line(store, kLine2).has_value(),
                  "and no governing change");
        });
    }

    section("a recorded price change is never rewritten");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        shop.run(protocol::OperationId::rate_override, kOverride,
                 fields({{"line_id", kLine}, {"reason", "goodwill"}},
                        {{"overridden_minor", 45000}}),
                 owner, next_key());

        // Same override id, new key - not a replay, an attempt to rewrite
        // history. The reason field is worthless if the record can be edited.
        const modules::Outcome rewrite = shop.run(
            protocol::OperationId::rate_override, kOverride,
            fields({{"line_id", kLine}, {"reason", "actually, a bigger discount"}},
                   {{"overridden_minor", 20000}}),
            owner, next_key());
        check(!rewrite.ok, "reusing a price-change id is refused");

        shop.look([&](const engine::Store& store) {
            check(pricing::data::find_override(store, kOverride)->overridden_minor == 45000,
                  "and the original change is intact");
        });
    }

    section("an override still needs a reason when it comes off the wire");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const modules::Outcome no_reason = shop.run(
            protocol::OperationId::rate_override, kOverride,
            fields({{"line_id", kLine}}, {{"overridden_minor", 45000}}),
            owner, next_key());
        check(!no_reason.ok, "a price change with no reason is refused at the wire too");

        const modules::Outcome no_line = shop.run(
            protocol::OperationId::rate_override, kOverrid2,
            fields({{"reason", "goodwill"}}, {{"overridden_minor", 45000}}),
            owner, next_key());
        check(!no_line.ok, "and one that names no line is refused");

        shop.look([&](const engine::Store& store) {
            check(pricing::data::overrides_for_line(store, kLine).empty(),
                  "neither was recorded");
        });
    }

    section("a replay is accepted once and flagged");
    {
        Shop shop;
        const engine::Session owner = owner_session();
        const std::string key = next_key();

        const modules::Outcome first = shop.run(
            protocol::OperationId::rate_set, kRateA,
            fields({{"product_id", kBanner}}, {{"amount_minor", 40000}}),
            owner, key);
        check(first.ok, "the first send is applied");
        check(!first.replayed, "and is not a replay");

        const modules::Outcome again = shop.run(
            protocol::OperationId::rate_set, kRateA,
            fields({{"product_id", kBanner}}, {{"amount_minor", 40000}}),
            owner, key);
        check(again.ok, "the same send arriving twice is accepted");
        check(again.replayed, "and flagged as a replay rather than applied again");
    }

    section("the rights are enforced at the gate");
    {
        Shop shop;
        const engine::Session reader = reader_session();
        const engine::Session setter = setter_session();

        check(!shop.run(protocol::OperationId::rate_set, kRateA,
                        fields({{"product_id", kBanner}}, {{"amount_minor", 40000}}),
                        reader, next_key()).ok,
              "setting a rate is refused without right_rate_write");
        check(!shop.run(protocol::OperationId::rate_remove, kRateA, reader, next_key()).ok,
              "removing a rate is refused without right_rate_write");
        check(!shop.run(protocol::OperationId::rate_default_set, kBanner,
                        fields({}, {{"amount_minor", 45000}}), reader, next_key()).ok,
              "setting the standard price is refused without right_rate_write");

        // The distinction the third right exists for: this person may maintain
        // the price list but may not deviate from it on a line.
        check(shop.run(protocol::OperationId::rate_set, kRateA,
                       fields({{"product_id", kBanner}}, {{"amount_minor", 40000}}),
                       setter, next_key()).ok,
              "a price setter may set the list");
        check(!shop.run(protocol::OperationId::rate_override, kOverride,
                        fields({{"line_id", kLine}, {"reason", "goodwill"}},
                               {{"overridden_minor", 20000}}),
                        setter, next_key()).ok,
              "but may not override a price without right_rate_override");

        shop.look([&](const engine::Store& store) {
            check(pricing::data::overrides_for_line(store, kLine).empty(),
                  "and no deviation was recorded");
        });
    }

    section("a damaged payload is refused with a clear message");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        const engine::Blob garbage = {0x01, 0x02, 0x03, 0x04, 0x05};
        const modules::Outcome bad = shop.run(
            protocol::OperationId::rate_set, kRateA, garbage, owner, next_key());
        check(!bad.ok, "a damaged payload is refused");
        check(!bad.error.empty(), "with a message rather than a crash");

        shop.look([&](const engine::Store& store) {
            check(!pricing::data::find_rate(store, kRateA).has_value(),
                  "and nothing was written");
        });
    }

    section("a request that names no record never reaches this module");
    {
        // Every pricing operation is synchronisable, and the registry refuses a
        // synchronisable call with no record to order it against. That is a
        // RegistryError, not a refusal: a person cannot cause it and cannot fix
        // it, so it is loud rather than shown. The module's own subject() guard
        // is therefore a second line of defence, unreachable from the wire, and
        // this test records which layer actually holds.
        Shop shop;
        const engine::Session owner = owner_session();

        bool loud = false;
        try {
            shop.run(protocol::OperationId::rate_set, "",
                     fields({{"product_id", kBanner}}, {{"amount_minor", 40000}}),
                     owner, next_key());
        } catch (const modules::RegistryError&) {
            loud = true;
        }
        check(loud, "a synchronisable call with no record id is a wiring error, not a refusal");

        shop.look([&](const engine::Store& store) {
            check(pricing::data::rates_for_product(store, kBanner).empty(),
                  "and nothing was written");
        });
    }

    section("many rates for one product do not confuse resolution");
    {
        Shop shop;
        const engine::Session owner = owner_session();

        // A product priced for two parties and everybody else at once.
        shop.run(protocol::OperationId::rate_set, kRateA,
                 fields({{"product_id", kBanner}}, {{"amount_minor", 50000}}),
                 owner, next_key());
        shop.run(protocol::OperationId::rate_set, kRateB,
                 fields({{"product_id", kBanner}, {"party_id", kSchool}}, {{"amount_minor", 40000}}),
                 owner, next_key());
        shop.run(protocol::OperationId::rate_set, kRateC,
                 fields({{"product_id", kBanner}, {"party_id", kHospital}}, {{"amount_minor", 38000}}),
                 owner, next_key());

        shop.look([&](const engine::Store& store) {
            check(pricing::data::rates_for_product(store, kBanner).size() == 3,
                  "all three rates are stored against the product");
            check(pricing::data::rates_for_product(store, kFlex).empty(),
                  "and none against a product with no rates");

            check(pricing::resolve_rate(store, kBanner, kSchool, 5000).amount_minor == 40000,
                  "the school gets its rate");
            check(pricing::resolve_rate(store, kBanner, kHospital, 5000).amount_minor == 38000,
                  "the hospital gets its own");
            check(pricing::resolve_rate(store, kBanner, "", 5000).amount_minor == 50000,
                  "and a walk-in gets the list price");
        });
    }

    return squiflow::testing::report();
}
