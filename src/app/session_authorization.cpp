#include "app/session_authorization.hpp"

namespace squiflow::app {

namespace {

Result<void, DomainError> unauthorized(std::string field) {
    return Result<void, DomainError>::failure(
        DomainError{DomainErrorCode::Unauthorized, "session_authorization.error.unauthorized",
                    std::move(field)});
}

}  // namespace

Result<void, DomainError> authorize_session(const RequestContext& context,
                                            const engine::Session& live_session,
                                            std::uint64_t live_session_generation) {
    if (!live_session.is_signed_in() || live_session_generation == 0) {
        return unauthorized("session");
    }
    if (context.session_generation() != live_session_generation) {
        return unauthorized("session_generation");
    }
    if (context.user_id() != live_session.person) {
        return unauthorized("user_id");
    }
    for (const protocol::RightId right : context.permissions().granted()) {
        if (!live_session.rights.has(right)) {
            return unauthorized("permissions");
        }
    }
    return Result<void, DomainError>::success();
}

}  // namespace squiflow::app
