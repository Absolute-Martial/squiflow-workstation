#pragma once

// The pricing module is the sole owner of rate resolution. Every other module
// that needs to know what a product costs asks here and gets one answer. A
// rate is a relation between a product, an optional party and an optional time
// window -- not an attribute of a product. That is why this module exists and
// why catalog stores no price.
//
// Resolution order, most specific first:
//   1. A rate naming this party whose window covers the moment of issue.
//   2. A catch-all rate (empty party_id) whose window covers the moment.
//   3. The product's default rate, which has no party and no time bounds.
//   4. Nothing. The caller is told nothing was found rather than being handed
//      a zero it might mistake for a free item.
//
// A one-time override can replace whichever rate would have applied, for a
// single record. Overrides require a reason, because a price deviation that
// cannot be explained cannot be audited.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"

namespace squiflow::modules::pricing {

// A sanity ceiling on any stored amount, in minor units. This is not a
// business limit; it is a garbage filter. A misplaced decimal point or a wire
// value from a corrupt payload arrives as an absurd number, and an absurd
// number that reaches an invoice is worse than a refusal. Ten to the fifteenth
// minor units is far above any real price and far below the point where adding
// amounts together can overflow a signed 64-bit integer.
inline constexpr std::int64_t kMaxAmountMinor = 1000000000000000;

// A rate record.
//
// product_id is required. party_id empty means "any customer".
// valid_from == 0 means "has always applied"; valid_until == 0 means "no
// expiry". The window is half-open: valid_from is included, valid_until is
// not, so two consecutive windows that share a boundary cannot both match the
// same instant.
// Amounts are in minor currency units (paisa in NPR).
struct Rate {
    std::string id{};
    std::string product_id{};
    std::string party_id{};
    std::int64_t amount_minor{0};
    std::int64_t valid_from{0};
    std::int64_t valid_until{0};
    std::int64_t created_at{0};
    std::string created_by{};
};

// The fallback rate for a product when no time-bounded rate matches. Keyed by
// product_id, so there is at most one default per product and no resolution
// question about which default applies.
struct DefaultRate {
    std::string product_id{};
    std::int64_t amount_minor{0};
    std::int64_t updated_at{0};
    std::string updated_by{};
};

// A one-time override applied to one specific line: an order line, an invoice
// line.
//
// line_id is that line's id. It is deliberately not called record_id, because
// Call::record_id already means "the row this request creates or changes", and
// for this operation that row is the override itself. Two different ids under
// one name in the same handler is how the wrong one gets written.
//
// Pricing does not know or care which module owns the line, which is what
// keeps this module from depending on orders.
struct RateOverride {
    std::string id{};
    std::string line_id{};
    std::int64_t overridden_minor{0};
    std::string reason{};
    std::int64_t applied_at{0};
    std::string applied_by{};
};

// Where a resolved price came from. Returned alongside the amount because
// "500, because this customer has an agreed rate" and "500, because that is
// the standard price" are different answers to the person at the counter, and
// only one of them is safe to change without a conversation.
enum class RateSource : std::uint8_t {
    None,
    PartyRate,
    CatchAllRate,
    Default,
};

const char* to_string(RateSource source) noexcept;

struct ResolvedRate {
    RateSource source{RateSource::None};
    std::int64_t amount_minor{0};

    // The id of the rate row that won, for a PartyRate or CatchAllRate.
    // Empty for Default and None, which have no rate row.
    std::string rate_id{};

    bool found() const noexcept { return source != RateSource::None; }
};

// True when the moment falls inside the rate's window. Half-open, as above.
bool covers(const Rate& rate, std::int64_t at) noexcept;

// Pick the winning rate from a set of candidates, without a database.
//
// Kept pure and separate from the repository on purpose: resolution is the one
// piece of this module with real decisions in it, and a decision that can only
// be tested through a store is a decision that will be tested lightly. The
// candidates need not be sorted, need not all be for the same product, and may
// contain rates that do not apply; anything that does not match is ignored.
//
// When two candidates of the same specificity both cover the moment the data
// is already questionable, but the answer must still not depend on the order
// rows came back from a table. The later valid_from wins, then the later
// created_at, then the greater id. Fully determined, and biased towards the
// most recently effective rate.
// Not noexcept: the winner's id is copied into the result, which allocates.
ResolvedRate choose_rate(const std::vector<Rate>& candidates,
                         const std::string& party_id,
                         std::int64_t at);

// Fold the product's default in behind a chooser result. Separate from
// choose_rate because the default lives in its own table and is not a Rate.
ResolvedRate with_default(const ResolvedRate& chosen, const DefaultRate& fallback);

// Validation: throws RuleViolation on the first problem found.
void validate(const Rate& rate);
void validate(const DefaultRate& rate);
void validate(const RateOverride& override_);

// Row mapping.
engine::Row to_row(const Rate& rate);
Rate rate_from_row(const engine::Row& row);

engine::Row to_row(const DefaultRate& rate);
DefaultRate default_rate_from_row(const engine::Row& row);

engine::Row to_row(const RateOverride& override_);
RateOverride override_from_row(const engine::Row& row);

}  // namespace squiflow::modules::pricing
