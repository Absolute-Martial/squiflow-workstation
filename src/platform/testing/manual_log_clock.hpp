#pragma once

// A clock a test drives by hand.
//
// Timestamps are asserted exactly, so the clock must be exact. It also moves
// backwards on request, because a shop machine that syncs its time really does
// that and nothing in the log path is allowed to misbehave when it happens.

#include <cstdint>

#include "platform/log_clock.hpp"

namespace squiflow::platform::testing {

class ManualLogClock final : public LogClock {
public:
    explicit ManualLogClock(std::int64_t start_milliseconds = 0)
        : now_(start_milliseconds) {}

    std::int64_t now_milliseconds() const override { return now_; }

    void set(std::int64_t milliseconds) { now_ = milliseconds; }
    void advance(std::int64_t milliseconds) { now_ += milliseconds; }

private:
    std::int64_t now_;
};

}  // namespace squiflow::platform::testing
