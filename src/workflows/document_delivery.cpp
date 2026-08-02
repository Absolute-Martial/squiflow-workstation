#include "workflows/document_delivery.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/agreements/data/repository.hpp"
#include "modules/parties/data/repository.hpp"
#include "modules/quotations/data/repository.hpp"
#include "modules/receivables/data/repository.hpp"
#include "modules/registry.hpp"

namespace squiflow::workflows {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
    });
}

engine::Row fields(const modules::Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw modules::RuleViolation("This document delivery request could not be read.");
    }
}

const engine::Session& actor(const modules::Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("document_delivery: request has no session");
    }
    return *call.actor;
}

std::string required_text(const engine::Row& row, const char* name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) throw modules::RuleViolation(complaint);
    return *value;
}

std::string optional_text(const engine::Row& row, const char* name) {
    const engine::Value& value = row.get(name);
    if (value.is_null()) return {};
    const std::string* text = value.as_text();
    if (text == nullptr) {
        throw modules::RuleViolation(std::string("Delivery field '") + name +
                                     "' must be text when supplied.");
    }
    return *text;
}

bool optional_boolean(const engine::Row& row, const char* name) {
    const engine::Value& value = row.get(name);
    if (value.is_null()) return false;
    const auto result = value.as_integer();
    if (!result || (*result != 0 && *result != 1)) {
        throw modules::RuleViolation(std::string("Delivery field '") + name +
                                     "' must be true or false.");
    }
    return *result == 1;
}

void reject_unknown(const engine::Row& row, const std::set<std::string>& allowed) {
    for (const auto& field : row.fields()) {
        if (!allowed.contains(field.first)) {
            throw modules::RuleViolation(
                "The document delivery contains an unknown field: " + field.first + ".");
        }
    }
}

protocol::ModuleId validate_document(engine::Transaction& transaction,
                                     const std::string& type,
                                     const std::string& document_id,
                                     const std::string& version_id,
                                     const std::string& party_id) {
    if (type == "invoice") {
        if (!version_id.empty()) {
            throw modules::RuleViolation("An invoice delivery must not invent a revision.");
        }
        const auto invoice = modules::receivables::data::find_invoice(transaction, document_id);
        if (!invoice) throw modules::RuleViolation("That invoice is not on file.");
        modules::receivables::validate(*invoice);
        if (invoice->party_id != party_id) {
            throw modules::RuleViolation("That invoice belongs to another customer.");
        }
        if (invoice->state == engine::DocumentState::Draft ||
            invoice->state == engine::DocumentState::Discarded) {
            throw modules::RuleViolation("Only issued invoice evidence can be emailed.");
        }
        return protocol::ModuleId::receivables;
    }
    if (type == "quotation") {
        const auto quotation = modules::quotations::data::find_quotation(transaction, document_id);
        if (!quotation) throw modules::RuleViolation("That quotation is not on file.");
        modules::quotations::validate(*quotation);
        if (quotation->party_id != party_id) {
            throw modules::RuleViolation("That quotation belongs to another customer.");
        }
        if (blank(version_id)) {
            throw modules::RuleViolation("A quotation delivery must pin an exact revision.");
        }
        const auto revision = modules::quotations::data::find_revision(transaction, version_id);
        if (!revision || revision->quotation_id != document_id || !revision->issued) {
            throw modules::RuleViolation("Only an exact issued quotation revision can be emailed.");
        }
        modules::quotations::validate(*revision);
        return protocol::ModuleId::quotations;
    }
    if (type == "agreement") {
        if (!version_id.empty()) {
            throw modules::RuleViolation("An agreement delivery must not invent a revision.");
        }
        const auto agreement = modules::agreements::data::find_agreement(transaction, document_id);
        if (!agreement) throw modules::RuleViolation("That agreement is not on file.");
        modules::agreements::validate(*agreement);
        if (agreement->party_id != party_id) {
            throw modules::RuleViolation("That agreement belongs to another customer.");
        }
        return protocol::ModuleId::agreements;
    }
    throw modules::RuleViolation("That document type cannot be delivered in this build.");
}

WorkflowResult prepare(engine::Transaction& transaction,
                       const modules::Call& call,
                       const DocumentDeliveryClock& clock) {
    const engine::Row row = fields(call);
    reject_unknown(row, {"party_id", "document_type", "document_id",
                         "document_version_id", "recipient_contact_id", "channel",
                         "transport_profile_id",
                         "subject", "message_body", "attachment_name",
                         "content_sha256", "approval_requested"});
    const engine::RecordId delivery_id = engine::record_id_from_string(call.record_id);
    if (!delivery_id.is_valid()) {
        throw modules::RuleViolation("The prepared delivery identity is invalid.");
    }
    if (modules::receivables::data::find_document_delivery(transaction, call.record_id)) {
        throw modules::RuleViolation("That prepared delivery identity is already in use.");
    }

    modules::receivables::DocumentDelivery delivery;
    delivery.id = call.record_id;
    delivery.party_id = required_text(row, "party_id", "A document delivery needs a customer.");
    const auto party = modules::parties::data::find_party(transaction, delivery.party_id);
    if (!party) throw modules::RuleViolation("That delivery customer is not on file.");
    modules::parties::validate(*party);
    if (!party->is_customer || party->archived) {
        throw modules::RuleViolation("A document can only be sent to an active customer.");
    }

    delivery.document_id = required_text(row, "document_id", "A delivery needs a document.");
    delivery.document_version_id = optional_text(row, "document_version_id");
    delivery.document_module = validate_document(
        transaction,
        required_text(row, "document_type", "A delivery needs a document type."),
        delivery.document_id, delivery.document_version_id, delivery.party_id);

    const std::string channel = optional_text(row, "channel");
    if (!channel.empty() && channel != "email") {
        if (channel == "whatsapp") {
            throw modules::RuleViolation(
                "WhatsApp delivery is reserved for a future backend and is not enabled yet.");
        }
        throw modules::RuleViolation("That delivery channel is not understood.");
    }
    delivery.channel = modules::receivables::DeliveryChannel::Email;
    delivery.transport_profile_id = required_text(
        row, "transport_profile_id",
        "Email delivery is unavailable until a remote transport profile is configured.");
    delivery.recipient_contact_id = required_text(
        row, "recipient_contact_id", "Email delivery needs a selected customer email.");
    const auto contacts = modules::parties::data::contacts_of(transaction, delivery.party_id);
    const auto selected = std::find_if(contacts.begin(), contacts.end(), [&](const auto& contact) {
        return contact.id == delivery.recipient_contact_id;
    });
    if (selected == contacts.end() || selected->label != "email") {
        throw modules::RuleViolation("The selected contact is not this customer's email.");
    }
    modules::parties::validate(*selected);
    delivery.recipient = selected->value;
    if (!modules::receivables::valid_email_address(delivery.recipient)) {
        throw modules::RuleViolation("The selected customer email is not safe to send.");
    }

    delivery.subject = optional_text(row, "subject");
    delivery.message_body = optional_text(row, "message_body");
    delivery.attachment_name = required_text(
        row, "attachment_name", "Email delivery needs the reviewed document attachment.");
    delivery.content_sha256 = required_text(
        row, "content_sha256", "Email delivery needs the exact attachment hash.");
    delivery.approval_requested = optional_boolean(row, "approval_requested");
    delivery.prepared_at = clock();
    if (delivery.prepared_at <= 0) {
        throw modules::RuleViolation("The delivery preparation time is invalid.");
    }
    delivery.prepared_by = engine::to_string(actor(call).person);
    delivery.confirmation_token = modules::receivables::delivery_confirmation_token(delivery);
    modules::receivables::data::save_document_delivery(transaction, delivery);
    return {{protocol::ModuleId::receivables, delivery_id},
            "Prepared optional email delivery; nothing was sent."};
}

WorkflowResult request(engine::Transaction& transaction,
                       const modules::Call& call,
                       const DocumentDeliveryClock& clock) {
    const engine::Row row = fields(call);
    reject_unknown(row, {"confirmation_token"});
    auto delivery = modules::receivables::data::find_document_delivery(transaction, call.record_id);
    if (!delivery) throw modules::RuleViolation("That prepared delivery is not on file.");
    modules::receivables::validate(*delivery);
    if (delivery->state != modules::receivables::DeliveryState::Prepared) {
        throw modules::RuleViolation("That delivery has already been requested from the backend.");
    }
    const std::string confirmed = required_text(
        row, "confirmation_token", "Sending requires confirmation of the reviewed message.");
    if (confirmed != delivery->confirmation_token) {
        throw modules::RuleViolation("The recipient, message, or attachment changed after review.");
    }
    if (delivery->channel != modules::receivables::DeliveryChannel::Email) {
        throw modules::RuleViolation("That remote delivery channel is not enabled.");
    }
    if (delivery->document_module == protocol::ModuleId::receivables) {
        static_cast<void>(validate_document(transaction, "invoice", delivery->document_id,
                                            delivery->document_version_id,
                                            delivery->party_id));
    } else if (delivery->document_module == protocol::ModuleId::quotations) {
        static_cast<void>(validate_document(transaction, "quotation", delivery->document_id,
                                            delivery->document_version_id,
                                            delivery->party_id));
    } else if (delivery->document_module == protocol::ModuleId::agreements) {
        static_cast<void>(validate_document(transaction, "agreement", delivery->document_id,
                                            delivery->document_version_id,
                                            delivery->party_id));
    } else {
        throw modules::RuleViolation("That prepared document module is no longer supported.");
    }
    delivery->state = modules::receivables::DeliveryState::Requested;
    delivery->requested_at = clock();
    if (delivery->requested_at < delivery->prepared_at) {
        throw modules::RuleViolation("The delivery request time precedes its preparation.");
    }
    delivery->requested_by = engine::to_string(actor(call).person);
    delivery->request_idempotency_key = call.idempotency_key;
    modules::receivables::data::save_document_delivery(transaction, *delivery);
    return {{protocol::ModuleId::receivables,
             engine::record_id_from_string(delivery->id)},
            "Requested idempotent email delivery from the remote backend; approval remains pending."};
}

std::vector<protocol::ModuleId> requirements() {
    return {protocol::ModuleId::parties, protocol::ModuleId::receivables,
            protocol::ModuleId::quotations, protocol::ModuleId::agreements};
}

}  // namespace

WorkflowDefinition make_prepare_document_delivery(DocumentDeliveryClock clock) {
    if (!clock) throw modules::RegistryError("prepare_document_delivery needs a clock");
    return {protocol::OperationId::prepare_document_delivery, requirements(),
            [clock = std::move(clock)](engine::Transaction& transaction,
                                       const modules::Call& call) {
                return prepare(transaction, call, clock);
            }};
}

WorkflowDefinition make_request_document_delivery(DocumentDeliveryClock clock) {
    if (!clock) throw modules::RegistryError("request_document_delivery needs a clock");
    return {protocol::OperationId::request_document_delivery, requirements(),
            [clock = std::move(clock)](engine::Transaction& transaction,
                                       const modules::Call& call) {
                return request(transaction, call, clock);
            }};
}

}  // namespace squiflow::workflows
