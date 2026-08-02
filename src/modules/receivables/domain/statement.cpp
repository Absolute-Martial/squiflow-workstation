#include "modules/receivables/domain/statement.hpp"

#include <algorithm>
#include <limits>

#include "engine/records/money.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::receivables {
namespace {

constexpr std::int64_t kMillisecondsPerDay = 86'400'000;

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

StatementEntryKind entry_kind_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return StatementEntryKind::InvoiceCharged;
        case 1: return StatementEntryKind::PaymentReceived;
        case 2: return StatementEntryKind::AllocationReleased;
        case 3: return StatementEntryKind::PaymentAllocated;
        case 4: return StatementEntryKind::InvoiceCancelled;
        default:
            return static_cast<StatementEntryKind>(
                std::numeric_limits<std::uint8_t>::max());
    }
}

bool add_amount(std::int64_t& target, std::int64_t amount) noexcept {
    const engine::MoneyResult result = engine::money_add(
        engine::Money{target}, engine::Money{amount});
    if (!result.ok) {
        return false;
    }
    target = result.value.minor;
    return true;
}

bool subtract_amount(std::int64_t& target, std::int64_t amount) noexcept {
    const engine::MoneyResult result = engine::money_subtract(
        engine::Money{target}, engine::Money{amount});
    if (!result.ok || result.value.is_negative()) {
        return false;
    }
    target = result.value.minor;
    return true;
}

std::int64_t age_anchor(const AgingItem& item) noexcept {
    return item.due_at > 0 ? item.due_at : item.issued_at;
}

bool nonnegative(const StatementTotals& totals) noexcept {
    return totals.opening_outstanding_minor >= 0 &&
           totals.opening_unallocated_minor >= 0 &&
           totals.charged_minor >= 0 &&
           totals.cancelled_minor >= 0 &&
           totals.paid_minor >= 0 &&
           totals.allocated_minor >= 0 &&
           totals.released_minor >= 0 &&
           totals.outstanding_minor >= 0 &&
           totals.unallocated_minor >= 0;
}

}  // namespace

void validate(const StatementEntry& entry) {
    if (blank(entry.id)) {
        throw RuleViolation("This statement row has no record to be saved under.");
    }
    if (blank(entry.statement_id)) {
        throw RuleViolation("A statement row must belong to a statement.");
    }
    if (blank(entry.source_id)) {
        throw RuleViolation("A statement row must identify its source record.");
    }
    if (entry.occurred_at <= 0) {
        throw RuleViolation("A statement row must record when its event occurred.");
    }
    if (entry.amount_minor <= 0) {
        throw RuleViolation("A statement row amount must be greater than zero.");
    }
    if (blank(entry.reference) && blank(entry.description)) {
        throw RuleViolation("A statement row must say what customer fact it represents.");
    }

    switch (entry.kind) {
        case StatementEntryKind::InvoiceCharged:
        case StatementEntryKind::PaymentReceived:
        case StatementEntryKind::AllocationReleased:
        case StatementEntryKind::PaymentAllocated:
        case StatementEntryKind::InvoiceCancelled:
            break;
        default:
            throw RuleViolation("That statement row has an event this build does not understand.");
    }
}

void validate(const Statement& statement) {
    if (blank(statement.id)) {
        throw RuleViolation("This statement has no record to be saved under.");
    }
    if (blank(statement.party_id)) {
        throw RuleViolation("A statement must belong to a customer.");
    }
    if (statement.period_from <= 0 ||
        statement.period_through < statement.period_from) {
        throw RuleViolation("A statement must have a usable inclusive period.");
    }
    if (statement.prepared_at < statement.period_through ||
        blank(statement.prepared_by)) {
        throw RuleViolation("A statement must record who prepared the completed period and when.");
    }
    if (!nonnegative(statement.totals)) {
        throw RuleViolation("Statement totals cannot contain negative money.");
    }

    for (const StatementEntry& entry : statement.entries) {
        validate(entry);
        if (entry.statement_id != statement.id) {
            throw RuleViolation("A statement contains a row belonging to another statement.");
        }
        if (entry.occurred_at < statement.period_from ||
            entry.occurred_at > statement.period_through) {
            throw RuleViolation("A statement row lies outside its requested period.");
        }
    }
}

StatementResult prepare_statement(Statement source) noexcept {
    try {
        validate(source);
    } catch (...) {
        return {};
    }

    std::sort(source.entries.begin(), source.entries.end(),
              [](const StatementEntry& left, const StatementEntry& right) {
                  if (left.occurred_at != right.occurred_at) {
                      return left.occurred_at < right.occurred_at;
                  }
                  if (left.kind != right.kind) {
                      return static_cast<std::uint8_t>(left.kind) <
                             static_cast<std::uint8_t>(right.kind);
                  }
                  if (left.source_id != right.source_id) {
                      return left.source_id < right.source_id;
                  }
                  return left.id < right.id;
              });

    const std::int64_t opening_outstanding =
        source.totals.opening_outstanding_minor;
    const std::int64_t opening_unallocated =
        source.totals.opening_unallocated_minor;
    source.totals = {};
    source.totals.opening_outstanding_minor = opening_outstanding;
    source.totals.opening_unallocated_minor = opening_unallocated;
    source.totals.outstanding_minor = opening_outstanding;
    source.totals.unallocated_minor = opening_unallocated;

    for (const StatementEntry& entry : source.entries) {
        bool ok = false;
        switch (entry.kind) {
            case StatementEntryKind::InvoiceCharged:
                ok = add_amount(source.totals.charged_minor, entry.amount_minor) &&
                     add_amount(source.totals.outstanding_minor, entry.amount_minor);
                break;

            case StatementEntryKind::PaymentReceived:
                ok = add_amount(source.totals.paid_minor, entry.amount_minor) &&
                     add_amount(source.totals.unallocated_minor, entry.amount_minor);
                break;

            case StatementEntryKind::AllocationReleased:
                ok = add_amount(source.totals.released_minor, entry.amount_minor) &&
                     add_amount(source.totals.outstanding_minor, entry.amount_minor) &&
                     add_amount(source.totals.unallocated_minor, entry.amount_minor);
                break;

            case StatementEntryKind::PaymentAllocated:
                ok = add_amount(source.totals.allocated_minor, entry.amount_minor) &&
                     subtract_amount(source.totals.outstanding_minor, entry.amount_minor) &&
                     subtract_amount(source.totals.unallocated_minor, entry.amount_minor);
                break;

            case StatementEntryKind::InvoiceCancelled:
                ok = add_amount(source.totals.cancelled_minor, entry.amount_minor) &&
                     subtract_amount(source.totals.outstanding_minor, entry.amount_minor);
                break;

            default:
                return {};
        }
        if (!ok) {
            return {};
        }
    }

    try {
        validate(source);
    } catch (...) {
        return {};
    }
    return {true, std::move(source)};
}

void validate(const StatementDelivery& delivery) {
    if (blank(delivery.id) || blank(delivery.statement_id)) {
        throw RuleViolation("A statement delivery must identify itself and its statement.");
    }
    if (blank(delivery.recipient)) {
        throw RuleViolation("A sent statement must retain its confirmed recipient.");
    }
    if (blank(delivery.content_hash)) {
        throw RuleViolation("A sent statement must retain the hash of its exact content.");
    }
    if (blank(delivery.transport_reference)) {
        throw RuleViolation("A sent statement must retain the transport confirmation.");
    }
    if (delivery.sent_at <= 0 || blank(delivery.sent_by)) {
        throw RuleViolation("A sent statement must retain when and by whom it was confirmed.");
    }
}

void validate(const AgingItem& item) {
    if (blank(item.invoice_id) || blank(item.party_id)) {
        throw RuleViolation("An aging row must identify its invoice and customer.");
    }
    if (blank(item.invoice_reference)) {
        throw RuleViolation("An aging row must preserve the invoice reference shown to the customer.");
    }
    if (item.issued_at <= 0 || item.due_at < 0) {
        throw RuleViolation("An aging row has an invalid issue or due time.");
    }
    if (item.due_at > 0 && item.due_at < item.issued_at) {
        throw RuleViolation("An invoice cannot be due before it was issued.");
    }
    if (item.outstanding_minor <= 0) {
        throw RuleViolation("An aging row must contain positive outstanding money.");
    }
}

AgingResult prepare_aging(std::vector<AgingItem> items,
                          std::int64_t as_of) noexcept {
    if (as_of <= 0) {
        return {};
    }

    std::vector<AgingRow> rows;
    rows.reserve(items.size());
    engine::Money total{0};
    for (const AgingItem& item : items) {
        try {
            validate(item);
        } catch (...) {
            return {};
        }
        if (item.issued_at > as_of) {
            return {};
        }
        const engine::MoneyResult sum = engine::money_add(
            total, engine::Money{item.outstanding_minor});
        if (!sum.ok) {
            return {};
        }
        total = sum.value;

        AgingRow row;
        static_cast<AgingItem&>(row) = item;
        const std::int64_t anchor = age_anchor(item);
        if (as_of > anchor) {
            row.age_days = (as_of - anchor) / kMillisecondsPerDay;
        }
        rows.push_back(std::move(row));
    }

    std::sort(rows.begin(), rows.end(),
              [](const AgingRow& left, const AgingRow& right) {
                  const std::int64_t left_anchor = age_anchor(left);
                  const std::int64_t right_anchor = age_anchor(right);
                  if (left_anchor != right_anchor) {
                      return left_anchor < right_anchor;
                  }
                  return left.invoice_id < right.invoice_id;
              });
    return {true, total.minor, std::move(rows)};
}

engine::Row to_row(const Statement& statement) {
    engine::Row row;
    row.set("id", engine::Value::text(statement.id));
    row.set("party_id", engine::Value::text(statement.party_id));
    row.set("period_from", engine::Value::integer(statement.period_from));
    row.set("period_through", engine::Value::integer(statement.period_through));
    row.set("prepared_at", engine::Value::integer(statement.prepared_at));
    row.set("prepared_by", engine::Value::text(statement.prepared_by));
    row.set("opening_outstanding_minor",
            engine::Value::integer(statement.totals.opening_outstanding_minor));
    row.set("opening_unallocated_minor",
            engine::Value::integer(statement.totals.opening_unallocated_minor));
    row.set("charged_minor", engine::Value::integer(statement.totals.charged_minor));
    row.set("cancelled_minor",
            engine::Value::integer(statement.totals.cancelled_minor));
    row.set("paid_minor", engine::Value::integer(statement.totals.paid_minor));
    row.set("allocated_minor",
            engine::Value::integer(statement.totals.allocated_minor));
    row.set("released_minor", engine::Value::integer(statement.totals.released_minor));
    row.set("outstanding_minor",
            engine::Value::integer(statement.totals.outstanding_minor));
    row.set("unallocated_minor",
            engine::Value::integer(statement.totals.unallocated_minor));
    return row;
}

Statement statement_from_row(const engine::Row& row) {
    Statement statement;
    statement.id = row.get("id").text_or({});
    statement.party_id = row.get("party_id").text_or({});
    statement.period_from = row.get("period_from").integer_or(0);
    statement.period_through = row.get("period_through").integer_or(0);
    statement.prepared_at = row.get("prepared_at").integer_or(0);
    statement.prepared_by = row.get("prepared_by").text_or({});
    statement.totals.opening_outstanding_minor =
        row.get("opening_outstanding_minor").integer_or(-1);
    statement.totals.opening_unallocated_minor =
        row.get("opening_unallocated_minor").integer_or(-1);
    statement.totals.charged_minor = row.get("charged_minor").integer_or(-1);
    statement.totals.cancelled_minor = row.get("cancelled_minor").integer_or(-1);
    statement.totals.paid_minor = row.get("paid_minor").integer_or(-1);
    statement.totals.allocated_minor = row.get("allocated_minor").integer_or(-1);
    statement.totals.released_minor = row.get("released_minor").integer_or(-1);
    statement.totals.outstanding_minor = row.get("outstanding_minor").integer_or(-1);
    statement.totals.unallocated_minor = row.get("unallocated_minor").integer_or(-1);
    return statement;
}

engine::Row to_row(const StatementEntry& entry) {
    engine::Row row;
    row.set("id", engine::Value::text(entry.id));
    row.set("statement_id", engine::Value::text(entry.statement_id));
    row.set("kind", engine::Value::integer(static_cast<std::int64_t>(entry.kind)));
    row.set("source_id", engine::Value::text(entry.source_id));
    row.set("occurred_at", engine::Value::integer(entry.occurred_at));
    row.set("reference", engine::Value::text(entry.reference));
    row.set("description", engine::Value::text(entry.description));
    row.set("amount_minor", engine::Value::integer(entry.amount_minor));
    return row;
}

StatementEntry statement_entry_from_row(const engine::Row& row) {
    StatementEntry entry;
    entry.id = row.get("id").text_or({});
    entry.statement_id = row.get("statement_id").text_or({});
    entry.kind = entry_kind_from(row.get("kind").integer_or(-1));
    entry.source_id = row.get("source_id").text_or({});
    entry.occurred_at = row.get("occurred_at").integer_or(0);
    entry.reference = row.get("reference").text_or({});
    entry.description = row.get("description").text_or({});
    entry.amount_minor = row.get("amount_minor").integer_or(0);
    return entry;
}

engine::Row to_row(const StatementDelivery& delivery) {
    engine::Row row;
    row.set("id", engine::Value::text(delivery.id));
    row.set("statement_id", engine::Value::text(delivery.statement_id));
    row.set("recipient", engine::Value::text(delivery.recipient));
    row.set("content_hash", engine::Value::text(delivery.content_hash));
    row.set("transport_reference",
            engine::Value::text(delivery.transport_reference));
    row.set("sent_at", engine::Value::integer(delivery.sent_at));
    row.set("sent_by", engine::Value::text(delivery.sent_by));
    return row;
}

StatementDelivery statement_delivery_from_row(const engine::Row& row) {
    StatementDelivery delivery;
    delivery.id = row.get("id").text_or({});
    delivery.statement_id = row.get("statement_id").text_or({});
    delivery.recipient = row.get("recipient").text_or({});
    delivery.content_hash = row.get("content_hash").text_or({});
    delivery.transport_reference = row.get("transport_reference").text_or({});
    delivery.sent_at = row.get("sent_at").integer_or(0);
    delivery.sent_by = row.get("sent_by").text_or({});
    return delivery;
}

}  // namespace squiflow::modules::receivables
