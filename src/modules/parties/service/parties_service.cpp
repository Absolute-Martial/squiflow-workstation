#include "modules/parties/service/parties_service.hpp"

#include <stdexcept>

#include "engine/records/payload.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/parties/domain/party.hpp"

namespace squiflow::modules::parties {
namespace {

std::string required_text(const engine::Row& fields, const std::string& name,
                          const std::string& complaint) {
    const std::string value = fields.get(name).text_or({});
    bool blank = true;
    for (const char c : value) {
        if (static_cast<unsigned char>(c) > ' ') { blank = false; break; }
    }
    if (blank) throw RuleViolation(complaint);
    return value;
}

std::string subject(const Call& call) {
    if (call.record_id.empty())
        throw RuleViolation("This request does not say which record it is about.");
    return call.record_id;
}

}  // namespace

engine::Row read_fields(const Call& call) {
    if (call.payload.empty()) return {};
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError& broken) {
        throw RuleViolation(std::string("This request could not be read: ") + broken.what());
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr)
        throw std::logic_error("a handler was run without a session");
    return *call.actor;
}

PartiesService::PartiesService(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) throw std::logic_error("parties needs a clock");
}

void PartiesService::create_party(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);

    if (data::find_party(transaction, id)) {
        throw RuleViolation("That party already exists.");
    }

    Party party;
    party.id           = id;
    party.display_name = required_text(fields, "display_name", "A customer or supplier needs a name.");
    party.kind         = party_kind_from_string(fields.get("kind").text_or({}));
    party.is_customer  = fields.get("is_customer").boolean_or(true);
    party.is_supplier  = fields.get("is_supplier").boolean_or(false);
    party.notes        = fields.get("notes").text_or({});
    party.created_at   = clock_();
    party.updated_at   = party.created_at;
    party.created_by   = engine::to_string(actor(call).person);
    party.updated_by   = party.created_by;
    validate(party);

    data::save_party(transaction, party);
}

void PartiesService::update_party(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);

    std::optional<Party> existing = data::find_party(transaction, id);
    if (!existing) throw RuleViolation("That party is not in the list.");
    if (existing->archived) throw RuleViolation("Unarchive this party before changing it.");

    Party updated = *existing;
    if (fields.has("display_name"))
        updated.display_name = required_text(fields, "display_name",
                                             "A customer or supplier needs a name.");
    if (fields.has("kind"))        updated.kind = party_kind_from_string(fields.get("kind").text_or({}));
    if (fields.has("is_customer")) updated.is_customer = fields.get("is_customer").boolean_or(true);
    if (fields.has("is_supplier")) updated.is_supplier = fields.get("is_supplier").boolean_or(false);
    if (fields.has("notes"))       updated.notes = fields.get("notes").text_or({});
    updated.updated_at = clock_();
    updated.updated_by = engine::to_string(actor(call).person);
    validate(updated);

    data::save_party(transaction, updated);
}

void PartiesService::archive_party(engine::Transaction& transaction, const Call& call) {
    const std::string id = subject(call);
    std::optional<Party> party = data::find_party(transaction, id);
    if (!party) throw RuleViolation("That party is not in the list.");
    if (party->archived) return; // asking twice is not an error

    party->archived   = true;
    party->updated_at = clock_();
    party->updated_by = engine::to_string(actor(call).person);
    data::save_party(transaction, *party);
}

void PartiesService::set_terms(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);

    std::optional<Party> party = data::find_party(transaction, id);
    if (!party) throw RuleViolation("That party is not in the list.");
    if (!party->is_customer)
        throw RuleViolation("Billing terms only apply to customers.");

    if (fields.has("billing"))
        party->terms.arrangement = billing_from_string(fields.get("billing").text_or({}));
    if (fields.has("net_days"))
        party->terms.net_days = static_cast<std::int32_t>(fields.get("net_days").integer_or(0));
    if (fields.has("customer_ref"))
        party->terms.customer_reference = fields.get("customer_ref").text_or({});

    if (party->terms.net_days < 0)
        throw RuleViolation("Days-until-overdue cannot be negative.");

    party->updated_at = clock_();
    party->updated_by = engine::to_string(actor(call).person);
    data::save_party(transaction, *party);
}

void PartiesService::add_contact(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string contact_id = subject(call);

    ContactInfo contact;
    contact.id       = contact_id;
    contact.party_id = fields.get("party_id").text_or({});
    contact.label    = fields.get("label").text_or({});
    contact.value    = fields.get("value").text_or({});
    contact.added_at = clock_();
    validate(contact);

    if (!data::find_party(transaction, contact.party_id))
        throw RuleViolation("That party is not in the list.");

    data::save_contact(transaction, contact);
}

}  // namespace squiflow::modules::parties
