#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace squiflow::engine {

// A record identifier is generated on the device that creates the record.
// That is what lets a person create things with no connection: nothing waits
// for a server to hand out a key. 128 bits, rendered as 32 hex characters.
struct RecordId {
    std::uint64_t high = 0;
    std::uint64_t low = 0;

    constexpr bool is_valid() const noexcept { return high != 0 || low != 0; }

    friend constexpr bool operator==(const RecordId&, const RecordId&) = default;
};

std::string to_string(const RecordId& id);

// Returns an invalid identifier when the text is not 32 hex characters, rather
// than throwing or guessing. Callers check is_valid().
RecordId record_id_from_string(std::string_view text);

using PersonId = RecordId;
using DeviceId = RecordId;

// Milliseconds since the epoch, always UTC. Local time exists for display and
// nowhere else; a shop that syncs between machines cannot afford two clocks
// with different ideas of what time it is.
struct Timestamp {
    std::int64_t ms = 0;

    constexpr bool is_set() const noexcept { return ms != 0; }

    friend constexpr auto operator<=>(const Timestamp&, const Timestamp&) = default;
    friend constexpr bool operator==(const Timestamp&, const Timestamp&) = default;
};

// Incremented by the server on every accepted change. Conflict detection is a
// version comparison, never a timestamp comparison, because two devices'
// clocks disagree and the disagreement is invisible.
struct RecordVersion {
    std::uint64_t value = 0;

    friend constexpr auto operator<=>(const RecordVersion&, const RecordVersion&) = default;
    friend constexpr bool operator==(const RecordVersion&, const RecordVersion&) = default;
};

// Carried by every synchronized record. Deletion is a flag, never a removed
// row: the other device has to learn about the deletion somehow.
struct RecordHeader {
    RecordId id;
    Timestamp created_at;
    Timestamp updated_at;
    PersonId created_by;
    PersonId updated_by;
    DeviceId updated_on;
    RecordVersion version;
    bool deleted = false;
};

}  // namespace squiflow::engine
