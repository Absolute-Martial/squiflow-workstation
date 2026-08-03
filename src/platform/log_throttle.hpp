#pragma once

// Stops one repeating event from eating the whole log.
//
// The failure this exists for is ordinary: a sync retries against a server
// that is refusing connections, and writes the same Error line thirty times a
// second. Within a minute the budget is spent, rotation has thrown away the
// morning, and the one line that explains why the shop's till behaved oddly at
// nine o'clock is gone. The flood destroys the evidence, not the disk.
//
// Two instruments, both off by default:
//
//   A minimum interval between identical records.
//   A count, so that every Nth occurrence is written even inside that interval.
//
// Identity is level, category and message together. Fields are deliberately
// excluded: fields are where the varying part lives (the attempt number, the
// customer), and including them would mean nothing ever looked identical and
// the throttle would never engage.
//
// Nothing is ever dropped silently. Every record that is held back is counted,
// and the count travels with the next record that is written, or is reported
// when the throttle is drained at a flush or at shutdown. A gap in the log
// always says how big it was.
//
// Fatal is never throttled. The last thing an application says before it dies
// is not a candidate for rate limiting.
//
// This class holds no lock. `Logger` owns it and serialises access, and it is
// usable on its own in a test with nothing but a number for the time.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "platform/log_record.hpp"

namespace squiflow::platform {

// How many distinct repeating events are watched at once. A shop machine has a
// handful of things that can go wrong in a loop; the bound exists so that a
// program logging a million distinct messages cannot turn the throttle into an
// unbounded cache of every line it has ever written.
inline constexpr std::size_t kMaxThrottledEvents = 64;

// The default when throttling is on but no interval was chosen. One second is
// long enough to collapse a tight retry loop and short enough that a human
// watching the file still sees the event moving.
inline constexpr std::int64_t kDefaultThrottleIntervalMilliseconds = 1000;

struct LogThrottlePolicy {
    // Zero disables the interval rule. Negative values are treated as zero,
    // because a clock that moved backwards must not switch throttling on.
    std::int64_t minimum_interval_milliseconds = 0;

    // Write every Nth occurrence even while the interval says be quiet. Zero
    // and one both mean "no count rule"; one would mean every occurrence,
    // which is the same as no rule at all.
    std::uint32_t every_nth = 0;

    bool engaged() const {
        return minimum_interval_milliseconds > 0 || every_nth > 1;
    }
};

// A record the throttle held back, reported later so the gap is never silent.
struct RepeatSummary {
    LogLevel level = LogLevel::Info;
    std::string category;
    std::string message;
    std::uint64_t suppressed = 0;
};

struct ThrottleDecision {
    // False means the caller must not write this record.
    bool emit = false;

    // How many identical records were held back since the last one written.
    // Attached to the emitted record as a field, so a reader can see that the
    // single line in front of them stands for many.
    std::uint64_t suppressed_since_last = 0;

    // Set when watching this event forced an older one out of the table and
    // that older one still had held-back records to account for. The caller
    // writes this summary before the record it asked about.
    std::optional<RepeatSummary> evicted;
};

struct ThrottleCounters {
    std::uint64_t considered = 0;
    std::uint64_t held_back = 0;
    std::uint64_t summaries_reported = 0;
    std::uint64_t evictions = 0;
};

class LogThrottle {
public:
    explicit LogThrottle(LogThrottlePolicy policy = {});

    void set_policy(const LogThrottlePolicy& policy);
    LogThrottlePolicy policy() const;

    // The question asked of every record before it is formatted.
    //
    // `now_milliseconds` comes from the injected clock and is allowed to move
    // backwards; a backwards jump releases the throttle for that event rather
    // than silencing it until the clock catches up, because being too talkative
    // after a time sync is a nuisance and being silent is a lost afternoon.
    ThrottleDecision consider(LogLevel level, std::string_view category,
                              std::string_view message,
                              std::int64_t now_milliseconds);

    // Everything still held back, reported and forgotten. Called at a flush
    // and at shutdown so that a run never ends with an unreported gap.
    std::vector<RepeatSummary> drain();

    std::size_t watched_event_count() const;

    ThrottleCounters counters() const;

private:
    struct Event {
        LogLevel level = LogLevel::Info;
        std::string category;
        std::string message;
        std::int64_t last_emitted_milliseconds = 0;
        std::int64_t last_seen_milliseconds = 0;
        std::uint64_t suppressed = 0;
        // Occurrences since the last one written, used by the count rule.
        std::uint32_t since_emitted = 0;
    };

    Event* find(LogLevel level, std::string_view category,
                std::string_view message);

    // Returns the summary owed by whichever event was pushed out, if any.
    std::optional<RepeatSummary> make_room();

    LogThrottlePolicy policy_;
    std::vector<Event> events_;
    ThrottleCounters counters_;
};

}  // namespace squiflow::platform
