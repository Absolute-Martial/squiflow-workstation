#pragma once

// Per-organisation credit terms and deterministic hold decisions.
//
// A hold is derived from facts: projected exposure over the limit or positive
// exposure past its due time. It is never a mutable status somebody can forget
// to refresh. Overrides are separate immutable evidence; this domain does not
// grant the missing protocol right or invent a command for it.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/records/reference.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::receivables {

struct CreditAccount {
    std::string id{};
    std::string party_id{};

    // Zero means no amount may be carried on credit, not unlimited credit.
    std::int64_t credit_limit_minor{0};

    // Calendar days after issue. Zero means due on receipt. Cycle day is the
    // nominal day of month, 1..31; presentation/workflow code uses the last
    // day of a shorter month rather than changing this stored agreement.
    std::int32_t credit_period_days{0};
    std::uint8_t cycle_day{1};

    std::int64_t updated_at{0};
    std::string updated_by{};
};

// One positive component of live exposure. It may represent an issued invoice
// or unbilled work. due_at is zero for exposure that is not yet due; only a
// positive amount with a positive due_at can breach the credit period.
struct CreditExposureItem {
    std::string id{};
    std::int64_t amount_minor{0};
    std::int64_t due_at{0};
};

struct CreditEvaluation {
    std::int64_t exposure_minor{0};
    std::int64_t proposed_minor{0};
    std::int64_t projected_minor{0};
    bool over_limit{false};
    bool over_period{false};
    bool hold{false};
    std::int64_t oldest_overdue_at{0};
};

struct CreditEvaluationResult {
    bool ok{false};
    CreditEvaluation value{};
};

// Evaluate current or proposed exposure. proposed_minor may be zero for a live
// account view but never negative. The result fails on malformed inputs or any
// arithmetic overflow; it never wraps into an apparently safe decision.
CreditEvaluationResult evaluate_credit(
    const CreditAccount& account,
    const std::vector<CreditExposureItem>& exposure,
    std::int64_t proposed_minor,
    std::int64_t as_of) noexcept;

struct DueAtResult {
    bool ok{false};
    std::int64_t value{0};
};

// Calculate issue time plus the agreed whole-day period, checked at both the
// day-to-millisecond multiplication and timestamp addition boundaries.
DueAtResult credit_due_at(std::int64_t issued_at,
                          std::int32_t credit_period_days) noexcept;

// Evidence that an authorized workflow accepted one job despite a derived
// hold. This is not the hold itself and cannot make later evaluations safe.
struct CreditOverride {
    std::string id{};
    std::string party_id{};
    engine::Reference target_job{};

    // Snapshot the decision inputs so later term changes cannot rewrite why
    // this exception was authorized.
    std::int64_t exposure_minor{0};
    std::int64_t proposed_minor{0};
    std::int64_t credit_limit_minor{0};
    std::int64_t oldest_overdue_at{0};
    bool over_limit{false};
    bool over_period{false};

    std::string reason{};
    std::int64_t authorized_at{0};
    std::string authorized_by{};
};

void validate(const CreditAccount& account);
void validate(const CreditExposureItem& item);
void validate(const CreditOverride& override_evidence);

engine::Row to_row(const CreditAccount& account);
CreditAccount credit_account_from_row(const engine::Row& row);

engine::Row to_row(const CreditOverride& override_evidence);
CreditOverride credit_override_from_row(const engine::Row& row);

}  // namespace squiflow::modules::receivables
