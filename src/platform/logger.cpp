#include "platform/logger.hpp"

#include <utility>

#include "platform/log_formatter.hpp"

namespace squiflow::platform {

Logger::Logger(LogSink& sink, const LogClock& clock, LogLevel minimum_level)
    : sink_(sink), clock_(clock), minimum_level_(minimum_level) {}

void Logger::set_minimum_level(LogLevel level) {
    const std::lock_guard<std::mutex> guard(mutex_);
    minimum_level_ = level;
}

LogLevel Logger::minimum_level() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return minimum_level_;
}

bool Logger::is_enabled(LogLevel level) const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return static_cast<std::uint8_t>(level) >=
           static_cast<std::uint8_t>(minimum_level_);
}

void Logger::log(LogLevel level, std::string_view category,
                 std::string_view message, std::vector<LogField> fields) {
    // The whole call is serialised. Formatting outside the lock would be a
    // little faster and would allow two threads to write lines in an order
    // that contradicts their timestamps, which is exactly the confusion a log
    // exists to prevent.
    const std::lock_guard<std::mutex> guard(mutex_);

    if (static_cast<std::uint8_t>(level) <
        static_cast<std::uint8_t>(minimum_level_)) {
        ++counters_.suppressed;
        return;
    }

    LogRecord record;
    record.level = level;
    record.category.assign(category);
    record.message.assign(message);
    record.fields = std::move(fields);
    record.timestamp_milliseconds = clock_.now_milliseconds();

    if (sink_.write_line(format_log_record(record))) {
        ++counters_.emitted;
    } else {
        ++counters_.sink_failures;
    }

    if (static_cast<std::uint8_t>(level) >=
        static_cast<std::uint8_t>(LogLevel::Error)) {
        sink_.flush();
    }
}

void Logger::debug(std::string_view category, std::string_view message,
                   std::vector<LogField> fields) {
    log(LogLevel::Debug, category, message, std::move(fields));
}

void Logger::info(std::string_view category, std::string_view message,
                  std::vector<LogField> fields) {
    log(LogLevel::Info, category, message, std::move(fields));
}

void Logger::warning(std::string_view category, std::string_view message,
                     std::vector<LogField> fields) {
    log(LogLevel::Warning, category, message, std::move(fields));
}

void Logger::error(std::string_view category, std::string_view message,
                   std::vector<LogField> fields) {
    log(LogLevel::Error, category, message, std::move(fields));
}

void Logger::fatal(std::string_view category, std::string_view message,
                   std::vector<LogField> fields) {
    log(LogLevel::Fatal, category, message, std::move(fields));
}

void Logger::flush() {
    const std::lock_guard<std::mutex> guard(mutex_);
    sink_.flush();
}

LoggerCounters Logger::counters() const {
    const std::lock_guard<std::mutex> guard(mutex_);
    return counters_;
}

}  // namespace squiflow::platform
