#include "modules/receivables/data/repository.hpp"

#include "modules/context.hpp"

namespace squiflow::modules::receivables::data {
namespace {

void upsert(engine::Transaction& transaction, const char* table,
            const std::string& key, const engine::Row& row) {
    if (!transaction.replace(table, key, row)) transaction.insert(table, row);
}

}  // namespace

void save_invoice(engine::Transaction& transaction, const Invoice& invoice) {
    validate(invoice);
    upsert(transaction, tables::kInvoice, invoice.id, to_row(invoice));
}

void save_number_block(engine::Transaction& transaction,
                       const DocumentNumberBlock& block) {
    validate(block);
    engine::Query query{tables::kNumberBlock};
    query.where_equals("document_kind",
                       engine::Value::integer(static_cast<std::int64_t>(block.kind)));
    query.where_equals("series", engine::Value::text(block.series));
    for (const engine::Row& row : transaction.select(query)) {
        const DocumentNumberBlock other = document_number_block_from_row(row);
        validate(other);
        if (other.id != block.id && overlaps(block, other)) {
            throw RuleViolation(
                "That reserved number block overlaps an existing range.");
        }
    }
    upsert(transaction, tables::kNumberBlock, block.id, to_row(block));
}

void save_invoice_line(engine::Transaction& transaction, const InvoiceLine& line) {
    validate(line);
    upsert(transaction, tables::kInvoiceLine, line.id, to_row(line));
}

bool remove_invoice_line(engine::Transaction& transaction, const std::string& id) {
    return transaction.remove(tables::kInvoiceLine, id);
}

void save_payment(engine::Transaction& transaction, const Payment& payment) {
    validate(payment);
    upsert(transaction, tables::kPayment, payment.id, to_row(payment));
}

void save_allocation(engine::Transaction& transaction,
                     const PaymentAllocation& allocation) {
    validate(allocation);
    upsert(transaction, tables::kAllocation, allocation.id, to_row(allocation));
}

void save_credit_account(engine::Transaction& transaction,
                         const CreditAccount& account) {
    validate(account);
    upsert(transaction, tables::kCreditAccount, account.id, to_row(account));
}

void save_credit_override(engine::Transaction& transaction,
                          const CreditOverride& evidence) {
    validate(evidence);
    upsert(transaction, tables::kCreditOverride, evidence.id, to_row(evidence));
}

void save_statement(engine::Transaction& transaction, const Statement& statement) {
    validate(statement);
    upsert(transaction, tables::kStatement, statement.id, to_row(statement));
}

void save_statement_entry(engine::Transaction& transaction,
                          const StatementEntry& entry) {
    validate(entry);
    upsert(transaction, tables::kStatementEntry, entry.id, to_row(entry));
}

void save_statement_delivery(engine::Transaction& transaction,
                             const StatementDelivery& delivery) {
    validate(delivery);
    upsert(transaction, tables::kStatementDelivery, delivery.id, to_row(delivery));
}

}  // namespace squiflow::modules::receivables::data
