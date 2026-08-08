#include "app/credential_command.hpp"

#include "engine/records/payload.hpp"
#include "platform/password_hash.hpp"

namespace squiflow::app {
namespace {

CredentialPayloadResult failure(CredentialPayloadFault fault,
                                std::string message_key,
                                std::string field = "password") {
    CredentialPayloadResult result;
    result.fault = fault;
    result.message_key = std::move(message_key);
    result.field = std::move(field);
    return result;
}

CredentialPayloadResult success(engine::Blob payload) {
    CredentialPayloadResult result;
    result.ok = true;
    result.payload = std::move(payload);
    return result;
}

}  // namespace

CredentialPayloadResult prepare_credential_payload(
    protocol::OperationId operation, const engine::Blob& payload) {
    const bool create = operation == protocol::OperationId::person_create;
    const bool update = operation == protocol::OperationId::person_update;
    if (!create && !update) {
        return success(payload);
    }

    engine::Row fields;
    try {
        fields = engine::decode_payload(payload);
    } catch (const engine::PayloadError&) {
        return failure(CredentialPayloadFault::MalformedPayload,
                       "credential.error.malformed_payload", "payload");
    }

    // Hashes are an internal representation, not an input format. Refusing
    // them also prevents a caller from choosing deliberately weak parameters.
    if (fields.has("password_hash")) {
        return failure(CredentialPayloadFault::HashInjection,
                       "credential.error.hash_not_accepted");
    }
    if (!fields.has("password")) {
        if (update) {
            return success(payload);
        }
        return failure(CredentialPayloadFault::MissingPassword,
                       "credential.error.password_required");
    }

    const std::string* plaintext = fields.get("password").as_text();
    if (plaintext == nullptr) {
        return failure(CredentialPayloadFault::InvalidPassword,
                       "credential.error.password_invalid");
    }
    const platform::PasswordHashResult hashed = platform::hash_password(*plaintext);
    if (!hashed.ok) {
        const CredentialPayloadFault fault =
            hashed.fault == platform::PasswordHashFault::HashingFailed
                ? CredentialPayloadFault::HashingFailed
                : CredentialPayloadFault::InvalidPassword;
        return failure(fault, fault == CredentialPayloadFault::HashingFailed
                                  ? "credential.error.hashing_failed"
                                  : "credential.error.password_invalid");
    }

    fields.erase("password");
    fields.set("password_hash", engine::Value::text(hashed.hash));
    return success(engine::encode_payload(fields));
}

}  // namespace squiflow::app
