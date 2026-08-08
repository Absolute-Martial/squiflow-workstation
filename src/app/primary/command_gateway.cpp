#include "app/primary/command_gateway.hpp"
#include "app/credential_command.hpp"

#include <algorithm>

namespace squiflow::app::primary {

namespace {

Result<CommandAck, DomainError> error(DomainErrorCode code, std::string message_key,
                                      std::string field) {
    return Result<CommandAck, DomainError>::failure(
        DomainError{code, std::move(message_key), std::move(field)});
}

Result<CommandAck, DomainError> invalid(std::string field) {
    return error(DomainErrorCode::ValidationFailed, "command_gateway.error.invalid_request",
                 std::move(field));
}

Result<CommandAck, DomainError> unauthorized(std::string field) {
    return error(DomainErrorCode::Unauthorized, "command_gateway.error.unauthorized",
                 std::move(field));
}

bool canonical_record_id(const std::string& text) {
    const engine::RecordId id = engine::record_id_from_string(text);
    return id.is_valid() && engine::to_string(id) == text;
}

bool safe_idempotency_key(const std::string& key) {
    if (key.empty() || key.size() > CommandGateway::kMaximumIdempotencyKeyBytes) {
        return false;
    }
    return std::all_of(key.begin(), key.end(), [](char value) {
        const bool alpha = (value >= 'a' && value <= 'z') || (value >= 'A' && value <= 'Z');
        const bool digit = value >= '0' && value <= '9';
        const bool punctuation = value == '-' || value == '_' || value == '.' || value == ':';
        return alpha || digit || punctuation;
    });
}

}  // namespace

Result<CommandAck, DomainError> CommandGateway::dispatch(
    const RequestContext& context, const engine::Session& live_session,
    std::uint64_t live_session_generation, engine::ConnectionState connection,
    const CommandRequest& request) const {
    if (auto authorized = authorize_session(context, live_session, live_session_generation);
        !authorized) {
        return Result<CommandAck, DomainError>::failure(authorized.error());
    }

    if (!protocol::is_valid(request.operation) || !registry_.is_command(request.operation)) {
        return invalid("operation");
    }

    if (!request.record_id.empty() && !canonical_record_id(request.record_id)) {
        return invalid("record_id");
    }

    const bool synchronizable = protocol::operation(request.operation).sync_class ==
                                protocol::OperationClass::Synchronizable;
    if (synchronizable) {
        if (request.record_id.empty()) {
            return invalid("record_id");
        }
        if (!safe_idempotency_key(request.idempotency_key)) {
            return invalid("idempotency_key");
        }
    } else if (!request.idempotency_key.empty()) {
        return invalid("idempotency_key");
    }

    auto prepared = prepare_credential_payload(request.operation, request.payload);
    if (!prepared.ok) {
        return error(prepared.fault == CredentialPayloadFault::HashingFailed
                         ? DomainErrorCode::Conflict
                         : DomainErrorCode::ValidationFailed,
                     std::move(prepared.message_key), std::move(prepared.field));
    }

    modules::Call call;
    call.operation = request.operation;
    call.record_id = request.record_id;
    call.payload = std::move(prepared.payload);
    call.idempotency_key = request.idempotency_key;

    modules::Outcome outcome;
    try {
        outcome = registry_.run(database_, call, live_session, connection);
    } catch (const modules::RegistryError&) {
        // Defensive: is_command() above should make this unreachable in a
        // correctly wired application. If it is ever wrong, a framework
        // programming error becomes an ordinary refusal instead of an
        // exception escaping into shell code that did not ask for one.
        return invalid("operation");
    }

    if (!outcome.ok) {
        if (outcome.reason != engine::DenialReason::None) {
            return unauthorized("operation");
        }
        return error(DomainErrorCode::Conflict, "command_gateway.error.refused", "operation");
    }

    return Result<CommandAck, DomainError>::success(
        CommandAck{outcome.queued, outcome.replayed});
}

}  // namespace squiflow::app::primary
