#pragma once

// A priced offer to a customer, and the stack of revisions behind it.
//
// Three records rather than one, and the reason is a single rule from the
// specification: the accepted revision is pinned exactly and never re-reads
// current prices. If lines hung off the quotation, revising it would rewrite
// the lines the customer already holds on paper. So lines belong to a
// revision, a revision is frozen the moment it is issued, and revisions stack
// rather than overwrite.
//
// The head record carries only what is true of the whole conversation: who it
// is for, where it has got to, and which revision - if any - was accepted.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/records/money.hpp"
#include "engine/records/quantity.hpp"
#include "engine/records/snapshot.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::quotations {

// Four states, and deliberately only four. The written lifecycle also names
// "declined", but the protocol declares no operation that could reach it, and
// a state no operation can produce is a state no screen can ever show.
enum class QuotationState : std::uint8_t {
    Draft,
    Issued,
    Accepted,
    Expired,
};

const char* to_string(QuotationState state) noexcept;
bool transition_allowed(QuotationState from, QuotationState to) noexcept;

// A quotation may be revised while it is still a live offer. Once accepted it
// is settled, and once expired it needs a deliberate act before anything else
// happens to it.
constexpr bool can_revise(QuotationState state) noexcept {
    return state == QuotationState::Draft || state == QuotationState::Issued;
}

struct Quotation {
    std::string id{};

    // Empty is legitimate: a counter enquiry priced for someone who has not
    // become a customer record yet.
    std::string party_id{};

    QuotationState state{QuotationState::Draft};

    // Which revision is the live one. Always at least 1: creating a quotation
    // creates its first revision in the same breath.
    std::int64_t current_revision{1};

    // Which revision the customer actually accepted. Zero until acceptance,
    // and never changed afterwards.
    std::int64_t accepted_revision{0};

    std::string customer_reference{};
    std::string note{};

    std::int64_t created_at{0};
    std::string created_by{};
    std::int64_t accepted_at{0};
    std::string accepted_by{};
    std::int64_t expired_at{0};
    std::string expired_by{};
    std::string expiry_reason{};
};

// One version of the offer. Immutable from the moment it is issued.
struct QuotationRevision {
    std::string id{};
    std::string quotation_id{};

    // 1, 2, 3 ... unique within one quotation.
    std::int64_t revision{1};

    // Issuing is the explicit act that locks this revision and gives it its
    // number. A revision with no number has never left the building.
    bool issued{false};
    std::string series{};
    std::uint64_t number{0};

    // Zero means the offer carries no expiry date at all.
    std::int64_t valid_until{0};

    std::string terms{};

    // The sum of this revision's lines, computed once when the lines change
    // and stored, so a printed total never disagrees with a recomputed one.
    std::int64_t total_minor{0};

    std::int64_t created_at{0};
    std::string created_by{};
    std::int64_t issued_at{0};
    std::string issued_by{};
};

// A priced line on one revision.
struct QuotationLine {
    std::string id{};
    std::string revision_id{};
    std::string quotation_id{};

    // Position on the printed page. Ties are broken by id so that two devices
    // print the same order.
    std::int64_t position{0};

    // Empty for an off-catalog line: a description and a price, with no
    // catalog entry required.
    std::string product_id{};

    std::string description{};
    std::string specifications{};

    std::int64_t quantity_scaled{0};
    std::int64_t unit_price_minor{0};
    std::int64_t amount_minor{0};

    engine::RateOrigin rate_origin{engine::RateOrigin::CatalogDefault};
    std::string rate_reason{};
};

// quantity x unit price, checked. Never computed in the caller.
engine::MoneyResult line_amount(const QuotationLine& line) noexcept;

// The sum of the lines given, checked at every step.
engine::MoneyResult revision_total(const std::vector<QuotationLine>& lines) noexcept;

// True once the moment given is past the revision's validity date. A revision
// with no date never expires by time alone.
bool expired_at_moment(const QuotationRevision& revision, std::int64_t at) noexcept;

void validate(const Quotation& quotation);
void validate(const QuotationRevision& revision);
void validate(const QuotationLine& line);

engine::Row to_row(const Quotation& quotation);
engine::Row to_row(const QuotationRevision& revision);
engine::Row to_row(const QuotationLine& line);

Quotation quotation_from_row(const engine::Row& row);
QuotationRevision revision_from_row(const engine::Row& row);
QuotationLine line_from_row(const engine::Row& row);

}  // namespace squiflow::modules::quotations
