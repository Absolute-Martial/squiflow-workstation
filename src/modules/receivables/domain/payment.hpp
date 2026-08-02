#pragma once

// Independent incoming-money evidence and its manual allocations.
//
// A payment exists before, after, and without any invoice. Allocation never
// mutates that payment and never happens automatically. Releasing an allocation
// preserves the original row so a paid cancelled invoice can return the money
// to the customer's visible unallocated balance without inventing a refund.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/records/money.hpp"
#include "engine/records/reference.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::receivables {

struct Payment {
    std::string id{};

    // Empty only for a walk-in counter payment. An advance must name a party
    // so the unallocated money remains visible on that customer's account.
    std::string party_id{};

    // Incoming money only. Zero and negative values are not payments, and a
    // negative value would be a refund disguised as ordinary data.
    std::int64_t amount_minor{0};
    std::int64_t paid_at{0};
    std::string method{};
    std::string external_reference{};
    std::string note{};

    // Optional visible receipt evidence. Most shops identify a payment by its
    // customer, date, amount and method, so neither field is required. When a
    // shop enters a receipt reference, both fields form one coherent pair.
    // external_reference remains the free-form place for a cheque number,
    // bank transaction id, handwritten receipt reference, or similar evidence.
    std::string receipt_series{};
    std::uint64_t receipt_number{0};

    std::int64_t recorded_at{0};
    std::string recorded_by{};
};

enum class AllocationState : std::uint8_t {
    Active,
    Released,
};

struct PaymentAllocation {
    std::string id{};
    std::string payment_id{};

    // Only receivables/invoice and jobs/job references are accepted. The
    // generic reference avoids a forbidden dependency from receivables to the
    // jobs module while keeping the target explicit and typed.
    engine::Reference target{};
    std::int64_t amount_minor{0};

    AllocationState state{AllocationState::Active};
    std::int64_t allocated_at{0};
    std::string allocated_by{};

    // Releasing is the only reversal. It does not send money out and does not
    // erase evidence; it simply makes this amount unallocated again.
    std::int64_t released_at{0};
    std::string released_by{};
    std::string release_reason{};
};

void validate(const Payment& payment);
void validate(const PaymentAllocation& allocation);

// Checked remaining amount. All rows must belong to payment. Released rows do
// not consume money. Returns ok=false for malformed rows, overflow, or any
// allocation total greater than the payment amount.
engine::MoneyResult unallocated_amount(
    const Payment& payment,
    const std::vector<PaymentAllocation>& allocations) noexcept;

engine::Row to_row(const Payment& payment);
Payment payment_from_row(const engine::Row& row);

engine::Row to_row(const PaymentAllocation& allocation);
PaymentAllocation payment_allocation_from_row(const engine::Row& row);

}  // namespace squiflow::modules::receivables
