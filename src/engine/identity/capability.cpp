#include "engine/identity/capability.hpp"

namespace squiflow::engine {

std::string_view to_string(ConnectionState state) noexcept {
    switch (state) {
        case ConnectionState::Online:
            return "online";
        case ConnectionState::Metered:
            return "metered";
        case ConnectionState::Weak:
            return "weak";
        case ConnectionState::Offline:
            return "offline";
    }
    return "?";
}

Decision may_run(protocol::OperationId operation, const Session& session,
                 ConnectionState connection,
                 const protocol::Activation& activation) {
    const protocol::OperationInfo& info = protocol::operation(operation);

    if (!session.is_signed_in()) {
        return {false, DenialReason::NotSignedIn, "Nobody is signed in."};
    }

    // Module activation first. A switched-off module should look absent, not
    // forbidden: telling someone they lack permission for something the shop
    // has turned off sends them to ask for a right they do not need.
    if (!activation.is_active(info.module)) {
        return {false, DenialReason::ModuleInactive,
                "This shop does not use " +
                    std::string(protocol::module_name(info.module)) + "."};
    }

    if (!session.rights.has(info.right)) {
        return {false, DenialReason::NoRight,
                "You do not have permission to do this."};
    }

    const bool offline = connection == ConnectionState::Offline;
    if (offline) {
        if (info.offline == protocol::OfflineRule::OnlineOnly) {
            return {false, DenialReason::RequiresConnection,
                    "This needs the shop server, and it cannot be reached "
                    "right now."};
        }

        // A person who is not the owner works read-only while disconnected,
        // with the counter-sale exception so the counter never stops.
        if (!session.is_owner && !protocol::staff_offline_exception(operation)) {
            return {false, DenialReason::ReadOnlyOffline,
                    "While the connection is down you can take payments and "
                    "print, but not change records."};
        }
    }

    // Metered and weak are not refusals. They change how sync behaves, not
    // what a person is allowed to do; refusing work because a connection is
    // slow would be the system serving itself.
    return {true, DenialReason::None, {}};
}

bool may_ever_run(protocol::OperationId operation, const Session& session,
                  const protocol::Activation& activation) {
    const Decision decision =
        may_run(operation, session, ConnectionState::Online, activation);
    return decision.allowed;
}

}  // namespace squiflow::engine
