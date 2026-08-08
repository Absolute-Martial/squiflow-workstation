#pragma once

// The write-side credential boundary. UI and shell code submit a plaintext
// `password`; administration receives only `password_hash`. Encoded hashes are
// never accepted from an untrusted command payload, so callers cannot bypass
// the application-selected Argon2id cost parameters.

#include "engine/storage/store.hpp"

#include <squiflow/protocol/operation_table.hpp>

#include <cstdint>
#include <string>

namespace squiflow::app {

enum class CredentialPayloadFault : std::uint8_t {
    None,
    MalformedPayload,
    HashInjection,
    MissingPassword,
    InvalidPassword,
    HashingFailed,
};

struct CredentialPayloadResult {
    bool ok{false};
    engine::Blob payload{};
    CredentialPayloadFault fault{CredentialPayloadFault::None};
    std::string message_key{};
    std::string field{};
};

// Non-person operations pass through byte-for-byte. person_create requires a
// password; person_update hashes it when supplied and otherwise leaves the
// existing hash untouched. Plaintext is erased before the payload leaves this
// function.
CredentialPayloadResult prepare_credential_payload(
    protocol::OperationId operation, const engine::Blob& payload);

}  // namespace squiflow::app
