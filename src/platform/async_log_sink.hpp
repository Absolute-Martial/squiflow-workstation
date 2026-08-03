#pragma once

// Takes the disk off the counter clerk's thread.
//
// Writing a log line is a disk write, and a disk write on a shop machine can
// block for a surprisingly long time: a spinning disk waking up, an antivirus
// scanner deciding to inspect the file, a network drive somebody redirected
// the data folder onto. None of that should be felt by whoever is standing at
// the counter waiting for an invoice to print.
//
// So this sink hands lines to a single writer thread through a bounded queue
// and returns immediately. It wraps another sink, which is the one that
// actually touches the disk, and that wrapped sink is only ever used from the
// writer thread: the sinks underneath do not have to be thread-safe.
//
// Three decisions worth stating plainly, because they are the ones that hurt
// if they are wrong:
//
//  - The queue is bounded. An unbounded queue turns a logging storm into an
//    out-of-memory crash, which is a far worse failure than a gap in a log.
//
//  - When the queue is full the oldest line is dropped, not the newest. Under
//    pressure the recent lines are the ones that explain what is happening
//    now. Dropping the newest would throw away exactly the evidence somebody
//    is waiting for.
//
//  - No gap is ever silent. Dropped lines are counted and, as soon as the
//    pressure clears, a line is written saying how many were lost. A log that
//    quietly omits records is worse than no log, because it is believed.
//
// `flush()` blocks until everything queued before it has reached the wrapped
// sink and that sink has itself been flushed. The logger already flushes
// after anything at Error or above, so that rule alone gives the property
// that matters at the end of a run: a fatal record is on the disk before the
// call that wrote it returns, even though everything else is asynchronous.
//
// Ordering is never disturbed. One writer thread and a first-in-first-out
// queue mean lines reach the disk in the order they were written.

#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>

#include "platform/log_clock.hpp"
#include "platform/log_sink.hpp"

namespace squiflow::platform {

// A queue deep enough to absorb a burst, shallow enough that the memory it
// can hold is bounded by something a shop machine will not notice. Each entry
// is at most kMaxLogLineLength, so the worst case is a few tens of megabytes
// at the maximum depth and well under a megabyte at the default.
inline constexpr std::size_t kMaxAsyncQueueDepth = 65536;
inline constexpr std::size_t kDefaultAsyncQueueDepth = 1024;

struct AsyncLogPolicy {
    // Clamped into [1, kMaxAsyncQueueDepth] when applied.
    std::size_t queue_depth = kDefaultAsyncQueueDepth;
};

struct AsyncLogCounters {
    std::uint64_t submitted = 0;
    std::uint64_t written = 0;
    // Lines lost because the queue was full. Never silent: see gap_reports.
    std::uint64_t dropped = 0;
    // Lines written to declare a gap once the pressure cleared.
    std::uint64_t gap_reports = 0;
    std::uint64_t flushes = 0;
    // The deepest the queue has ever been, which is what tells an operator
    // whether the configured depth is anywhere near enough.
    std::uint64_t peak_depth = 0;
};

class AsyncLogSink final : public LogSink {
public:
    // The target must outlive this sink. The writer thread starts here and is
    // stopped and joined in the destructor, so the target is untouched once
    // this object is gone.
    AsyncLogSink(LogSink& target, const LogClock& clock,
                 AsyncLogPolicy policy = {});

    // Drains what is queued, declares any outstanding gap, flushes the target
    // and joins the writer thread. Shutdown never abandons queued lines: the
    // last thing written before a machine is switched off is usually the
    // reason it is being switched off.
    ~AsyncLogSink() override;

    // Returns as soon as the line is queued. Only false if the line could not
    // be queued at all; a dropped line is a counted gap, not a refusal, since
    // the caller can do nothing useful about it either way.
    bool write_line(std::string_view line) override;

    // Blocks until everything queued before this call has reached the target
    // and the target has been flushed.
    void flush() override;

    AsyncLogPolicy policy() const;
    AsyncLogCounters counters() const;

private:
    void run();

    // Builds the line that declares a gap. Called on the writer thread only.
    std::string gap_line(std::uint64_t dropped) const;

    LogSink& target_;
    const LogClock& clock_;
    AsyncLogPolicy policy_;

    mutable std::mutex mutex_;
    // Woken when there is work, a flush is wanted, or the sink is stopping.
    std::condition_variable work_;
    // Woken when the writer has caught up, for the benefit of flushers.
    std::condition_variable idle_;

    std::deque<std::string> queue_;
    // Lines dropped since the last gap report was written.
    std::uint64_t dropped_since_report_ = 0;
    // Counts completed flushes of the target, so a waiter can tell its own
    // flush apart from one another thread asked for.
    std::uint64_t flush_generation_ = 0;
    bool flush_wanted_ = false;
    bool stopping_ = false;

    AsyncLogCounters counters_;

    // Declared last so the thread cannot start before the state it reads.
    std::thread writer_;
};

}  // namespace squiflow::platform
