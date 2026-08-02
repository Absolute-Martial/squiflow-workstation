#pragma once

#include <string>

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/operation_table.hpp>

#include "engine/identity/session.hpp"

namespace squiflow::engine {

enum class DenialReason : std::uint8_t {
    None,
    NotSignedIn,
    ModuleInactive,
    NoRight,
    RequiresConnection,
    ReadOnlyOffline,
};

struct Decision {
    bool allowed = false;
    DenialReason reason = DenialReason::None;
    // Written for the person at the counter, not for a log file. A refusal
    // nobody understands gets worked around, and the workaround is worse than
    // whatever the rule was protecting.
    std::string explanation;
};

// The one function that answers whether an operation may run.
//
// Every screen, every keyboard shortcut, every sync handler and every workflow
// goes through here. Scattering these conditions across screens is how a
// system ends up permitting through one path what it forbids through another.
Decision may_run(protocol::OperationId operation, const Session& session,
                 ConnectionState connection,
                 const protocol::Activation& activation);

// Convenience for screens: can this person ever do it, ignoring the connection?
// Used to hide a button entirely rather than show one that always refuses.
bool may_ever_run(protocol::OperationId operation, const Session& session,
                  const protocol::Activation& activation);

}  // namespace squiflow::engine
