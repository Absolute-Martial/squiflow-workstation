#include "app/credential_command.hpp"

#include "engine/records/payload.hpp"
#include "platform/password_hash.hpp"
#include "support/check.hpp"

#include <string>

namespace {
using namespace squiflow;

engine::Blob person_payload(const engine::Value& password) {
    engine::Row fields;
    fields.set("display_name", engine::Value::text("Clerk"));
    fields.set("username", engine::Value::text("clerk"));
    fields.set("password", password);
    return engine::encode_payload(fields);
}

}  // namespace

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;

    t::section("non-person payloads pass through byte-for-byte");
    {
        const engine::Blob payload{1, 2, 3, 4};
        const auto prepared = app::prepare_credential_payload(
            protocol::OperationId::party_create, payload);
        t::check(prepared.ok, "an unrelated operation is accepted");
        t::check(prepared.payload == payload, "unrelated bytes are not decoded or rewritten");
    }

    t::section("person create replaces plaintext with a fresh Argon2id hash");
    {
        const auto prepared = app::prepare_credential_payload(
            protocol::OperationId::person_create,
            person_payload(engine::Value::text("correct horse battery staple")));
        t::check(prepared.ok, "a valid plaintext password is prepared");
        const engine::Row fields = engine::decode_payload(prepared.payload);
        t::check(!fields.has("password"), "plaintext is removed before the module boundary");
        const std::string encoded = fields.get("password_hash").text_or({});
        t::check(!encoded.empty(), "an encoded hash is present");
        t::check(platform::verify_password("correct horse battery staple", encoded),
                 "the encoded hash verifies the submitted password");
        t::check(fields.get("display_name").text_or({}) == "Clerk" &&
                     fields.get("username").text_or({}) == "clerk",
                 "non-secret person fields survive unchanged");
    }

    t::section("equal passwords receive independent salts");
    {
        const auto first = app::prepare_credential_payload(
            protocol::OperationId::person_create,
            person_payload(engine::Value::text("same passphrase")));
        const auto second = app::prepare_credential_payload(
            protocol::OperationId::person_create,
            person_payload(engine::Value::text("same passphrase")));
        const auto first_hash = engine::decode_payload(first.payload)
                                    .get("password_hash").text_or({});
        const auto second_hash = engine::decode_payload(second.payload)
                                     .get("password_hash").text_or({});
        t::check(first.ok && second.ok, "both preparations succeed");
        t::check(first_hash != second_hash, "a fresh salt is used for every write");
    }

    t::section("person update may omit a password without changing the stored hash");
    {
        engine::Row fields;
        fields.set("display_name", engine::Value::text("Updated name"));
        const engine::Blob payload = engine::encode_payload(fields);
        const auto prepared = app::prepare_credential_payload(
            protocol::OperationId::person_update, payload);
        t::check(prepared.ok && prepared.payload == payload,
                 "an update with no password passes through unchanged");
    }

    t::section("hash injection malformed payloads and invalid passwords fail closed");
    {
        engine::Row injected;
        injected.set("password_hash", engine::Value::text("weak-or-foreign-hash"));
        const auto hash_injection = app::prepare_credential_payload(
            protocol::OperationId::person_create, engine::encode_payload(injected));
        t::check(!hash_injection.ok &&
                     hash_injection.fault == app::CredentialPayloadFault::HashInjection,
                 "a caller cannot supply an encoded hash");

        const auto malformed = app::prepare_credential_payload(
            protocol::OperationId::person_create, engine::Blob{1, 2, 3});
        t::check(!malformed.ok &&
                     malformed.fault == app::CredentialPayloadFault::MalformedPayload,
                 "malformed bytes are refused");

        engine::Row missing;
        missing.set("username", engine::Value::text("clerk"));
        const auto absent = app::prepare_credential_payload(
            protocol::OperationId::person_create, engine::encode_payload(missing));
        t::check(!absent.ok &&
                     absent.fault == app::CredentialPayloadFault::MissingPassword,
                 "person create requires a password");

        const auto wrong_kind = app::prepare_credential_payload(
            protocol::OperationId::person_create,
            person_payload(engine::Value::integer(1234)));
        t::check(!wrong_kind.ok &&
                     wrong_kind.fault == app::CredentialPayloadFault::InvalidPassword,
                 "a non-text password is refused");

        const auto empty = app::prepare_credential_payload(
            protocol::OperationId::person_create,
            person_payload(engine::Value::text("")));
        t::check(!empty.ok && empty.fault == app::CredentialPayloadFault::InvalidPassword,
                 "an empty password is refused");

        const auto oversized = app::prepare_credential_payload(
            protocol::OperationId::person_create,
            person_payload(engine::Value::text(std::string(
                platform::kMaxPasswordBytes + 1, 'x'))));
        t::check(!oversized.ok &&
                     oversized.fault == app::CredentialPayloadFault::InvalidPassword,
                 "an oversized password is refused before hashing");
    }

    return t::report();
}
