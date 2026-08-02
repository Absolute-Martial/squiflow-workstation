#pragma once

// A party is anyone the shop does business with. Customers, suppliers, and
// organisations that are both, are all the same kind of record. Nothing in
// this model forces a choice, because the shop does not need one: a party can
// receive an order and a purchase in the same week.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"

namespace squiflow::modules::parties {

enum class PartyKind : std::uint8_t {
    Individual = 0,
    Organisation = 1,
};

std::string_view party_kind_name(PartyKind kind);
PartyKind party_kind_from_string(std::string_view text);

// Billing terms. The field names match what is shown to the person, so a
// support conversation always uses the same words the screen does.
enum class BillingArrangement : std::uint8_t {
    PayPerJob = 0,   // cash / card on completion, no account
    CreditAccount = 1,  // invoices accumulate, settled periodically
    QuotedRate = 2,  // an agreement sets the rate before work starts
};

std::string_view billing_name(BillingArrangement billing);
BillingArrangement billing_from_string(std::string_view text);

struct BillingTerms {
    BillingArrangement arrangement{BillingArrangement::PayPerJob};
    // How many days after an invoice is issued before it is overdue.
    // Zero means due on receipt. Meaningful only for credit accounts.
    std::int32_t net_days{0};
    // A reference or note that goes on every document for this customer,
    // such as a purchase order number they insist on.
    std::string customer_reference{};
};

struct ContactInfo {
    std::string id{};
    std::string party_id{};
    std::string label{}; // "phone", "email", "address", etc.
    std::string value{};
    std::int64_t added_at{0};
};

struct Party {
    std::string id{};
    std::string display_name{};
    PartyKind kind{PartyKind::Individual};
    // True when this party is a supplier we buy from.
    bool is_supplier{false};
    // True when this party is a customer we sell to.
    bool is_customer{true};
    std::string notes{};
    BillingTerms terms{};
    bool archived{false};
    std::int64_t created_at{0};
    std::int64_t updated_at{0};
    std::string created_by{};
    std::string updated_by{};
};

void validate(const Party& party);
void validate(const ContactInfo& contact);

engine::Row to_row(const Party& party);
Party party_from_row(const engine::Row& row);

engine::Row to_row(const ContactInfo& contact);
ContactInfo contact_from_row(const engine::Row& row);

}  // namespace squiflow::modules::parties
