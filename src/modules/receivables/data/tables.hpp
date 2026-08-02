#pragma once

#include <vector>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules::receivables::tables {

inline constexpr const char* kInvoice = "receivable_invoice";
inline constexpr const char* kInvoiceLine = "receivable_invoice_line";
inline constexpr const char* kPayment = "customer_payment";
inline constexpr const char* kAllocation = "payment_allocation";
inline constexpr const char* kCreditAccount = "customer_credit_account";
inline constexpr const char* kCreditOverride = "credit_hold_override";
inline constexpr const char* kStatement = "customer_statement";
inline constexpr const char* kStatementEntry = "statement_entry";
inline constexpr const char* kStatementDelivery = "statement_delivery";
inline constexpr const char* kNumberBlock = "receivable_number_block";
inline constexpr const char* kDocumentDelivery = "document_delivery";

// Global sequence: engine 1, administration 10, parties 11, catalog 12,
// pricing 13, orders 14, receivables 15.
inline constexpr int kFirstMigration = 15;
inline constexpr int kNumberBlockMigration = 23;
inline constexpr int kDocumentDeliveryMigration = 25;

std::vector<engine::Migration> migrations();

}  // namespace squiflow::modules::receivables::tables
