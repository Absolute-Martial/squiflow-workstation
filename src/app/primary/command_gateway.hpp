#pragma once

// The one door for a command coming from the shell.
//
// A screen holds a RequestContext captured when its data last loaded, and a
// session that keeps living underneath it. Between the two, a person can sign
// out, lose a right, or have the shop switch a module off, and the screen has
// no way to know until it tries something. This gateway is where that gap is
// closed: every command re-checks the captured context against the session
// that is actually live right now, before the registry ever sees a Call.
//
// It also stands between the shell and modules::RegistryError. That exception
// exists to catch a programming mistake found at startup - an operation
// nobody handles - and startup already refuses to run with one of those
// unhandled. A command gateway used correctly should therefore never see one
// escape registry_.run(), but "should never" is not "cannot": is_command()
// and this gateway's own validation are kept in one place, and if they are
// ever wrong in a way that lets an unhandled operation through, the mismatch
// becomes an ordinary refusal instead of an uncaught exception taking down
// the shell.

#include "app/contracts/request_context.hpp"
#include "app/session_authorization.hpp"
#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/storage/database.hpp"
#include "modules/registry.hpp"

#include <squiflow/protocol/operation_table.hpp>

#include <cstddef>
#include <cstdint>
#include <string>

namespace squiflow::app::primary {

struct CommandRequest final {
    protocol::OperationId operation{};
    std::string record_id{};
    engine::Blob payload{};
    std::string idempotency_key{};
};

struct CommandAck final {
    bool queued{false};
    bool replayed{false};
};

class CommandGateway final {
  public:
    // Long enough for a UUID or a device-scoped sequence, short enough that
    // nothing built the outbox around a key that costs real storage.
    static constexpr std::size_t kMaximumIdempotencyKeyBytes = 128;

    CommandGateway(modules::Registry& registry, engine::Database& database)
        : registry_(registry), database_(database) {}

    // live_session_generation is the generation the shell currently considers
    // live, and live_session is the session that goes with it. Both are
    // compared against the ones frozen inside context; any mismatch refuses
    // the command before a Call is built.
    Result<CommandAck, DomainError> dispatch(
        const RequestContext& context, const engine::Session& live_session,
        std::uint64_t live_session_generation, engine::ConnectionState connection,
        const CommandRequest& request) const;

  private:
    modules::Registry& registry_;
    engine::Database& database_;
};

}  // namespace squiflow::app::primary
