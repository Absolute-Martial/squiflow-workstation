#include "platform/log_throttle.hpp"

#include <utility>

namespace squiflow::platform {
namespace {

// A held-back count is only worth reporting if there is something in it.
bool owes_a_summary(std::uint64_t suppressed) { return suppressed > 0; }

}  // namespace

LogThrottle::LogThrottle(LogThrottlePolicy policy) { set_policy(policy); }

void LogThrottle::set_policy(const LogThrottlePolicy& policy) {
    policy_ = policy;
    // A negative interval would otherwise compare as "engaged" in some future
    // edit and silence the log. Normalise once, here, so nothing downstream
    // has to remember.
    if (policy_.minimum_interval_milliseconds < 0) {
        policy_.minimum_interval_milliseconds = 0;
    }
}

LogThrottlePolicy LogThrottle::policy() const { return policy_; }

LogThrottle::Event* LogThrottle::find(LogLevel level, std::string_view category,
                                      std::string_view message) {
    for (Event& event : events_) {
        if (event.level == level && event.category == category &&
            event.message == message) {
            return &event;
        }
    }
    return nullptr;
}

std::optional<RepeatSummary> LogThrottle::make_room() {
    if (events_.size() < kMaxThrottledEvents) {
        return std::nullopt;
    }

    // Push out whichever event has been quiet the longest. It is the one least
    // likely to still be repeating, and therefore the one whose eviction costs
    // the least accuracy.
    auto oldest = events_.begin();
    for (auto candidate = events_.begin(); candidate != events_.end();
         ++candidate) {
        if (candidate->last_seen_milliseconds < oldest->last_seen_milliseconds) {
            oldest = candidate;
        }
    }

    std::optional<RepeatSummary> owed;
    if (owes_a_summary(oldest->suppressed)) {
        owed = RepeatSummary{oldest->level, oldest->category, oldest->message,
                             oldest->suppressed};
        ++counters_.summaries_reported;
    }

    events_.erase(oldest);
    ++counters_.evictions;
    return owed;
}

ThrottleDecision LogThrottle::consider(LogLevel level,
                                       std::string_view category,
                                       std::string_view message,
                                       std::int64_t now_milliseconds) {
    ++counters_.considered;

    ThrottleDecision decision;

    // Off, or the one level that is never held back. Nothing is remembered in
    // this case, so the table cannot grow while the feature is unused.
    if (!policy_.engaged() || level == LogLevel::Fatal) {
        decision.emit = true;
        return decision;
    }

    Event* event = find(level, category, message);
    if (event == nullptr) {
        decision.evicted = make_room();

        Event fresh;
        fresh.level = level;
        fresh.category = std::string(category);
        fresh.message = std::string(message);
        fresh.last_emitted_milliseconds = now_milliseconds;
        fresh.last_seen_milliseconds = now_milliseconds;
        events_.push_back(std::move(fresh));

        // The first sighting of anything is always written. A throttle that
        // swallowed the first occurrence would delay the news of a new fault.
        decision.emit = true;
        return decision;
    }

    event->last_seen_milliseconds = now_milliseconds;
    ++event->since_emitted;

    const std::int64_t elapsed =
        now_milliseconds - event->last_emitted_milliseconds;

    // A clock that moved backwards produces a negative elapsed time. Treat it
    // as "long enough": talking too soon after a time sync is a nuisance,
    // staying silent until the clock catches up could cost an afternoon.
    const bool interval_passed =
        policy_.minimum_interval_milliseconds == 0 || elapsed < 0 ||
        elapsed >= policy_.minimum_interval_milliseconds;

    const bool count_reached =
        policy_.every_nth > 1 && event->since_emitted >= policy_.every_nth;

    if (!interval_passed && !count_reached) {
        ++event->suppressed;
        ++counters_.held_back;
        decision.emit = false;
        return decision;
    }

    decision.emit = true;
    decision.suppressed_since_last = event->suppressed;
    if (owes_a_summary(event->suppressed)) {
        ++counters_.summaries_reported;
    }

    event->suppressed = 0;
    event->since_emitted = 0;
    event->last_emitted_milliseconds = now_milliseconds;
    return decision;
}

std::vector<RepeatSummary> LogThrottle::drain() {
    std::vector<RepeatSummary> owed;
    for (Event& event : events_) {
        if (owes_a_summary(event.suppressed)) {
            owed.push_back(RepeatSummary{event.level, event.category,
                                         event.message, event.suppressed});
            event.suppressed = 0;
            ++counters_.summaries_reported;
        }
        event.since_emitted = 0;
    }
    return owed;
}

std::size_t LogThrottle::watched_event_count() const { return events_.size(); }

ThrottleCounters LogThrottle::counters() const { return counters_; }

}  // namespace squiflow::platform
