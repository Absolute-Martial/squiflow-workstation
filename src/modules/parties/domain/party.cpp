#include "modules/parties/domain/party.hpp"

#include "modules/context.hpp"

namespace squiflow::modules::parties {

std::string_view party_kind_name(PartyKind kind) {
    switch (kind) {
        case PartyKind::Individual:   return "individual";
        case PartyKind::Organisation: return "organisation";
    }
    return "individual";
}

PartyKind party_kind_from_string(std::string_view text) {
    if (text == "organisation") return PartyKind::Organisation;
    return PartyKind::Individual;
}

std::string_view billing_name(BillingArrangement billing) {
    switch (billing) {
        case BillingArrangement::PayPerJob:      return "pay_per_job";
        case BillingArrangement::CreditAccount:  return "credit_account";
        case BillingArrangement::QuotedRate:     return "quoted_rate";
    }
    return "pay_per_job";
}

BillingArrangement billing_from_string(std::string_view text) {
    if (text == "credit_account") return BillingArrangement::CreditAccount;
    if (text == "quoted_rate")    return BillingArrangement::QuotedRate;
    return BillingArrangement::PayPerJob;
}

void validate(const Party& party) {
    if (party.id.empty()) {
        throw RuleViolation("This party has no record to be saved under.");
    }
    const std::string name = std::string(party.display_name);
    bool blank = true;
    for (const char character : name) {
        if (static_cast<unsigned char>(character) > ' ') {
            blank = false;
            break;
        }
    }
    if (blank) {
        throw RuleViolation("A customer or supplier needs a name.");
    }
    if (!party.is_customer && !party.is_supplier) {
        throw RuleViolation("A party has to be a customer, a supplier, or both.");
    }
    if (party.terms.net_days < 0) {
        throw RuleViolation("Days-until-overdue cannot be negative.");
    }
}

void validate(const ContactInfo& contact) {
    if (contact.id.empty()) {
        throw RuleViolation("This contact entry has no record to be saved under.");
    }
    if (contact.party_id.empty()) {
        throw RuleViolation("This contact entry does not say which party it belongs to.");
    }
    if (contact.label.empty()) {
        throw RuleViolation("A contact entry needs a label such as 'phone' or 'email'.");
    }
    if (contact.value.empty()) {
        throw RuleViolation("A contact entry with no value does not tell us anything.");
    }
}

engine::Row to_row(const Party& party) {
    engine::Row row;
    row.set("id",           engine::Value::text(party.id));
    row.set("display_name", engine::Value::text(party.display_name));
    row.set("kind",         engine::Value::text(std::string(party_kind_name(party.kind))));
    row.set("is_supplier",  engine::Value::boolean(party.is_supplier));
    row.set("is_customer",  engine::Value::boolean(party.is_customer));
    row.set("notes",        engine::Value::text(party.notes));
    row.set("billing",      engine::Value::text(std::string(billing_name(party.terms.arrangement))));
    row.set("net_days",     engine::Value::integer(party.terms.net_days));
    row.set("customer_ref",engine::Value::text(party.terms.customer_reference));
    row.set("archived",     engine::Value::boolean(party.archived));
    row.set("created_at",   engine::Value::integer(party.created_at));
    row.set("updated_at",   engine::Value::integer(party.updated_at));
    row.set("created_by",   engine::Value::text(party.created_by));
    row.set("updated_by",   engine::Value::text(party.updated_by));
    return row;
}

Party party_from_row(const engine::Row& row) {
    Party party;
    party.id           = row.get("id").text_or({});
    party.display_name = row.get("display_name").text_or({});
    party.kind         = party_kind_from_string(row.get("kind").text_or({}));
    party.is_supplier  = row.get("is_supplier").boolean_or(false);
    party.is_customer  = row.get("is_customer").boolean_or(true);
    party.notes        = row.get("notes").text_or({});
    party.archived     = row.get("archived").boolean_or(false);
    party.terms.arrangement      = billing_from_string(row.get("billing").text_or({}));
    party.terms.net_days         = static_cast<std::int32_t>(row.get("net_days").integer_or(0));
    party.terms.customer_reference = row.get("customer_ref").text_or({});
    party.created_at   = row.get("created_at").integer_or(0);
    party.updated_at   = row.get("updated_at").integer_or(0);
    party.created_by   = row.get("created_by").text_or({});
    party.updated_by   = row.get("updated_by").text_or({});
    return party;
}

engine::Row to_row(const ContactInfo& contact) {
    engine::Row row;
    row.set("id",       engine::Value::text(contact.id));
    row.set("party_id", engine::Value::text(contact.party_id));
    row.set("label",    engine::Value::text(contact.label));
    row.set("value",    engine::Value::text(contact.value));
    row.set("added_at", engine::Value::integer(contact.added_at));
    return row;
}

ContactInfo contact_from_row(const engine::Row& row) {
    ContactInfo contact;
    contact.id       = row.get("id").text_or({});
    contact.party_id = row.get("party_id").text_or({});
    contact.label    = row.get("label").text_or({});
    contact.value    = row.get("value").text_or({});
    contact.added_at = row.get("added_at").integer_or(0);
    return contact;
}

}  // namespace squiflow::modules::parties
