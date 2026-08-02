#pragma once

// The front door for logging.
//
// One object, injected by constructor reference wherever logging is needed.
// There is no global logger and no macro: a global would make the shutdown
// order unprovable, and a macro would hide the fact that a call can be
// suppressed by level.
//
// Guarantees this class makes to every caller:
//
//   It never throws. A caller in the middle of issuing an invoice must not
//   have to reason about what happens if the disk is full.
//   It never blocks on anything except its own mutex.
//   It never loses an event silently: suppressed and failed events are
//   counted, and the counts can be reported at shutdown.
//   Anything at Error or above is flushed immediately, because the line that
//   matters is usually the last one before the machine was switched off.

#include <cstdint>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "platform/log_clock.hpp"
#include "platform/log_record.hpp"
#include "platform/log_sink.hpp"

namespace squiflow::platform {

struct LoggerCounters {
    std::uint64_t emitted = 0;
    std::uint64_t suppressed = 0;
    std::uint64_t sink_failures = 0;
};

class Logger {
public:
    Logger(LogSink& sink, const LogClock& clock,
           LogLevel minimum_level = LogLevel::Info);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;
    ~Logger() = default;

    void set_minimum_level(LogLevel level);
    LogLevel minimum_level() const;

    // Cheap enough to guard an expensive diagnostic with.
    bool is_enabled(LogLevel level) const;

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

    void flush();

    LoggerCounters counters() const;

private:
    LogSink& sink_;
    const LogClock& clock_;
    mutable std::mutex mutex_;
    LogLevel minimum_level_;
    LoggerCounters counters_;
};

}  // namespace squiflow::platform
