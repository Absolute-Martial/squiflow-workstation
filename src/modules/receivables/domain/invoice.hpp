#pragma once

// Invoice evidence and its editable draft lines.
//
// The state is the shared document state used by quotations and agreements.
// Drafts are freely editable and unnumbered. Issuing, cancelling and replacing
// are cross-module workflows; this domain owns the evidence those workflows
// must leave behind. There is no refund or credit-note state.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/records/lifecycle.hpp"
#include "engine/records/money.hpp"
#include "engine/records/quantity.hpp"
#include "engine/records/snapshot.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::receivables {

struct Invoice {
    std::string id{};
    std::string party_id{};  // Empty is a walk-in; credit and statements require a party.
    engine::DocumentState state{engine::DocumentState::Draft};

    // A draft has no final number. Issuance fills both together and neither is
    // ever reused, even when the invoice is cancelled.
    std::string number_series{};
    std::uint64_t number{0};

    // Zero means no due date was promised. An issued credit invoice normally
    // has one, but cash invoices and drafts need not.
    std::int64_t due_at{0};
    std::string note{};

    std::int64_t created_at{0};
    std::string created_by{};

    std::int64_t issued_at{0};
    std::string issued_by{};

    std::int64_t cancelled_at{0};
    std::string cancelled_by{};
    std::string cancel_reason{};

    std::int64_t discarded_at{0};
    std::string discarded_by{};

    // A replacement pair is permanent and two-way: the new draft names the
    // cancelled source, and the source names the replacement once issued.
    std::string replaces_invoice_id{};
    std::string replacement_invoice_id{};
};

struct InvoiceLine {
    std::string id{};
    std::string invoice_id{};
    std::int64_t position{0};

    // Empty product_id means off-catalog. Description is copied as it will be
    // printed so a later catalog rename cannot alter historical evidence.
    std::string product_id{};
    std::string description{};

    std::int64_t quantity_scaled{0};
    std::int64_t rate_minor{0};
    std::int64_t amount_minor{0};
    engine::RateOrigin rate_origin{engine::RateOrigin::CatalogDefault};
    std::string rate_reason{};

    // Structured agreement provenance. All empty/zero for ordinary rates;
    // all populated together for an agreement-backed line.
    std::string agreement_id{};
    std::string agreement_line_id{};
    std::int64_t agreement_rate_minor{0};
    std::int64_t agreement_quantity_scaled{0};

    std::int64_t added_at{0};
    std::string added_by{};
};

// Recompute one line from rate and quantity using the shared checked rounding
// rule. A stored amount is accepted only when it equals this result exactly.
engine::MoneyResult calculate_amount(const InvoiceLine& line) noexcept;

// Checked sum. An empty draft totals zero. Overflow is a refusal, never a
// wrapped invoice amount.
engine::MoneyResult invoice_total(const std::vector<InvoiceLine>& lines) noexcept;

// Throws RuleViolation on the first invalid or contradictory fact.
void validate(const Invoice& invoice);
void validate(const InvoiceLine& line);

// Row mapping. Unknown stored document states fail closed as Cancelled, and
// unknown rate origins become ManualOverride so they never look ordinary.
engine::Row to_row(const Invoice& invoice);
Invoice invoice_from_row(const engine::Row& row);

engine::Row to_row(const InvoiceLine& line);
InvoiceLine invoice_line_from_row(const engine::Row& row);

}  // namespace squiflow::modules::receivables
