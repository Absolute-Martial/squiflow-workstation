#include "modules/receivables/domain/payment.hpp"

#include <limits>

#include <squiflow/protocol/module_id.hpp>

#include "modules/context.hpp"

namespace squiflow::modules::receivables {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

protocol::ModuleId module_from(std::int64_t stored) noexcept {
    if (stored < 0 ||
        stored > static_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max())) {
        return protocol::ModuleId::Count;
    }
    protocol::ModuleId module = protocol::ModuleId::Count;
    if (!protocol::module_from_number(static_cast<std::uint32_t>(stored), module)) {
        return protocol::ModuleId::Count;
    }
    return module;
}

AllocationState allocation_state_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return AllocationState::Active;
        case 1: return AllocationState::Released;
        default:
            // Preserve invalidity. Mapping unknown data to Released would make
            // its money appear available for a second allocation.
            return static_cast<AllocationState>(
                std::numeric_limits<std::uint8_t>::max());
    }
}

std::uint64_t receipt_number_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("receipt_number").integer_or(0);
    return stored > 0 ? static_cast<std::uint64_t>(stored) : 0;
}

bool valid_target_module(protocol::ModuleId module) noexcept {
    return module == protocol::ModuleId::receivables ||
           module == protocol::ModuleId::jobs;
}

}  // namespace

void validate(const Payment& payment) {
    if (blank(payment.id)) {
        throw RuleViolation("This payment has no record to be saved under.");
    }
    if (!payment.party_id.empty() && blank(payment.party_id)) {
        throw RuleViolation("A payment customer cannot be only whitespace.");
    }
    if (payment.amount_minor <= 0) {
        throw RuleViolation("A payment must be incoming money greater than zero.");
    }
    if (payment.paid_at <= 0 || payment.recorded_at <= 0) {
        throw RuleViolation("A payment must record when it was paid and entered.");
    }
    if (payment.recorded_at < payment.paid_at) {
        throw RuleViolation("A payment cannot be entered before it was paid.");
    }
    if (blank(payment.method)) {
        throw RuleViolation("A payment must record its method.");
    }
    if (blank(payment.receipt_series) || payment.receipt_number == 0 ||
        payment.receipt_number > static_cast<std::uint64_t>(
                                     std::numeric_limits<std::int64_t>::max())) {
        throw RuleViolation("A payment must carry a usable final receipt number.");
    }
    if (blank(payment.recorded_by)) {
        throw RuleViolation("A payment must record who entered it.");
    }
}

void validate(const PaymentAllocation& allocation) {
    if (blank(allocation.id)) {
        throw RuleViolation("This allocation has no record to be saved under.");
    }
    if (blank(allocation.payment_id)) {
        throw RuleViolation("An allocation must belong to a payment.");
    }
    if (!protocol::is_valid(allocation.target.module) ||
        !valid_target_module(allocation.target.module) ||
        !allocation.target.record.is_valid()) {
        throw RuleViolation("An allocation must name an invoice or job.");
    }
    if (allocation.amount_minor <= 0) {
        throw RuleViolation("An allocation must apply an amount greater than zero.");
    }
    if (allocation.allocated_at <= 0 || blank(allocation.allocated_by)) {
        throw RuleViolation("An allocation must record when and by whom it was made.");
    }

    switch (allocation.state) {
        case AllocationState::Active:
            if (allocation.released_at != 0 || !blank(allocation.released_by) ||
                !blank(allocation.release_reason)) {
                throw RuleViolation("An active allocation cannot carry release evidence.");
            }
            break;

        case AllocationState::Released:
            if (allocation.released_at <= 0 || blank(allocation.released_by) ||
                blank(allocation.release_reason)) {
                throw RuleViolation(
                    "A released allocation must record when, by whom, and why it was released.");
            }
            if (allocation.released_at < allocation.allocated_at) {
                throw RuleViolation("An allocation cannot be released before it was made.");
            }
            break;

        default:
            throw RuleViolation("That allocation has a state this build does not understand.");
    }
}

engine::MoneyResult unallocated_amount(
    const Payment& payment,
    const std::vector<PaymentAllocation>& allocations) noexcept {
    try {
        validate(payment);
    } catch (...) {
        return {false, {}};
    }

    engine::Money allocated{0};
    for (const PaymentAllocation& allocation : allocations) {
        try {
            validate(allocation);
        } catch (...) {
            return {false, {}};
        }
        if (allocation.payment_id != payment.id) {
            return {false, {}};
        }
        if (allocation.state == AllocationState::Released) {
            continue;
        }
        const engine::MoneyResult sum = engine::money_add(
            allocated, engine::Money{allocation.amount_minor});
        if (!sum.ok) {
            return {false, {}};
        }
        allocated = sum.value;
    }

    const engine::MoneyResult remaining = engine::money_subtract(
        engine::Money{payment.amount_minor}, allocated);
    if (!remaining.ok || remaining.value.is_negative()) {
        return {false, {}};
    }
    return remaining;
}

engine::Row to_row(const Payment& payment) {
    engine::Row row;
    row.set("id", engine::Value::text(payment.id));
    row.set("party_id", engine::Value::text(payment.party_id));
    row.set("amount_minor", engine::Value::integer(payment.amount_minor));
    row.set("paid_at", engine::Value::integer(payment.paid_at));
    row.set("method", engine::Value::text(payment.method));
    row.set("external_reference", engine::Value::text(payment.external_reference));
    row.set("note", engine::Value::text(payment.note));
    row.set("receipt_series", engine::Value::text(payment.receipt_series));
    row.set("receipt_number",
            engine::Value::integer(static_cast<std::int64_t>(payment.receipt_number)));
    row.set("recorded_at", engine::Value::integer(payment.recorded_at));
    row.set("recorded_by", engine::Value::text(payment.recorded_by));
    return row;
}

Payment payment_from_row(const engine::Row& row) {
    Payment payment;
    payment.id = row.get("id").text_or({});
    payment.party_id = row.get("party_id").text_or({});
    payment.amount_minor = row.get("amount_minor").integer_or(0);
    payment.paid_at = row.get("paid_at").integer_or(0);
    payment.method = row.get("method").text_or({});
    payment.external_reference = row.get("external_reference").text_or({});
    payment.note = row.get("note").text_or({});
    payment.receipt_series = row.get("receipt_series").text_or({});
    payment.receipt_number = receipt_number_from(row);
    payment.recorded_at = row.get("recorded_at").integer_or(0);
    payment.recorded_by = row.get("recorded_by").text_or({});
    return payment;
}

engine::Row to_row(const PaymentAllocation& allocation) {
    engine::Row row;
    row.set("id", engine::Value::text(allocation.id));
    row.set("payment_id", engine::Value::text(allocation.payment_id));
    row.set("target_module",
            engine::Value::integer(static_cast<std::int64_t>(allocation.target.module)));
    row.set("target_record_id",
            engine::Value::text(engine::to_string(allocation.target.record)));
    row.set("amount_minor", engine::Value::integer(allocation.amount_minor));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(allocation.state)));
    row.set("allocated_at", engine::Value::integer(allocation.allocated_at));
    row.set("allocated_by", engine::Value::text(allocation.allocated_by));
    row.set("released_at", engine::Value::integer(allocation.released_at));
    row.set("released_by", engine::Value::text(allocation.released_by));
    row.set("release_reason", engine::Value::text(allocation.release_reason));
    return row;
}

PaymentAllocation payment_allocation_from_row(const engine::Row& row) {
    PaymentAllocation allocation;
    allocation.id = row.get("id").text_or({});
    allocation.payment_id = row.get("payment_id").text_or({});
    allocation.target.module = module_from(row.get("target_module").integer_or(-1));
    allocation.target.record = engine::record_id_from_string(
        row.get("target_record_id").text_or({}));
    allocation.amount_minor = row.get("amount_minor").integer_or(0);
    allocation.state = allocation_state_from(row.get("state").integer_or(-1));
    allocation.allocated_at = row.get("allocated_at").integer_or(0);
    allocation.allocated_by = row.get("allocated_by").text_or({});
    allocation.released_at = row.get("released_at").integer_or(0);
    allocation.released_by = row.get("released_by").text_or({});
    allocation.release_reason = row.get("release_reason").text_or({});
    return allocation;
}

}  // namespace squiflow::modules::receivables
