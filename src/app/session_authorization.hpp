#pragma once

// The one check every authenticated entry point performs before doing
// anything else: does the context a screen captured when it last loaded
// still describe the session that is actually live right now?
//
// A person can sign out, sign back in as someone else, or lose a right
// entirely between the moment a screen captures its context and the moment
// it acts on that context. Command dispatch and record/list reads used to
// each answer this question with their own copy of the same four checks;
// this is where that logic lives once, so a read and a write can never
// silently disagree about what "still authorized" means.

#include "app/contracts/domain_error.hpp"
#include "app/contracts/request_context.hpp"
#include "app/contracts/result.hpp"
#include "engine/identity/session.hpp"

#include <cstdint>

namespace squiflow::app {

Result<void, DomainError> authorize_session(const RequestContext& context,
                                            const engine::Session& live_session,
                                            std::uint64_t live_session_generation);

}  // namespace squiflow::app
