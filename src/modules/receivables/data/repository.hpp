#pragma once

#include <limits>
#include <optional>
#include <string>
#include <vector>

#include <squiflow/protocol/module_id.hpp>

#include "engine/storage/store.hpp"
#include "modules/receivables/data/tables.hpp"
#include "modules/receivables/domain/credit_account.hpp"
#include "modules/receivables/domain/invoice.hpp"
#include "modules/receivables/domain/payment.hpp"
#include "modules/receivables/domain/statement.hpp"

namespace squiflow::modules::receivables::data {

template <typename Reader>
std::optional<Invoice> find_invoice(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kInvoice, id);
    return row ? std::optional<Invoice>{invoice_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<InvoiceLine> find_invoice_line(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kInvoiceLine, id);
    return row ? std::optional<InvoiceLine>{invoice_line_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<Payment> find_payment(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kPayment, id);
    return row ? std::optional<Payment>{payment_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<PaymentAllocation> find_allocation(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kAllocation, id);
    return row ? std::optional<PaymentAllocation>{payment_allocation_from_row(*row)}
               : std::nullopt;
}

template <typename Reader>
std::optional<CreditAccount> find_credit_account(const Reader& reader,
                                                 const std::string& party_id) {
    const auto row = reader.find(tables::kCreditAccount, party_id);
    return row ? std::optional<CreditAccount>{credit_account_from_row(*row)}
               : std::nullopt;
}

template <typename Reader>
std::optional<Statement> find_statement(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kStatement, id);
    return row ? std::optional<Statement>{statement_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<StatementDelivery> find_statement_delivery(
    const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kStatementDelivery, id);
    return row ? std::optional<StatementDelivery>{statement_delivery_from_row(*row)}
               : std::nullopt;
}

template <typename Reader>
std::vector<InvoiceLine> lines_for_invoice(const Reader& reader,
                                           const std::string& invoice_id) {
    engine::Query query{tables::kInvoiceLine};
    query.where_equals("invoice_id", engine::Value::text(invoice_id));
    query.order_by("position");
    query.order_by("id");
    std::vector<InvoiceLine> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(invoice_line_from_row(row));
    }
    return result;
}

template <typename Reader>
std::optional<std::int64_t> next_line_position(const Reader& reader,
                                               const std::string& invoice_id) {
    std::int64_t highest = -1;
    for (const InvoiceLine& line : lines_for_invoice(reader, invoice_id)) {
        if (line.position > highest) highest = line.position;
    }
    if (highest == std::numeric_limits<std::int64_t>::max()) return std::nullopt;
    return highest + 1;
}

template <typename Reader>
std::vector<Invoice> invoices_for_party(const Reader& reader,
                                        const std::string& party_id) {
    engine::Query query{tables::kInvoice};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.order_by("issued_at");
    query.order_by("id");
    std::vector<Invoice> result;
    for (const engine::Row& row : reader.select(query)) result.push_back(invoice_from_row(row));
    return result;
}

template <typename Reader>
std::vector<Payment> payments_for_party(const Reader& reader,
                                        const std::string& party_id) {
    engine::Query query{tables::kPayment};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.order_by("paid_at");
    query.order_by("id");
    std::vector<Payment> result;
    for (const engine::Row& row : reader.select(query)) result.push_back(payment_from_row(row));
    return result;
}

template <typename Reader>
std::vector<PaymentAllocation> allocations_for_payment(const Reader& reader,
                                                       const std::string& payment_id) {
    engine::Query query{tables::kAllocation};
    query.where_equals("payment_id", engine::Value::text(payment_id));
    query.order_by("allocated_at");
    query.order_by("id");
    std::vector<PaymentAllocation> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(payment_allocation_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<PaymentAllocation> allocations_for_target(
    const Reader& reader, protocol::ModuleId module, const std::string& record_id) {
    engine::Query query{tables::kAllocation};
    query.where_equals("target_module",
                       engine::Value::integer(static_cast<std::int64_t>(module)));
    query.where_equals("target_record_id", engine::Value::text(record_id));
    query.order_by("allocated_at");
    query.order_by("id");
    std::vector<PaymentAllocation> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(payment_allocation_from_row(row));
    }
    return result;
}

template <typename Reader>
engine::MoneyResult outstanding_for_invoice(const Reader& reader,
                                            const std::string& invoice_id) {
    const engine::MoneyResult total = invoice_total(lines_for_invoice(reader, invoice_id));
    if (!total.ok) return {false, {}};
    engine::Money allocated{0};
    for (const PaymentAllocation& allocation : allocations_for_target(
             reader, protocol::ModuleId::receivables, invoice_id)) {
        try {
            validate(allocation);
        } catch (...) {
            return {false, {}};
        }
        if (allocation.state == AllocationState::Released) continue;
        const engine::MoneyResult sum = engine::money_add(
            allocated, engine::Money{allocation.amount_minor});
        if (!sum.ok) return {false, {}};
        allocated = sum.value;
    }
    const engine::MoneyResult remaining = engine::money_subtract(total.value, allocated);
    if (!remaining.ok || remaining.value.is_negative()) return {false, {}};
    return remaining;
}

template <typename Reader>
std::vector<StatementEntry> entries_for_statement(const Reader& reader,
                                                  const std::string& statement_id) {
    engine::Query query{tables::kStatementEntry};
    query.where_equals("statement_id", engine::Value::text(statement_id));
    query.order_by("occurred_at");
    query.order_by("kind");
    query.order_by("id");
    std::vector<StatementEntry> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(statement_entry_from_row(row));
    }
    return result;
}

void save_invoice(engine::Transaction& transaction, const Invoice& invoice);
void save_invoice_line(engine::Transaction& transaction, const InvoiceLine& line);
bool remove_invoice_line(engine::Transaction& transaction, const std::string& id);
void save_payment(engine::Transaction& transaction, const Payment& payment);
void save_allocation(engine::Transaction& transaction, const PaymentAllocation& allocation);
void save_credit_account(engine::Transaction& transaction, const CreditAccount& account);
void save_credit_override(engine::Transaction& transaction, const CreditOverride& evidence);
void save_statement(engine::Transaction& transaction, const Statement& statement);
void save_statement_entry(engine::Transaction& transaction, const StatementEntry& entry);
void save_statement_delivery(engine::Transaction& transaction,
                             const StatementDelivery& delivery);

}  // namespace squiflow::modules::receivables::data
