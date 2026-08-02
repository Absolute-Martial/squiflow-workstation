#include "modules/receivables/domain/document_delivery.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <sstream>
#include <string_view>

#include "modules/context.hpp"

namespace squiflow::modules::receivables {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
    });
}

bool sha256_text(const std::string& value) noexcept {
    return value.size() == 64U && std::all_of(value.begin(), value.end(), [](const char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
    });
}

std::uint64_t fnv1a(std::string_view input, std::uint64_t seed) noexcept {
    std::uint64_t hash = seed;
    for (const char c : input) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

void append(std::string& target, const std::string& value) {
    target += std::to_string(value.size());
    target.push_back(':');
    target += value;
    target.push_back('|');
}

}  // namespace

bool valid_email_address(const std::string& value) noexcept {
    if (value.empty() || value.size() > 254U || value.front() == '.' ||
        value.back() == '.' || value.find('\r') != std::string::npos ||
        value.find('\n') != std::string::npos || value.find(' ') != std::string::npos ||
        value.find('\t') != std::string::npos) {
        return false;
    }
    const std::size_t at = value.find('@');
    if (at == std::string::npos || at == 0U || at + 1U >= value.size() ||
        value.find('@', at + 1U) != std::string::npos) {
        return false;
    }
    const std::string_view local{value.data(), at};
    const std::string_view domain{value.data() + at + 1U, value.size() - at - 1U};
    if (local.size() > 64U || domain.find('.') == std::string_view::npos ||
        domain.front() == '-' || domain.back() == '-' || domain.front() == '.' ||
        domain.back() == '.' || domain.find("..") != std::string_view::npos) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char c) {
        const unsigned char u = static_cast<unsigned char>(c);
        return u >= 33U && u <= 126U && c != '<' && c != '>' && c != ',' && c != ';';
    });
}

std::string delivery_confirmation_token(const DocumentDelivery& delivery) {
    std::string canonical;
    append(canonical, delivery.id);
    append(canonical, delivery.party_id);
    append(canonical, std::to_string(static_cast<std::uint32_t>(delivery.document_module)));
    append(canonical, delivery.document_id);
    append(canonical, delivery.document_version_id);
    append(canonical, std::to_string(static_cast<std::uint32_t>(delivery.channel)));
    append(canonical, delivery.transport_profile_id);
    append(canonical, delivery.recipient);
    append(canonical, delivery.subject);
    append(canonical, delivery.message_body);
    append(canonical, delivery.attachment_name);
    append(canonical, delivery.content_sha256);
    append(canonical, delivery.approval_requested ? "approval" : "delivery");
    const std::uint64_t first = fnv1a(canonical, UINT64_C(14695981039346656037));
    const std::uint64_t second = fnv1a(canonical, UINT64_C(1099511628211));
    std::ostringstream output;
    output << std::hex << std::setfill('0') << std::setw(16) << first
           << std::setw(16) << second;
    return output.str();
}

void validate(const DocumentDelivery& delivery) {
    if (blank(delivery.id) || blank(delivery.party_id) ||
        !protocol::is_valid(delivery.document_module) || blank(delivery.document_id) ||
        blank(delivery.transport_profile_id) || blank(delivery.recipient_contact_id) ||
        blank(delivery.recipient) ||
        blank(delivery.attachment_name) || !sha256_text(delivery.content_sha256) ||
        delivery.prepared_at <= 0 || blank(delivery.prepared_by)) {
        throw RuleViolation("The prepared document delivery is incomplete.");
    }
    if (delivery.channel != DeliveryChannel::Email &&
        delivery.channel != DeliveryChannel::WhatsApp) {
        throw RuleViolation("The prepared delivery channel is unknown.");
    }
    if (delivery.channel == DeliveryChannel::Email && !valid_email_address(delivery.recipient)) {
        throw RuleViolation("The prepared email recipient is invalid.");
    }
    if (delivery.subject.find('\r') != std::string::npos ||
        delivery.subject.find('\n') != std::string::npos) {
        throw RuleViolation("The email subject contains a header injection character.");
    }
    if (delivery.transport_profile_id.size() > 128U || delivery.subject.size() > 998U ||
        delivery.message_body.size() > 65'536U || delivery.attachment_name.size() > 255U) {
        throw RuleViolation("The prepared email exceeds a safe field-size limit.");
    }
    if (delivery.confirmation_token != delivery_confirmation_token(delivery)) {
        throw RuleViolation("The prepared delivery confirmation token is stale.");
    }
    if (delivery.state == DeliveryState::Prepared) {
        if (delivery.requested_at != 0 || !delivery.requested_by.empty() ||
            !delivery.request_idempotency_key.empty() || delivery.accepted_at != 0 ||
            !delivery.transport_reference.empty() || !delivery.failure_reason.empty()) {
            throw RuleViolation("An unsent prepared delivery carries transport evidence.");
        }
    } else {
        if (delivery.requested_at <= 0 || blank(delivery.requested_by) ||
            blank(delivery.request_idempotency_key)) {
            throw RuleViolation("A remote delivery request lacks request evidence.");
        }
        if (delivery.state == DeliveryState::Accepted &&
            (delivery.accepted_at < delivery.requested_at ||
             blank(delivery.transport_reference) || !delivery.failure_reason.empty())) {
            throw RuleViolation("An accepted delivery lacks coherent transport evidence.");
        }
        if (delivery.state == DeliveryState::Failed && blank(delivery.failure_reason)) {
            throw RuleViolation("A failed delivery must retain its reason.");
        }
        if (delivery.state == DeliveryState::Requested &&
            (delivery.accepted_at != 0 || !delivery.transport_reference.empty() ||
             !delivery.failure_reason.empty())) {
            throw RuleViolation("A pending backend request carries a result it does not have.");
        }
    }
}

engine::Row to_row(const DocumentDelivery& d) {
    engine::Row row;
    row.set("id", engine::Value::text(d.id));
    row.set("party_id", engine::Value::text(d.party_id));
    row.set("document_module", engine::Value::integer(static_cast<std::int64_t>(d.document_module)));
    row.set("document_id", engine::Value::text(d.document_id));
    row.set("document_version_id", engine::Value::text(d.document_version_id));
    row.set("channel", engine::Value::integer(static_cast<std::int64_t>(d.channel)));
    row.set("transport_profile_id", engine::Value::text(d.transport_profile_id));
    row.set("recipient_contact_id", engine::Value::text(d.recipient_contact_id));
    row.set("recipient", engine::Value::text(d.recipient));
    row.set("subject", engine::Value::text(d.subject));
    row.set("message_body", engine::Value::text(d.message_body));
    row.set("attachment_name", engine::Value::text(d.attachment_name));
    row.set("content_sha256", engine::Value::text(d.content_sha256));
    row.set("approval_requested", engine::Value::boolean(d.approval_requested));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(d.state)));
    row.set("confirmation_token", engine::Value::text(d.confirmation_token));
    row.set("prepared_at", engine::Value::integer(d.prepared_at));
    row.set("prepared_by", engine::Value::text(d.prepared_by));
    row.set("requested_at", engine::Value::integer(d.requested_at));
    row.set("requested_by", engine::Value::text(d.requested_by));
    row.set("request_idempotency_key", engine::Value::text(d.request_idempotency_key));
    row.set("accepted_at", engine::Value::integer(d.accepted_at));
    row.set("transport_reference", engine::Value::text(d.transport_reference));
    row.set("failure_reason", engine::Value::text(d.failure_reason));
    return row;
}

DocumentDelivery document_delivery_from_row(const engine::Row& row) {
    DocumentDelivery d;
    d.id = row.get("id").text_or({});
    d.party_id = row.get("party_id").text_or({});
    d.document_module = static_cast<protocol::ModuleId>(row.get("document_module").integer_or(-1));
    d.document_id = row.get("document_id").text_or({});
    d.document_version_id = row.get("document_version_id").text_or({});
    d.channel = static_cast<DeliveryChannel>(row.get("channel").integer_or(-1));
    d.transport_profile_id = row.get("transport_profile_id").text_or({});
    d.recipient_contact_id = row.get("recipient_contact_id").text_or({});
    d.recipient = row.get("recipient").text_or({});
    d.subject = row.get("subject").text_or({});
    d.message_body = row.get("message_body").text_or({});
    d.attachment_name = row.get("attachment_name").text_or({});
    d.content_sha256 = row.get("content_sha256").text_or({});
    d.approval_requested = row.get("approval_requested").boolean_or(false);
    d.state = static_cast<DeliveryState>(row.get("state").integer_or(-1));
    d.confirmation_token = row.get("confirmation_token").text_or({});
    d.prepared_at = row.get("prepared_at").integer_or(0);
    d.prepared_by = row.get("prepared_by").text_or({});
    d.requested_at = row.get("requested_at").integer_or(0);
    d.requested_by = row.get("requested_by").text_or({});
    d.request_idempotency_key = row.get("request_idempotency_key").text_or({});
    d.accepted_at = row.get("accepted_at").integer_or(0);
    d.transport_reference = row.get("transport_reference").text_or({});
    d.failure_reason = row.get("failure_reason").text_or({});
    return d;
}

}  // namespace squiflow::modules::receivables
