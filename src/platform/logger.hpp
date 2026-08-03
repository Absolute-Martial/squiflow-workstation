#pragma once

// The front door for logging.
//
// One object, injected by constructor reference wherever logging is needed.
// There is no global logger and no macro: a global would make the shutdown
// order unprovable, and a macro would hide the fact that a call can be
// suppressed by level.
//
// The dispatcher underneath is spdlog, pinned and vendored under
// external/spdlog. It is held behind a PIMPL so that no spdlog type appears
// in any SquiFlow header. Nothing above the platform boundary knows which
// logging library is in use, which is what makes the choice reversible.
//
// Guarantees this class makes to every caller, unchanged by that choice:
//
//   It never throws. A caller in the middle of issuing an invoice must not
//   have to reason about what happens if the disk is full.
//   It never blocks on anything except its own mutex.
//   It never loses an event silently: suppressed and failed events are
//   counted, and the counts can be reported at shutdown.
//   Anything at Error or above is flushed immediately, because the line that
//   matters is usually the last one before the machine was switched off.

#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

#include "platform/log_clock.hpp"
#include "platform/log_level_policy.hpp"
#include "platform/log_record.hpp"
#include "platform/log_sink.hpp"
#include "platform/log_backtrace.hpp"
#include "platform/log_throttle.hpp"

namespace squiflow::platform {

class CrashBreadcrumb;

// Which dispatcher was compiled in, as a plain string. Reported at startup and
// asserted by the test programme, so an unnoticed dependency change fails the
// gate rather than the shop counter.
std::string_view logging_backend_version() noexcept;

struct LoggerCounters {
    std::uint64_t emitted = 0;
    std::uint64_t suppressed = 0;
    std::uint64_t sink_failures = 0;
    // Records the throttle held back because the same thing was already being
    // said. Distinct from `suppressed`, which is the level filter.
    std::uint64_t rate_limited = 0;
    // Lines written purely to account for records held back earlier.
    std::uint64_t repeat_summaries = 0;
    // Records the level filter rejected that were later released
    // because a failure asked for the run-up to it.
    std::uint64_t backtrace_released = 0;
};

class Logger {
public:
    Logger(LogSink& sink, const LogClock& clock,
           LogLevel minimum_level = LogLevel::Info);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    ~Logger();

    // The level in force for any category without a rule of its own.
    void set_minimum_level(LogLevel level);
    LogLevel minimum_level() const;

    // Per-category verbosity. One area of the application can be turned up
    // for a support session without the rest of it drowning the file and
    // consuming the budget the interesting lines need.
    //
    // Rules match by dotted prefix, longest first: `storage` covers
    // `storage.migrate`, and a rule on `storage.migrate` overrides it. A
    // refused rule returns false and changes nothing; see
    // `log_level_policy.hpp` for what makes a category acceptable.
    bool set_category_level(std::string_view category, LogLevel level);
    bool clear_category_level(std::string_view category);
    void clear_all_category_levels();

    // Applies a settings string such as "info, sync=debug". Never throws and
    // never refuses wholesale: readable terms apply, unreadable ones are
    // returned so that startup can log exactly what it ignored.
    LevelConfigurationResult apply_level_configuration(std::string_view text);

    // What the current policy would be written as. Recorded at startup so a
    // support file states its own verbosity.
    std::string level_configuration() const;

    // The level actually in force for one category.
    LogLevel level_for(std::string_view category) const;

    // Cheap enough to guard an expensive diagnostic with. The category-aware
    // form is the accurate one; the other asks about the default level only.
    bool is_enabled(LogLevel level) const;
    bool is_enabled(std::string_view category, LogLevel level) const;

    // Rate limiting, off by default. When engaged, identical records (same
    // level, category and message) are held back and accounted for rather
    // than written, so that a retry loop failing thirty times a second cannot
    // spend the log budget and push the evidence of its own cause out of the
    // file.
    //
    // No gap is ever silent: the next record written carries a `repeated`
    // field counting what it stands for, and anything still owed is reported
    // at `flush()` and at shutdown. Fatal is never held back.
    // The deferred run-up, off by default. Records the level filter
    // rejects are held in a small ring instead of being discarded, and
    // released ahead of the next record at or above the trigger level.
    //
    // This is how a machine running at Info can still explain a failure
    // that only the Debug lines preceding it could have explained,
    // without paying for those lines on every ordinary day.
    void set_backtrace_policy(const LogBacktracePolicy& policy);
    LogBacktracePolicy backtrace_policy() const;

    void set_throttle_policy(const LogThrottlePolicy& policy);
    LogThrottlePolicy throttle_policy() const;

    void log(LogLevel level, std::string_view category,
             std::string_view message, std::vector<LogField> fields = {});

    void debug(std::string_view category, std::string_view message,
               std::vector<LogField> fields = {});
    void info(std::string_view category, std::string_view message,
              std::vector<LogField> fields = {});
    void warning(std::string_view category, std::string_view message,
                 std::vector<LogField> fields = {});
    void error(std::string_view category, std::string_view message,
               std::vector<LogField> fields = {});
    void fatal(std::string_view category, std::string_view message,
               std::vector<LogField> fields = {});

    // Borrowed startup resource. Passing nullptr detaches it. Records that
    // pass the level policy are published before the sink chain is entered.
    void set_breadcrumb(CrashBreadcrumb* breadcrumb) noexcept;

    void flush();

    LoggerCounters counters() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace squiflow::platform
