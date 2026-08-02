#pragma once

// The time seam.
//
// Rotation, ordering, and every assertion about a timestamp depend on the
// clock, so the clock is injected. Tests drive time by hand; the application
// uses the system clock. Nothing here needs Qt.

#include <cstdint>

namespace squiflow::platform {

class LogClock {
public:
    virtual ~LogClock() = default;

    LogClock(const LogClock&) = delete;
    LogClock& operator=(const LogClock&) = delete;
    LogClock(LogClock&&) = delete;
    LogClock& operator=(LogClock&&) = delete;

    // Milliseconds since the Unix epoch, UTC. Never throws.
    virtual std::int64_t now_milliseconds() const = 0;

protected:
    LogClock() = default;
};

// The wall clock. It can move backwards when the machine syncs time or a user
// edits it; the log records what it said, and rotation never trusts it for
// ordering, which is why generations are numbered rather than dated.
class SystemLogClock final : public LogClock {
public:
    std::int64_t now_milliseconds() const override;
};

}  // namespace squiflow::platform
