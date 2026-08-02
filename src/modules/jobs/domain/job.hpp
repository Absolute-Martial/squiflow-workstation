#pragma once

// One unit of work on the machine floor.
//
// A job may exist with no quotation and no order at all, so the upstream links
// are optional identifiers only and do not create a module dependency. A job
// owns its own ticket number and its own production truth. It does not borrow
// invoice lifecycle, because a job is worked, not issued.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/records/money.hpp"
#include "engine/records/quantity.hpp"
#include "engine/records/snapshot.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::jobs {

enum class JobState : std::uint8_t {
    Draft,
    InProgress,
    Done,
    Cancelled,
};

const char* to_string(JobState state) noexcept;
bool transition_allowed(JobState from, JobState to) noexcept;

constexpr bool can_change(JobState state) noexcept {
    return state == JobState::Draft || state == JobState::InProgress;
}

enum class CommercialProgress : std::uint8_t {
    Open,
    Approved,
    Closed,
};

enum class DesignProgress : std::uint8_t {
    NotNeeded,
    Waiting,
    Approved,
};

enum class ProductionProgress : std::uint8_t {
    Queued,
    Running,
    Produced,
};

enum class FulfilmentProgress : std::uint8_t {
    Waiting,
    Ready,
    Delivered,
};

enum class PaymentProgress : std::uint8_t {
    Unbilled,
    Invoiced,
    PartPaid,
    Paid,
};

struct Job {
    std::string id{};
    std::string party_id{};

    // Optional one-way upstream links. These are recorded strings so the job
    // module does not depend on orders or quotations.
    std::string source_order_id{};
    std::string source_quotation_id{};

    JobState state{JobState::Draft};

    // Final ticket identity. Drafts have no number yet; working jobs do.
    std::string ticket_series{};
    std::uint64_t ticket_number{0};

    // A thin job is intentionally incomplete, not silently broken.
    bool thin{false};

    // What is being made, as shown on the ticket.
    std::string title{};
    std::string description{};
    std::string specifications{};
    std::int64_t quantity_scaled{0};

    // The remembered selling snapshot attached to this job. No formula and no
    // backward repricing.
    std::int64_t unit_price_minor{0};
    std::int64_t total_price_minor{0};
    engine::RateOrigin rate_origin{engine::RateOrigin::CatalogDefault};
    std::string rate_reason{};

    std::int64_t promised_at{0};
    std::int64_t deadline_at{0};
    std::string note{};

    // Five independent axes. The top-level state stays small and the axes tell
    // the fuller truth such as "printed, delivered, unpaid".
    CommercialProgress commercial{CommercialProgress::Open};
    DesignProgress design{DesignProgress::Waiting};
    ProductionProgress production{ProductionProgress::Queued};
    FulfilmentProgress fulfilment{FulfilmentProgress::Waiting};
    PaymentProgress payment{PaymentProgress::Unbilled};

    // Optional free-text-level material memory only. Never quantities, stock,
    // or accounting.
    std::string material_reference{};

    // Delivery/pickup evidence.
    std::int64_t delivered_at{0};
    std::string delivered_by{};
    std::string received_by{};
    std::string delivery_signature_ref{};

    // Proof approval and reprint evidence are recorded here but their richer
    // workflows live later.
    std::string proof_approval_ref{};
    std::string reprint_of_job_id{};
    std::string reprint_reason{};

    std::int64_t created_at{0};
    std::string created_by{};
    std::int64_t started_at{0};
    std::string started_by{};
    std::int64_t done_at{0};
    std::string done_by{};
    std::int64_t cancelled_at{0};
    std::string cancelled_by{};
    std::string cancel_reason{};
};

engine::MoneyResult job_total(const Job& job) noexcept;

void validate(const Job& job);

engine::Row to_row(const Job& job);
Job job_from_row(const engine::Row& row);

}  // namespace squiflow::modules::jobs
