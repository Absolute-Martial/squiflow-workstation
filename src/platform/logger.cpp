#include "platform/logger.hpp"

#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/version.h>

#include "platform/log_formatter.hpp"

namespace squiflow::platform {
namespace {

spdlog::level::level_enum to_backend(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::Debug:
            return spdlog::level::debug;
        case LogLevel::Info:
            return spdlog::level::info;
        case LogLevel::Warning:
            return spdlog::level::warn;
        case LogLevel::Error:
            return spdlog::level::err;
        case LogLevel::Fatal:
            return spdlog::level::critical;
    }
    // A value from outside the enum is treated as the most serious thing that
    // could have happened, never as something to quietly drop.
    return spdlog::level::critical;
}

// The bridge from spdlog to the SquiFlow sink interface.
//
// Deliberately not an spdlog formatter. The payload arriving here has already
// been through SquiFlow's formatter, which is where escaping, credential
// redaction and the length bounds live. Letting spdlog re-format would put an
// unescaped pattern between those rules and the disk.
class ApplicationSink final : public spdlog::sinks::sink {
public:
    explicit ApplicationSink(LogSink& target) : target_(target) {}

    void log(const spdlog::details::log_msg& message) override {
        const std::string_view line(message.payload.data(),
                                    message.payload.size());
        if (!target_.write_line(line)) {
            refusals_ = refusals_ + 1;
        }
    }

    void flush() override { target_.flush(); }

    // SquiFlow decides the line shape, so a pattern from anywhere else is
    // ignored rather than half-applied.
    void set_pattern(const std::string&) override {}
    void set_formatter(std::unique_ptr<spdlog::formatter>) override {}

    std::uint64_t refusals() const noexcept { return refusals_; }

private:
    LogSink& target_;
    std::uint64_t refusals_ = 0;
};

// The formatter records an empty category as "general", so the policy has to
// agree: a rule on `general` must govern the records the formatter will file
// under that name.
std::string_view effective_category(std::string_view category) noexcept {
    return category.empty() ? std::string_view("general") : category;
}

}  // namespace

std::string_view logging_backend_version() noexcept {
    static_assert(SPDLOG_VER_MAJOR == 1, "vendored spdlog major version moved");
    static_assert(SPDLOG_VER_MINOR == 17, "vendored spdlog minor version moved");
    static_assert(SPDLOG_VER_PATCH == 0, "vendored spdlog patch version moved");
    return "spdlog 1.17.0";
}

class Logger::Impl {
public:
    Impl(LogSink& sink_target, const LogClock& clock_source,
         LogLevel minimum_level)
        : adapter(std::make_shared<ApplicationSink>(sink_target)),
          backend("squiflow", adapter),
          clock(clock_source),
          policy(minimum_level) {
        backend.set_level(to_backend(minimum_level));
        // spdlog would otherwise print its own complaints to stderr, which on
        // a shop machine nobody reads. Failures are counted instead.
        backend.set_error_handler(
            [this](const std::string&) { ++counters.sink_failures; });
    }

    // spdlog holds one threshold; SquiFlow holds a policy. The backend is
    // therefore set to the *lowest* level any category is allowed to speak at,
    // and the policy makes the real decision per record. Without this the
    // backend would silently discard a category that had been turned up.
    void refresh_backend_floor() {
        LogLevel floor = policy.default_level();
        for (const CategoryLevelRule& rule : policy.rules()) {
            if (static_cast<std::uint8_t>(rule.level) <
                static_cast<std::uint8_t>(floor)) {
                floor = rule.level;
            }
        }
        backend.set_level(to_backend(floor));
    }

    std::shared_ptr<ApplicationSink> adapter;
    spdlog::logger backend;
    const LogClock& clock;
    LogLevelPolicy policy;
    mutable std::mutex mutex;
    LoggerCounters counters;
    std::uint64_t seen_refusals = 0;
};

Logger::Logger(LogSink& sink, const LogClock& clock, LogLevel minimum_level)
    : impl_(std::make_unique<Impl>(sink, clock, minimum_level)) {}

Logger::~Logger() = default;

void Logger::set_minimum_level(LogLevel level) {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    impl_->policy.set_default_level(level);
    impl_->refresh_backend_floor();
}

LogLevel Logger::minimum_level() const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->policy.default_level();
}

bool Logger::set_category_level(std::string_view category, LogLevel level) {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    if (!impl_->policy.set_category_level(category, level)) {
        return false;
    }
    impl_->refresh_backend_floor();
    return true;
}

bool Logger::clear_category_level(std::string_view category) {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    if (!impl_->policy.clear_category_level(category)) {
        return false;
    }
    impl_->refresh_backend_floor();
    return true;
}

void Logger::clear_all_category_levels() {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    impl_->policy.clear_all_category_levels();
    impl_->refresh_backend_floor();
}

LevelConfigurationResult Logger::apply_level_configuration(
    std::string_view text) {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    LevelConfigurationResult result = impl_->policy.apply_configuration(text);
    impl_->refresh_backend_floor();
    return result;
}

std::string Logger::level_configuration() const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->policy.to_configuration();
}

LogLevel Logger::level_for(std::string_view category) const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->policy.level_for(effective_category(category));
}

bool Logger::is_enabled(LogLevel level) const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return static_cast<std::uint8_t>(level) >=
           static_cast<std::uint8_t>(impl_->policy.default_level());
}

bool Logger::is_enabled(std::string_view category, LogLevel level) const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->policy.is_enabled(effective_category(category), level);
}

void Logger::log(LogLevel level, std::string_view category,
                 std::string_view message, std::vector<LogField> fields) {
    // The whole call is serialised. Formatting outside the lock would be a
    // little faster and would allow two threads to write lines in an order
    // that contradicts their timestamps, which is exactly the confusion a log
    // exists to prevent.
    const std::lock_guard<std::mutex> guard(impl_->mutex);

    const spdlog::level::level_enum backend_level = to_backend(level);
    if (!impl_->policy.is_enabled(effective_category(category), level)) {
        ++impl_->counters.suppressed;
        return;
    }

    LogRecord record;
    record.level = level;
    record.category.assign(category);
    record.message.assign(message);
    record.fields = std::move(fields);
    record.timestamp_milliseconds = impl_->clock.now_milliseconds();

    try {
        // The payload is passed as an argument, never as a format string: a
        // message containing braces must not be interpreted as formatting.
        impl_->backend.log(backend_level, "{}", format_log_record(record));
    } catch (...) {
        // spdlog turns sink trouble into its error handler, but formatting and
        // allocation can still throw, and no caller of this class is prepared
        // for an exception.
        ++impl_->counters.sink_failures;
        return;
    }

    const std::uint64_t refusals = impl_->adapter->refusals();
    if (refusals != impl_->seen_refusals) {
        impl_->seen_refusals = refusals;
        ++impl_->counters.sink_failures;
    } else {
        ++impl_->counters.emitted;
    }

    if (level == LogLevel::Error || level == LogLevel::Fatal) {
        try {
            impl_->backend.flush();
        } catch (...) {
            ++impl_->counters.sink_failures;
        }
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
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    try {
        impl_->backend.flush();
    } catch (...) {
        ++impl_->counters.sink_failures;
    }
}

LoggerCounters Logger::counters() const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->counters;
}

}  // namespace squiflow::platform
