#include "platform/log_backtrace.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace squiflow::platform {
namespace {

std::size_t clamped_capacity(std::size_t requested) {
    if (requested == 0) {
        return 1;
    }
    return std::min(requested, kMaxBacktraceCapacity);
}

}  // namespace

LogBacktrace::LogBacktrace(LogBacktracePolicy policy) { set_policy(policy); }

void LogBacktrace::set_policy(const LogBacktracePolicy& policy) {
    LogBacktracePolicy applied = policy;
    applied.capacity = clamped_capacity(applied.capacity);

    if (!applied.enabled) {
        // The operator asked for silence. Records held here were rejected by
        // the level filter, so releasing them later would contradict that.
        policy_ = applied;
        ring_.clear();
        next_ = 0;
        held_ = 0;
        return;
    }

    // Take what is held, in order, then keep the newest that still fit. Losing
    // the oldest is what the ring does anyway; losing the newest would throw
    // away the records closest to any failure about to happen.
    //
    // Re-seating goes through `remember`, so there is one definition of what
    // putting a record into the ring means. That path adjusts the counters,
    // and `take` counted these records as released when they were not, so the
    // counters are restored afterwards: moving records between two shapes of
    // the same ring is not an event worth reporting.
    const LogBacktraceCounters unchanged = counters_;

    std::vector<LogRecord> kept = take();
    if (kept.size() > applied.capacity) {
        const std::size_t surplus = kept.size() - applied.capacity;
        kept.erase(
            kept.begin(),
            std::next(kept.begin(), static_cast<std::ptrdiff_t>(surplus)));
    }

    policy_ = applied;
    ring_.assign(policy_.capacity, LogRecord{});
    next_ = 0;
    held_ = 0;

    for (const LogRecord& record : kept) {
        remember(record);
    }

    counters_ = unchanged;
}

LogBacktracePolicy LogBacktrace::policy() const { return policy_; }

bool LogBacktrace::enabled() const { return policy_.enabled; }

bool LogBacktrace::triggers_on(LogLevel level) const {
    return policy_.enabled && static_cast<std::uint8_t>(level) >=
                                  static_cast<std::uint8_t>(policy_.trigger);
}

void LogBacktrace::remember(const LogRecord& record) {
    if (!policy_.enabled) {
        return;
    }

    if (ring_.size() != policy_.capacity) {
        // Only reachable if the ring was never sized, which `set_policy` does.
        // Sizing here as well means a future edit cannot turn a missed call
        // into an out-of-range write.
        ring_.assign(policy_.capacity, LogRecord{});
        next_ = 0;
        held_ = 0;
    }

    if (held_ == ring_.size()) {
        ++counters_.overwritten;
    } else {
        ++held_;
    }

    ring_[next_] = record;
    next_ = (next_ + 1) % ring_.size();
    ++counters_.remembered;
}

std::vector<LogRecord> LogBacktrace::take() {
    std::vector<LogRecord> released;
    if (held_ == 0) {
        return released;
    }

    released.reserve(held_);

    // The oldest record sits `held_` slots behind the write position, which is
    // the start of the buffer until the ring has wrapped.
    const std::size_t size = ring_.size();
    const std::size_t oldest = (next_ + size - held_) % size;
    for (std::size_t step = 0; step < held_; ++step) {
        released.push_back(std::move(ring_[(oldest + step) % size]));
    }

    counters_.released += static_cast<std::uint64_t>(released.size());

    ring_.assign(size, LogRecord{});
    next_ = 0;
    held_ = 0;
    return released;
}

std::size_t LogBacktrace::held() const { return held_; }

void LogBacktrace::clear() {
    ring_.assign(ring_.size(), LogRecord{});
    next_ = 0;
    held_ = 0;
}

LogBacktraceCounters LogBacktrace::counters() const { return counters_; }

}  // namespace squiflow::platform
