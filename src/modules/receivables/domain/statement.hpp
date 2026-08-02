#pragma once

// Deterministic customer statements and oldest-first aging read models.
//
// Statement money is represented as positive facts. There are no negative
// charge rows, refund rows, or credit-note rows. Cancellation and allocation
// are named events, so the customer-facing trail remains honest and the no-
// refund boundary remains enforceable.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"

namespace squiflow::modules::receivables {

// Numeric order is also same-instant application order: money and charges must
// exist before they are allocated or cancelled, and a released allocation must
// restore balances before cancellation removes the invoice.
enum class StatementEntryKind : std::uint8_t {
    InvoiceCharged,
    PaymentReceived,
    AllocationReleased,
    PaymentAllocated,
    InvoiceCancelled,
};

struct StatementEntry {
    std::string id{};
    std::string statement_id{};
    StatementEntryKind kind{StatementEntryKind::InvoiceCharged};
    std::string source_id{};
    std::int64_t occurred_at{0};
    std::string reference{};
    std::string description{};
    std::int64_t amount_minor{0};
};

struct StatementTotals {
    std::int64_t opening_outstanding_minor{0};
    std::int64_t opening_unallocated_minor{0};
    std::int64_t charged_minor{0};
    std::int64_t cancelled_minor{0};
    std::int64_t paid_minor{0};
    std::int64_t allocated_minor{0};
    std::int64_t released_minor{0};
    std::int64_t outstanding_minor{0};
    std::int64_t unallocated_minor{0};
};

struct Statement {
    std::string id{};
    std::string party_id{};
    std::int64_t period_from{0};
    std::int64_t period_through{0};
    std::int64_t prepared_at{0};
    std::string prepared_by{};
    StatementTotals totals{};
    std::vector<StatementEntry> entries{};
};

struct StatementResult {
    bool ok{false};
    Statement value{};
};

// Validates, sorts, and computes every total from the opening balances and
// positive event rows. Caller-supplied computed totals are ignored. Any
// overflow, underflow, wrong period, or impossible event sequence fails.
StatementResult prepare_statement(Statement source) noexcept;

void validate(const Statement& statement);
void validate(const StatementEntry& entry);

// Evidence created only after an online transport confirms the statement was
// accepted. Preparing or printing a local read model must never create this.
struct StatementDelivery {
    std::string id{};
    std::string statement_id{};
    std::string recipient{};
    std::string content_hash{};
    std::string transport_reference{};
    std::int64_t sent_at{0};
    std::string sent_by{};
};

void validate(const StatementDelivery& delivery);

struct AgingItem {
    std::string invoice_id{};
    std::string party_id{};
    std::string invoice_reference{};
    std::int64_t issued_at{0};
    std::int64_t due_at{0};
    std::int64_t outstanding_minor{0};
};

struct AgingRow : AgingItem {
    std::int64_t age_days{0};
};

struct AgingResult {
    bool ok{false};
    std::int64_t total_outstanding_minor{0};
    std::vector<AgingRow> rows{};
};

// Positive exposure only, sorted oldest first. A due time is the age anchor;
// when no due time was promised, issue time is used. Same-age rows tie-break
// by invoice id, making repeated reads deterministic.
AgingResult prepare_aging(std::vector<AgingItem> items,
                          std::int64_t as_of) noexcept;

void validate(const AgingItem& item);

engine::Row to_row(const Statement& statement);
Statement statement_from_row(const engine::Row& row);

engine::Row to_row(const StatementEntry& entry);
StatementEntry statement_entry_from_row(const engine::Row& row);

engine::Row to_row(const StatementDelivery& delivery);
StatementDelivery statement_delivery_from_row(const engine::Row& row);

}  // namespace squiflow::modules::receivables
