#include "modules/receivables/domain/invoice.hpp"

#include <limits>

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

bool set(std::int64_t moment, const std::string& person) noexcept {
    return moment > 0 && !blank(person);
}

bool clear(std::int64_t moment, const std::string& person) noexcept {
    return moment == 0 && blank(person);
}

engine::DocumentState document_state_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return engine::DocumentState::Draft;
        case 1: return engine::DocumentState::Issued;
        case 2: return engine::DocumentState::Cancelled;
        case 3: return engine::DocumentState::Replaced;
        case 4: return engine::DocumentState::Discarded;
        default:
            // Only Draft is editable. Unknown data from a newer build or a
            // damaged row must therefore land on a locked state.
            return engine::DocumentState::Cancelled;
    }
}

engine::RateOrigin rate_origin_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return engine::RateOrigin::CatalogDefault;
        case 1: return engine::RateOrigin::PartySpecific;
        case 2: return engine::RateOrigin::Agreement;
        case 3: return engine::RateOrigin::ManualOverride;
        case 4: return engine::RateOrigin::OffCatalog;
        default:
            // Unknown provenance must never look like an ordinary catalog
            // price. ManualOverride is the most cautious representation and
            // validation will require its reason.
            return engine::RateOrigin::ManualOverride;
    }
}

std::uint64_t number_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("number").integer_or(0);
    return stored > 0 ? static_cast<std::uint64_t>(stored) : 0;
}

void require_numbered(const Invoice& invoice) {
    if (blank(invoice.number_series) || invoice.number == 0 ||
        invoice.number > static_cast<std::uint64_t>(
                             std::numeric_limits<std::int64_t>::max())) {
        throw RuleViolation("An issued invoice must have a usable final number.");
    }
    if (!set(invoice.issued_at, invoice.issued_by)) {
        throw RuleViolation("An issued invoice must record when and by whom it was issued.");
    }
}

void require_not_numbered(const Invoice& invoice) {
    if (!blank(invoice.number_series) || invoice.number != 0 ||
        !clear(invoice.issued_at, invoice.issued_by)) {
        throw RuleViolation("A draft or discarded invoice cannot carry issue evidence.");
    }
}

void require_no_cancellation(const Invoice& invoice) {
    if (!clear(invoice.cancelled_at, invoice.cancelled_by) ||
        !blank(invoice.cancel_reason)) {
        throw RuleViolation("That invoice carries cancellation evidence in the wrong state.");
    }
}

void require_cancellation(const Invoice& invoice) {
    if (!set(invoice.cancelled_at, invoice.cancelled_by) ||
        blank(invoice.cancel_reason)) {
        throw RuleViolation(
            "A cancelled invoice must record when, by whom, and why it was cancelled.");
    }
}

void require_no_discard(const Invoice& invoice) {
    if (!clear(invoice.discarded_at, invoice.discarded_by)) {
        throw RuleViolation("An issued invoice cannot carry draft-discard evidence.");
    }
}

}  // namespace

engine::MoneyResult calculate_amount(const InvoiceLine& line) noexcept {
    return engine::money_multiply(engine::Money{line.rate_minor},
                                  engine::Quantity{line.quantity_scaled});
}

engine::MoneyResult invoice_total(const std::vector<InvoiceLine>& lines) noexcept {
    engine::Money running{0};
    for (const InvoiceLine& line : lines) {
        const engine::MoneyResult amount = calculate_amount(line);
        if (!amount.ok || amount.value.minor != line.amount_minor) {
            return {false, {}};
        }
        const engine::MoneyResult sum = engine::money_add(running, amount.value);
        if (!sum.ok) {
            return {false, {}};
        }
        running = sum.value;
    }
    return {true, running};
}

void validate(const Invoice& invoice) {
    if (invoice.id.empty()) {
        throw RuleViolation("This invoice has no record to be saved under.");
    }
    if (invoice.created_at <= 0 || blank(invoice.created_by)) {
        throw RuleViolation("An invoice must record when and by whom it was created.");
    }
    if (invoice.due_at < 0) {
        throw RuleViolation("An invoice due date cannot be before this shop existed.");
    }

    switch (invoice.state) {
        case engine::DocumentState::Draft:
            require_not_numbered(invoice);
            require_no_cancellation(invoice);
            require_no_discard(invoice);
            if (!invoice.replacement_invoice_id.empty()) {
                throw RuleViolation("A draft cannot already name the invoice that replaced it.");
            }
            break;

        case engine::DocumentState::Issued:
            require_numbered(invoice);
            require_no_cancellation(invoice);
            require_no_discard(invoice);
            if (!invoice.replacement_invoice_id.empty()) {
                throw RuleViolation("An issued invoice cannot already be marked as replaced.");
            }
            break;

        case engine::DocumentState::Cancelled:
            require_numbered(invoice);
            require_cancellation(invoice);
            require_no_discard(invoice);
            if (!invoice.replacement_invoice_id.empty()) {
                throw RuleViolation(
                    "A cancelled invoice with a completed replacement must be marked replaced.");
            }
            break;

        case engine::DocumentState::Replaced:
            require_numbered(invoice);
            require_cancellation(invoice);
            require_no_discard(invoice);
            if (invoice.replacement_invoice_id.empty()) {
                throw RuleViolation("A replaced invoice must name its replacement.");
            }
            break;

        case engine::DocumentState::Discarded:
            require_not_numbered(invoice);
            require_no_cancellation(invoice);
            if (!set(invoice.discarded_at, invoice.discarded_by)) {
                throw RuleViolation(
                    "A discarded draft must record when and by whom it was discarded.");
            }
            if (!invoice.replacement_invoice_id.empty()) {
                throw RuleViolation("A discarded draft cannot be marked as replaced.");
            }
            break;

        default:
            throw RuleViolation("That invoice has a state this build does not understand.");
    }
}

void validate(const InvoiceLine& line) {
    if (line.id.empty()) {
        throw RuleViolation("This invoice line has no record to be saved under.");
    }
    if (line.invoice_id.empty()) {
        throw RuleViolation("An invoice line must belong to an invoice.");
    }
    if (line.position < 0 ||
        line.position == std::numeric_limits<std::int64_t>::max()) {
        throw RuleViolation("That invoice line has no usable position.");
    }
    if (blank(line.description)) {
        throw RuleViolation("Every invoice line must preserve its printed description.");
    }
    if (line.quantity_scaled <= 0) {
        throw RuleViolation("An invoice line must have a quantity greater than zero.");
    }
    if (line.rate_minor < 0 || line.amount_minor < 0) {
        throw RuleViolation("An invoice line cannot carry negative money.");
    }
    if (line.added_at <= 0 || blank(line.added_by)) {
        throw RuleViolation("An invoice line must record when and by whom it was added.");
    }

    switch (line.rate_origin) {
        case engine::RateOrigin::CatalogDefault:
        case engine::RateOrigin::PartySpecific:
        case engine::RateOrigin::Agreement:
        case engine::RateOrigin::ManualOverride:
        case engine::RateOrigin::OffCatalog:
            break;
        default:
            throw RuleViolation("That invoice line has an unknown price origin.");
    }
    if ((line.rate_origin == engine::RateOrigin::Agreement ||
         line.rate_origin == engine::RateOrigin::ManualOverride) &&
        blank(line.rate_reason)) {
        throw RuleViolation("An agreed or manually changed price must keep its reason.");
    }
    const bool agreement_origin = line.rate_origin == engine::RateOrigin::Agreement;
    const bool has_agreement = !blank(line.agreement_id) &&
                               !blank(line.agreement_line_id);
    if (agreement_origin != has_agreement) {
        throw RuleViolation("Agreement-priced invoice lines need exact agreement provenance.");
    }
    if (agreement_origin &&
        (line.agreement_rate_minor != line.rate_minor ||
         line.agreement_quantity_scaled != line.quantity_scaled)) {
        throw RuleViolation("Agreement provenance must match the frozen invoice quantity and rate.");
    }
    if (!agreement_origin && (!line.agreement_id.empty() ||
        !line.agreement_line_id.empty() || line.agreement_rate_minor != 0 ||
        line.agreement_quantity_scaled != 0)) {
        throw RuleViolation("Ordinary invoice rates cannot carry agreement provenance.");
    }

    const engine::MoneyResult amount = calculate_amount(line);
    if (!amount.ok) {
        throw RuleViolation("That quantity and rate multiply to more than this shop can record.");
    }
    if (amount.value.minor != line.amount_minor) {
        throw RuleViolation("That invoice line amount does not match its quantity and rate.");
    }
}

engine::Row to_row(const Invoice& invoice) {
    engine::Row row;
    row.set("id", engine::Value::text(invoice.id));
    row.set("party_id", engine::Value::text(invoice.party_id));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(invoice.state)));
    row.set("number_series", engine::Value::text(invoice.number_series));
    row.set("number", engine::Value::integer(static_cast<std::int64_t>(invoice.number)));
    row.set("due_at", engine::Value::integer(invoice.due_at));
    row.set("note", engine::Value::text(invoice.note));
    row.set("created_at", engine::Value::integer(invoice.created_at));
    row.set("created_by", engine::Value::text(invoice.created_by));
    row.set("issued_at", engine::Value::integer(invoice.issued_at));
    row.set("issued_by", engine::Value::text(invoice.issued_by));
    row.set("cancelled_at", engine::Value::integer(invoice.cancelled_at));
    row.set("cancelled_by", engine::Value::text(invoice.cancelled_by));
    row.set("cancel_reason", engine::Value::text(invoice.cancel_reason));
    row.set("discarded_at", engine::Value::integer(invoice.discarded_at));
    row.set("discarded_by", engine::Value::text(invoice.discarded_by));
    row.set("replaces_invoice_id", engine::Value::text(invoice.replaces_invoice_id));
    row.set("replacement_invoice_id", engine::Value::text(invoice.replacement_invoice_id));
    return row;
}

Invoice invoice_from_row(const engine::Row& row) {
    Invoice invoice;
    invoice.id = row.get("id").text_or({});
    invoice.party_id = row.get("party_id").text_or({});
    invoice.state = document_state_from(row.get("state").integer_or(2));
    invoice.number_series = row.get("number_series").text_or({});
    invoice.number = number_from(row);
    invoice.due_at = row.get("due_at").integer_or(0);
    invoice.note = row.get("note").text_or({});
    invoice.created_at = row.get("created_at").integer_or(0);
    invoice.created_by = row.get("created_by").text_or({});
    invoice.issued_at = row.get("issued_at").integer_or(0);
    invoice.issued_by = row.get("issued_by").text_or({});
    invoice.cancelled_at = row.get("cancelled_at").integer_or(0);
    invoice.cancelled_by = row.get("cancelled_by").text_or({});
    invoice.cancel_reason = row.get("cancel_reason").text_or({});
    invoice.discarded_at = row.get("discarded_at").integer_or(0);
    invoice.discarded_by = row.get("discarded_by").text_or({});
    invoice.replaces_invoice_id = row.get("replaces_invoice_id").text_or({});
    invoice.replacement_invoice_id = row.get("replacement_invoice_id").text_or({});
    return invoice;
}

engine::Row to_row(const InvoiceLine& line) {
    engine::Row row;
    row.set("id", engine::Value::text(line.id));
    row.set("invoice_id", engine::Value::text(line.invoice_id));
    row.set("position", engine::Value::integer(line.position));
    row.set("product_id", engine::Value::text(line.product_id));
    row.set("description", engine::Value::text(line.description));
    row.set("quantity_scaled", engine::Value::integer(line.quantity_scaled));
    row.set("rate_minor", engine::Value::integer(line.rate_minor));
    row.set("amount_minor", engine::Value::integer(line.amount_minor));
    row.set("rate_origin", engine::Value::integer(static_cast<std::int64_t>(line.rate_origin)));
    row.set("rate_reason", engine::Value::text(line.rate_reason));
    row.set("agreement_id", engine::Value::text(line.agreement_id));
    row.set("agreement_line_id", engine::Value::text(line.agreement_line_id));
    row.set("agreement_rate_minor", engine::Value::integer(line.agreement_rate_minor));
    row.set("agreement_quantity_scaled", engine::Value::integer(line.agreement_quantity_scaled));
    row.set("added_at", engine::Value::integer(line.added_at));
    row.set("added_by", engine::Value::text(line.added_by));
    return row;
}

InvoiceLine invoice_line_from_row(const engine::Row& row) {
    InvoiceLine line;
    line.id = row.get("id").text_or({});
    line.invoice_id = row.get("invoice_id").text_or({});
    line.position = row.get("position").integer_or(0);
    line.product_id = row.get("product_id").text_or({});
    line.description = row.get("description").text_or({});
    line.quantity_scaled = row.get("quantity_scaled").integer_or(0);
    line.rate_minor = row.get("rate_minor").integer_or(0);
    line.amount_minor = row.get("amount_minor").integer_or(0);
    line.rate_origin = rate_origin_from(row.get("rate_origin").integer_or(3));
    line.rate_reason = row.get("rate_reason").text_or({});
    line.agreement_id = row.get("agreement_id").text_or({});
    line.agreement_line_id = row.get("agreement_line_id").text_or({});
    line.agreement_rate_minor = row.get("agreement_rate_minor").integer_or(0);
    line.agreement_quantity_scaled = row.get("agreement_quantity_scaled").integer_or(0);
    line.added_at = row.get("added_at").integer_or(0);
    line.added_by = row.get("added_by").text_or({});
    return line;
}

}  // namespace squiflow::modules::receivables
