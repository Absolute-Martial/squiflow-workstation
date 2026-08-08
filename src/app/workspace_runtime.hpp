#pragma once

// The one authenticated surface the shell talks to once a session exists.
//
// RealStartupRuntime hands the shell a Registry, a Database, and a session.
// AuthenticatedWorkspace is where those get composed exactly once: it owns
// the read services and the command gateway built on that Registry and
// Database, and it is the only place a sign-in, a sign-out, a lost right, or
// a connection state change gets applied before a read or a write is let
// through.

#include "app/contracts/request_context.hpp"
#include "app/contracts/result.hpp"
#include "app/primary/command_gateway.hpp"
#include "app/primary/local_primary_query.hpp"
#include "app/primary/local_record_query.hpp"
#include "app/primary/primary_page_service.hpp"
#include "app/primary/primary_query.hpp"
#include "app/primary/record_page_service.hpp"
#include "app/primary/record_query.hpp"
#include "engine/identity/session.hpp"
#include "engine/storage/database.hpp"
#include "modules/registry.hpp"

#include <cstdint>
#include <string_view>

namespace squiflow::app {

class AuthenticatedWorkspace final {
  public:
    AuthenticatedWorkspace(modules::Registry& registry, engine::Database& database);

    // Never zero, and never repeated for the lifetime of this workspace,
    // including across a sign-out followed by a new sign-in. A context
    // captured under a previous generation can therefore never be mistaken
    // for one captured under this one, even when the same person signs back
    // in.
    std::uint64_t sign_in(engine::Session session) noexcept;
    void sign_out() noexcept;
    void set_connection_state(engine::ConnectionState state) noexcept;

    bool signed_in() const noexcept { return session_.is_signed_in(); }
    std::uint64_t session_generation() const noexcept { return generation_; }
    engine::ConnectionState connection_state() const noexcept { return connection_; }
    const engine::Session& current_session() const noexcept { return session_; }

    Result<primary::ListPage, DomainError> list(
        const RequestContext& context, const protocol::Activation& activation,
        primary::PageKind kind, const primary::ListRequest& request) const;

    Result<primary::RecordSnapshot, DomainError> record(
        const RequestContext& context, const protocol::Activation& activation,
        primary::PageKind kind, std::string_view stable_id) const;

    Result<primary::CommandAck, DomainError> dispatch(
        const RequestContext& context, const primary::CommandRequest& request) const;

  private:
    engine::Database& database_;
    primary::LocalPrimaryQuery primary_query_;
    primary::LocalRecordQuery record_query_;
    primary::PrimaryPageService primary_page_service_;
    primary::RecordPageService record_page_service_;
    primary::CommandGateway command_gateway_;

    engine::Session session_{};
    std::uint64_t generation_{0};
    engine::ConnectionState connection_{engine::ConnectionState::Offline};
};

}  // namespace squiflow::app
