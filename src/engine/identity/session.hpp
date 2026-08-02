#pragma once

#include <string>

#include "engine/identity/rights_set.hpp"
#include "engine/records/identity.hpp"

namespace squiflow::engine {

// How the machine sees the connection right now. Reported by the operating
// system, never by pinging the server: a ping tells you about one server at
// one moment and costs a round trip to say so.
enum class ConnectionState : std::uint8_t {
    Online,
    Metered,  // connected, but every byte is being paid for
    Weak,     // connected in name; round trips are too slow to rely on
    Offline,
};

std::string_view to_string(ConnectionState state) noexcept;

// The signed-in person on this device.
//
// is_owner is not a role in disguise. It marks exactly two things that were
// decided: the owner's version wins a sync conflict, and a person who is not
// the owner works read-only while disconnected.
struct Session {
    PersonId person;
    DeviceId device;
    std::string display_name;
    bool is_owner = false;
    RightsSet rights;

    bool is_signed_in() const noexcept { return person.is_valid(); }
};

}  // namespace squiflow::engine
