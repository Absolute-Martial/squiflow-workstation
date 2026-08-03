#include "platform/async_log_sink.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include "platform/log_formatter.hpp"
#include "platform/log_record.hpp"

namespace squiflow::platform {
namespace {

std::size_t clamped_depth(std::size_t requested) {
    if (requested == 0) {
        return 1;
    }
    return std::min(requested, kMaxAsyncQueueDepth);
}

}  // namespace

AsyncLogSink::AsyncLogSink(LogSink& target, const LogClock& clock,
                           AsyncLogPolicy policy)
    : target_(target), clock_(clock), policy_(policy) {
    policy_.queue_depth = clamped_depth(policy_.queue_depth);
    writer_ = std::thread([this] { run(); });
}

AsyncLogSink::~AsyncLogSink() {
    try {
        {
            const std::lock_guard<std::mutex> guard(mutex_);
            stopping_ = true;
            // Ask for one final flush so the queue reaches the disk rather
            // than merely reaching the target's own buffer.
            flush_wanted_ = true;
        }
        work_.notify_all();
        if (writer_.joinable()) {
            writer_.join();
        }
    } catch (...) {
        // A destructor may not throw, and there is nothing left to log to,
        // since this is the thing that does the logging.
        if (writer_.joinable()) {
            writer_.detach();
        }
    }
}

bool AsyncLogSink::write_line(std::string_view line) {
    {
        const std::lock_guard<std::mutex> guard(mutex_);
        if (stopping_) {
            // Written during shutdown, after the writer was told to stop.
            // Refusing is honest: nothing would ever deliver it.
            return false;
        }

        ++counters_.submitted;

        if (queue_.size() >= policy_.queue_depth) {
            // Full. Drop the oldest, because under pressure the newest lines
            // are the ones that explain what is happening now.
            queue_.pop_front();
            ++counters_.dropped;
            ++dropped_since_report_;
        }

        queue_.emplace_back(line);
        counters_.peak_depth = std::max(
            counters_.peak_depth, static_cast<std::uint64_t>(queue_.size()));
    }

    work_.notify_one();
    return true;
}

void AsyncLogSink::flush() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!writer_.joinable()) {
        return;
    }

    const std::uint64_t wanted = flush_generation_ + 1;
    flush_wanted_ = true;
    lock.unlock();
    work_.notify_one();
    lock.lock();

    // Waiting on the generation rather than on a flag means a flush requested
    // by another thread at the same moment cannot be mistaken for this one.
    idle_.wait(lock, [this, wanted] { return flush_generation_ >= wanted; });
}

AsyncLogPolicy AsyncLogSink::policy() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return policy_;
}

AsyncLogCounters AsyncLogSink::counters() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return counters_;
}

std::string AsyncLogSink::gap_line(std::uint64_t dropped) const {
    LogRecord record;
    record.level = LogLevel::Warning;
    record.category = "logging";
    record.message = "log records were dropped while the queue was full";
    record.fields.push_back(LogField{"dropped", std::to_string(dropped)});
    record.timestamp_milliseconds = clock_.now_milliseconds();
    return format_log_record(record);
}

void AsyncLogSink::run() {
    for (;;) {
        std::string line;
        bool have_line = false;
        bool do_flush = false;
        std::uint64_t gap = 0;

        {
            std::unique_lock<std::mutex> lock(mutex_);
            work_.wait(lock, [this] {
                return !queue_.empty() || flush_wanted_ || stopping_;
            });

            if (!queue_.empty()) {
                // The queue is moving again, so this is the moment to admit
                // what was lost. The declaration goes out before the line that
                // follows the gap, which is where a reader would look for it.
                if (dropped_since_report_ > 0) {
                    gap = dropped_since_report_;
                    dropped_since_report_ = 0;
                }
                line = std::move(queue_.front());
                queue_.pop_front();
                have_line = true;
            } else if (flush_wanted_) {
                do_flush = true;
                flush_wanted_ = false;
            } else {
                // Nothing queued and no flush wanted: only stopping brings us
                // here, and the queue is already empty.
                return;
            }
        }

        if (gap > 0) {
            std::string declaration;
            try {
                declaration = gap_line(gap);
            } catch (...) {
                declaration.clear();
            }

            bool reported = false;
            if (!declaration.empty()) {
                try {
                    reported = target_.write_line(declaration);
                } catch (...) {
                    reported = false;
                }
            }

            const std::lock_guard<std::mutex> guard(mutex_);
            if (reported) {
                ++counters_.gap_reports;
            } else {
                // The declaration itself could not be written. Put the debt
                // back rather than losing the fact that there was a gap.
                dropped_since_report_ += gap;
            }
        }

        if (have_line) {
            bool written = false;
            try {
                written = target_.write_line(line);
            } catch (...) {
                // A sink is not supposed to throw, but this thread has no
                // caller to report to and must not take the process down.
                written = false;
            }

            const std::lock_guard<std::mutex> guard(mutex_);
            if (written) {
                ++counters_.written;
            }
            continue;
        }

        if (do_flush) {
            try {
                target_.flush();
            } catch (...) {
                // Nothing useful can be done here, and the waiter is released
                // either way: a failing disk must not hang the application.
            }

            bool finished = false;
            {
                const std::lock_guard<std::mutex> guard(mutex_);
                ++counters_.flushes;
                ++flush_generation_;
                finished = stopping_ && queue_.empty() && !flush_wanted_;
            }
            idle_.notify_all();

            if (finished) {
                return;
            }
        }
    }
}

}  // namespace squiflow::platform
