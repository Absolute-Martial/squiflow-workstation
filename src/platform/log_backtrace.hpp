#pragma once

// Keeps the detail nobody asked for, in case it turns out to be needed.
//
// The dilemma this resolves is the ordinary one on a shop machine. Run at
// Debug and the file fills with routine chatter, the budget is spent, and the
// interesting hour is rotated away. Run at Info, which is what actually
// happens, and when something finally fails the twenty lines that would have
// explained it were never written.
//
// So the records the level filter rejects are not discarded immediately. They
// are held in a small ring, overwriting oldest first, and thrown away unread
// in the normal case. When a record at or above the trigger level is written,
// the ring is released ahead of it: the failure arrives with the run-up that
// preceded it, and the file still costs almost nothing while nothing is wrong.
//
// The ring is bounded by count, and every record in it was already bounded in
// length by the formatter's limits, so the memory this can hold is bounded in
// both directions. Overwritten records are counted, never silently forgotten.
//
// This class holds no lock. `Logger` owns it and serialises access, and it is
// usable on its own in a test.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "platform/log_record.hpp"

namespace squiflow::platform {

// The largest run-up worth keeping. Beyond this the reader is no longer being
// helped, and the memory held while nothing is wrong stops being negligible.
inline constexpr std::size_t kMaxBacktraceCapacity = 256;

// Enough to carry the immediate run-up to a failure without being a burden.
inline constexpr std::size_t kDefaultBacktraceCapacity = 32;

struct LogBacktracePolicy {
    // Off by default. Holding records costs memory, and a feature that costs
    // memory is switched on deliberately.
    bool enabled = false;

    // Clamped into [1, kMaxBacktraceCapacity] when the policy is applied. A
    // capacity of zero would mean "enabled but keeps nothing", which is a
    // configuration mistake rather than an intention.
    std::size_t capacity = kDefaultBacktraceCapacity;

    // A record written at or above this level releases the ring ahead of
    // itself.
    LogLevel trigger = LogLevel::Error;
};

struct LogBacktraceCounters {
    std::uint64_t remembered = 0;
    // Records pushed out of the ring by newer ones before any failure asked
    // for them. Expected in normal running; useful when judging whether the
    // capacity is too small to reach the cause.
    std::uint64_t overwritten = 0;
    std::uint64_t released = 0;
};

class LogBacktrace {
public:
    explicit LogBacktrace(LogBacktracePolicy policy = {});

    // Applying a policy keeps the newest records that still fit. Switching the
    // feature off discards what is held: those records were rejected by the
    // level filter, and releasing them after the operator asked for silence
    // would be the opposite of what was asked.
    void set_policy(const LogBacktracePolicy& policy);
    LogBacktracePolicy policy() const;

    bool enabled() const;

    // Whether a record at this level should release the ring.
    bool triggers_on(LogLevel level) const;

    // Offered every record the level filter rejected. Ignored while disabled.
    void remember(const LogRecord& record);

    // The held records, oldest first, and the ring is emptied.
    std::vector<LogRecord> take();

    std::size_t held() const;

    // Forget without reporting. Used when the run-up is no longer relevant.
    void clear();

    LogBacktraceCounters counters() const;

private:
    LogBacktracePolicy policy_;

    // A plain circular buffer. `next_` is where the following record goes;
    // `held_` is how many of the slots are in use, which is less than the
    // capacity only until the ring has wrapped for the first time.
    std::vector<LogRecord> ring_;
    std::size_t next_ = 0;
    std::size_t held_ = 0;

    LogBacktraceCounters counters_;
};

}  // namespace squiflow::platform
