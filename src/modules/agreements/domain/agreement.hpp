#pragma once

// An agreed price list for one customer, for a period, optionally capped by
// quantity.
//
// Two records rather than one. The head carries the bargain itself - who it is
// with, how long it lasts, what happens to anything outside it - and the lines
// carry the agreed rates. Lines are separate because a cap is consumed against
// a line, not against the agreement, and because the same product may appear
// twice under two different agreed names at two different rates.
//
// This module owns the counter, not the event that moves it. A job created
// under an agreement consumes against the cap, but the job workflow is a later
// phase; what belongs here is the arithmetic, and the refusal to let that
// arithmetic go wrong.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/records/money.hpp"
#include "engine/records/quantity.hpp"
#include "engine/records/snapshot.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::agreements {

// Four states, and deliberately only four. The written lifecycle also names
// "expiring", but expiring is not a state anyone writes: it is what a validity
// date and a clock say together. Storing it would mean a row could claim to be
// open while its date said otherwise, and then two screens would disagree.
enum class AgreementState : std::uint8_t {
    Draft,
    Open,
    Closed,
    Superseded,
};

// What to charge for work outside the agreed list. An agreement that lists
// three products does not thereby forbid the fourth; it has to say what the
// fourth costs.
enum class FallbackRule : std::uint8_t {
    CatalogPrice,
    RefuseOutsideScope,
};

// Closing an agreement asks explicitly what happens to jobs still running
// under it. There is no default: the two answers bill differently, and
// guessing on the shopkeeper's behalf is how a customer gets a surprise.
enum class CloseEffect : std::uint8_t {
    KeepAgreedRate,
    RevertToCatalog,
};

const char* to_string(AgreementState state) noexcept;
const char* to_string(FallbackRule rule) noexcept;
const char* to_string(CloseEffect effect) noexcept;

bool transition_allowed(AgreementState from, AgreementState to) noexcept;

// A draft is still being negotiated; an open agreement is in force. Both can
// still be edited. A closed or superseded one cannot.
constexpr bool can_amend(AgreementState state) noexcept {
    return state == AgreementState::Draft || state == AgreementState::Open;
}

// A cap is "nearing" from nine tenths of the way through. The number is a
// judgement, not a discovery, so it is written down once here rather than
// spelled out at each call site.
inline constexpr std::int64_t kNearingNumerator = 9;
inline constexpr std::int64_t kNearingDenominator = 10;

struct Agreement {
    std::string id{};

    // Required. Unlike a quotation, an agreement is a bargain with a named
    // party; there is no walk-in equivalent.
    std::string party_id{};

    AgreementState state{AgreementState::Draft};

    // The quotation this was built from, when it was. Empty is legitimate: an
    // agreement can be struck across a counter without a paper trail before it.
    std::string source_quotation_id{};

    // The chain, readable end to end in both directions.
    std::string supersedes{};
    std::string superseded_by{};
    std::string renewed_from{};

    std::int64_t valid_from{0};

    // Zero means open-ended. An open-ended agreement never expires and must
    // never raise an attention item, because nagging about a thing that cannot
    // lapse trains people to ignore the warnings that matter.
    std::int64_t valid_until{0};

    FallbackRule fallback{FallbackRule::CatalogPrice};

    // The terms as they stood when this was agreed. Stored, not referenced:
    // editing a template later must never rewrite a signed agreement.
    std::string terms{};

    std::string customer_reference{};
    std::string note{};

    // Signature evidence. The artefact is a file reference; this module does
    // not read it, only refuses to lose it.
    std::string signed_by_name{};
    std::int64_t signed_on{0};
    std::string signed_artifact{};

    std::int64_t created_at{0};
    std::string created_by{};
    std::int64_t opened_at{0};
    std::string opened_by{};
    std::int64_t closed_at{0};
    std::string closed_by{};
    std::string close_reason{};
    CloseEffect close_effect{CloseEffect::RevertToCatalog};
    std::int64_t reopened_at{0};
    std::string reopened_by{};
    std::string reopen_reason{};
};

// One agreed rate, for one agreed name.
struct AgreementLine {
    std::string id{};
    std::string agreement_id{};

    std::int64_t position{0};

    // Required. An agreement prices catalog products; there is no off-catalog
    // line here, because a rate nobody can look up cannot be applied
    // automatically to a later job.
    std::string product_id{};

    // The name the customer agreed to, which is the name that will appear on
    // the job and the invoice. Two lines may name the same product
    // differently and price it differently on purpose.
    std::string agreed_name{};
    std::string specifications{};

    std::int64_t rate_minor{0};

    // Zero means uncapped. Any other value is a ceiling that consumption is
    // measured against.
    std::int64_t cap_scaled{0};
    std::int64_t consumed_scaled{0};

    std::string rate_reason{};
};

// What the agreement screen shows for one line: agreed, consumed, remaining.
struct CapState {
    bool capped{false};
    std::int64_t agreed_scaled{0};
    std::int64_t consumed_scaled{0};

    // Zero once the cap is reached or passed. Never negative: a screen showing
    // "-200 remaining" is a screen nobody can act on.
    std::int64_t remaining_scaled{0};

    bool nearing{false};
    bool exceeded{false};
};

CapState cap_state(const AgreementLine& line) noexcept;

// The result of moving a cap counter. Not a bool, because "it worked" and "it
// worked but you are now over the cap" are different answers and the second
// one needs an override with a reason.
struct ConsumeResult {
    bool ok{false};
    std::int64_t consumed_scaled{0};
    bool exceeds_cap{false};
};

// Consuming a non-positive quantity is refused rather than ignored: it means
// the caller computed something wrong, and silently doing nothing hides it.
ConsumeResult consume_quantity(const AgreementLine& line,
                               std::int64_t quantity_scaled) noexcept;

// Releasing more than was consumed is refused. A cancelled job cannot give
// back quantity the agreement never took.
ConsumeResult release_quantity(const AgreementLine& line,
                               std::int64_t quantity_scaled) noexcept;

// True only for agreements that can lapse at all.
bool has_expiry(const Agreement& agreement) noexcept;

// Past its date at the moment given. An open-ended agreement is never lapsed.
bool lapsed_at_moment(const Agreement& agreement, std::int64_t at) noexcept;

// Within the warning window and not yet past it. An open-ended agreement never
// reports as expiring, however wide the window.
bool expiring_at_moment(const Agreement& agreement, std::int64_t at,
                        std::int64_t window) noexcept;

// The sum of one line's agreed rate against its cap, checked. Zero for an
// uncapped line, which has no committed value to speak of.
engine::MoneyResult capped_value(const AgreementLine& line) noexcept;

void validate(const Agreement& agreement);
void validate(const AgreementLine& line);

engine::Row to_row(const Agreement& agreement);
engine::Row to_row(const AgreementLine& line);

Agreement agreement_from_row(const engine::Row& row);
AgreementLine line_from_row(const engine::Row& row);

}  // namespace squiflow::modules::agreements
