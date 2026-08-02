#pragma once

#include <cstdint>
#include <string_view>

namespace squiflow::protocol {

// What happens to an operation's effect.
enum class OperationClass : std::uint8_t {
    // Never leaves the machine. Printing a receipt, scanning a folder.
    LocalOnly,
    // Written locally, queued in the outbox, applied on the server later.
    Synchronizable,
    // Must reach the server as it happens. Anything where a local decision
    // could be wrong by the time it arrives.
    OnlineRequired,
};

// Whether a person may do it with no connection.
enum class OfflineRule : std::uint8_t {
    OfflineAllowed,
    OnlineOnly,
};

std::string_view to_string(OperationClass value) noexcept;
std::string_view to_string(OfflineRule value) noexcept;

}  // namespace squiflow::protocol
