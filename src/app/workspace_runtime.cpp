#include "app/workspace_runtime.hpp"

#include "app/session_authorization.hpp"

#include <utility>

namespace squiflow::app {

AuthenticatedWorkspace::AuthenticatedWorkspace(modules::Registry& registry,
                                               engine::Database& database)
    : database_(database),
      primary_query_(database_),
      record_query_(database_),
      primary_page_service_(primary_query_),
      record_page_service_(record_query_),
      command_gateway_(registry, database_) {}

std::uint64_t AuthenticatedWorkspace::sign_in(engine::Session session) noexcept {
    session_ = std::move(session);
    do {
        ++generation_;
    } while (generation_ == 0);
    return generation_;
}

void AuthenticatedWorkspace::sign_out() noexcept {
    session_ = engine::Session{};
    generation_ = 0;
}

void AuthenticatedWorkspace::set_connection_state(engine::ConnectionState state) noexcept {
    connection_ = state;
}

Result<primary::ListPage, DomainError> AuthenticatedWorkspace::list(
    const RequestContext& context, const protocol::Activation& activation,
    primary::PageKind kind, const primary::ListRequest& request) const {
    if (auto authorized = authorize_session(context, session_, generation_); !authorized) {
        return Result<primary::ListPage, DomainError>::failure(authorized.error());
    }
    return primary_page_service_.list(context, activation, kind, request);
}

Result<primary::RecordSnapshot, DomainError> AuthenticatedWorkspace::record(
    const RequestContext& context, const protocol::Activation& activation,
    primary::PageKind kind, std::string_view stable_id) const {
    if (auto authorized = authorize_session(context, session_, generation_); !authorized) {
        return Result<primary::RecordSnapshot, DomainError>::failure(authorized.error());
    }
    return record_page_service_.load(context, activation, kind, stable_id);
}

Result<primary::CommandAck, DomainError> AuthenticatedWorkspace::dispatch(
    const RequestContext& context, const primary::CommandRequest& request) const {
    return command_gateway_.dispatch(context, session_, generation_, connection_, request);
}

}  // namespace squiflow::app
