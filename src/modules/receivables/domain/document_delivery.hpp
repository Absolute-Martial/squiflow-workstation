#pragma once

#include <cstdint>
#include <string>

#include <squiflow/protocol/module_id.hpp>

#include "engine/storage/store.hpp"

namespace squiflow::modules::receivables {

// Delivery is channel-neutral so adding WhatsApp later does not rewrite stored
// evidence. Only Email is enabled by the Phase 5.8 workstation workflow.
enum class DeliveryChannel : std::uint8_t { Email = 0, WhatsApp = 1 };
enum class DeliveryState : std::uint8_t {
    Prepared = 0,
    Requested = 1,
    Accepted = 2,
    Failed = 3,
};

struct DocumentDelivery {
    std::string id{};
    std::string party_id{};
    protocol::ModuleId document_module{protocol::ModuleId::receivables};
    std::string document_id{};
    std::string document_version_id{};
    DeliveryChannel channel{DeliveryChannel::Email};
    // Public backend configuration identity, never an SMTP/API secret.
    std::string transport_profile_id{};
    std::string recipient_contact_id{};
    std::string recipient{};
    std::string subject{};       // Optional: an attachment-only email is valid.
    std::string message_body{};  // Optional by product decision.
    std::string attachment_name{};
    std::string content_sha256{};
    bool approval_requested{false};
    DeliveryState state{DeliveryState::Prepared};
    std::string confirmation_token{};
    std::int64_t prepared_at{0};
    std::string prepared_by{};
    std::int64_t requested_at{0};
    std::string requested_by{};
    std::string request_idempotency_key{};
    std::int64_t accepted_at{0};
    std::string transport_reference{};
    std::string failure_reason{};
};

bool valid_email_address(const std::string& value) noexcept;
std::string delivery_confirmation_token(const DocumentDelivery& delivery);
void validate(const DocumentDelivery& delivery);
engine::Row to_row(const DocumentDelivery& delivery);
DocumentDelivery document_delivery_from_row(const engine::Row& row);

}  // namespace squiflow::modules::receivables
