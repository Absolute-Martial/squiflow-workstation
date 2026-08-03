#include "platform/logger.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <spdlog/logger.h>
#include <spdlog/sinks/sink.h>
#include <spdlog/version.h>

#include "platform/crash_breadcrumb.hpp"
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
        if (backtrace.enabled()) {
            // The ring exists to release records the level filter rejected.
            // If the dispatcher threshold stayed at the filter level it would
            // discard exactly those records on their way out, and the feature
            // would appear to work while producing nothing.
            floor = LogLevel::Debug;
        }
        backend.set_level(to_backend(floor));
    }

    // The held run-up, written ahead of the failure that asked for it. Each
    // line is marked, so nobody mistakes a released Debug record for one the
    // current level would have written.
    void release_backtrace() {
        for (LogRecord& held : backtrace.take()) {
            held.fields.push_back(LogField{"backtrace", "1"});
            ++counters.backtrace_released;
            write_record(held);
        }
    }

    // One place where a record becomes a line, so that refusal detection and
    // the flush after Error cannot drift apart between the ordinary path and
    // the summaries written on the throttle's behalf.
    void write_record(const LogRecord& record) {
        if (CrashBreadcrumb* ring = breadcrumb.load(std::memory_order_acquire)) {
            ring->push(record.level, effective_category(record.category),
                       record.message, record.timestamp_milliseconds);
        }
        try {
            // The payload is passed as an argument, never as a format string:
            // a message containing braces must not be read as formatting.
            backend.log(to_backend(record.level), "{}",
                        format_log_record(record));
        } catch (...) {
            // spdlog turns sink trouble into its error handler, but formatting
            // and allocation can still throw, and no caller of this class is
            // prepared for an exception.
            ++counters.sink_failures;
            return;
        }

        const std::uint64_t refusals = adapter->refusals();
        if (refusals != seen_refusals) {
            seen_refusals = refusals;
            ++counters.sink_failures;
        } else {
            ++counters.emitted;
        }

        if (record.level == LogLevel::Error ||
            record.level == LogLevel::Fatal) {
            try {
                backend.flush();
            } catch (...) {
                ++counters.sink_failures;
            }
        }
    }

    // A held-back record is never simply forgotten. Whatever the throttle
    // silenced is accounted for by a line saying how many there were.
    void report_repeats(const std::vector<RepeatSummary>& owed,
                        std::int64_t now_milliseconds) {
        for (const RepeatSummary& summary : owed) {
            LogRecord record;
            record.level = summary.level;
            record.category = summary.category;
            record.message = summary.message;
            record.fields.push_back(LogField{"throttled", "summary"});
            record.fields.push_back(
                LogField{"repeated", std::to_string(summary.suppressed)});
            record.timestamp_milliseconds = now_milliseconds;
            ++counters.repeat_summaries;
            write_record(record);
        }
    }

    void drain_throttle(std::int64_t now_milliseconds) {
        report_repeats(throttle.drain(), now_milliseconds);
    }

    std::shared_ptr<ApplicationSink> adapter;
    spdlog::logger backend;
    const LogClock& clock;
    LogLevelPolicy policy;
    LogThrottle throttle;
    LogBacktrace backtrace;
    std::atomic<CrashBreadcrumb*> breadcrumb{nullptr};
    mutable std::mutex mutex;
    LoggerCounters counters;
    std::uint64_t seen_refusals = 0;
};

Logger::Logger(LogSink& sink, const LogClock& clock, LogLevel minimum_level)
    : impl_(std::make_unique<Impl>(sink, clock, minimum_level)) {}

Logger::~Logger() {
    impl_->breadcrumb.store(nullptr, std::memory_order_release);
    // A gap must never outlive the run that caused it, and a destructor may
    // never throw: whatever is still owed is written here, and any failure
    // doing so is swallowed because there is nothing left to report it to.
    try {
        const std::lock_guard<std::mutex> guard(impl_->mutex);
        impl_->drain_throttle(impl_->clock.now_milliseconds());
        impl_->backend.flush();
    } catch (...) {
    }
}

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

    if (!impl_->policy.is_enabled(effective_category(category), level)) {
        ++impl_->counters.suppressed;
        if (impl_->backtrace.enabled()) {
            // Rejected, but not yet thrown away: if something fails shortly
            // after this, these are the lines that will explain it.
            LogRecord rejected;
            rejected.level = level;
            rejected.category.assign(category);
            rejected.message.assign(message);
            rejected.fields = std::move(fields);
            rejected.timestamp_milliseconds = impl_->clock.now_milliseconds();
            impl_->backtrace.remember(rejected);
        }
        return;
    }

    // One reading of the clock for the whole decision, so that the record and
    // any summary it displaces cannot disagree about when this happened.
    const std::int64_t now = impl_->clock.now_milliseconds();

    const ThrottleDecision decision = impl_->throttle.consider(
        level, effective_category(category), message, now);

    if (decision.evicted.has_value()) {
        // Something was pushed out of the throttle's table still owing an
        // account of itself. Settle that debt before writing anything new.
        impl_->report_repeats({*decision.evicted}, now);
    }

    if (!decision.emit) {
        ++impl_->counters.rate_limited;
        return;
    }

    LogRecord record;
    record.level = level;
    record.category.assign(category);
    record.message.assign(message);
    record.fields = std::move(fields);
    record.timestamp_milliseconds = now;

    if (decision.suppressed_since_last > 0) {
        // This line speaks for the ones that were held back behind it.
        record.fields.push_back(LogField{
            "repeated", std::to_string(decision.suppressed_since_last)});
    }

    if (impl_->backtrace.triggers_on(level)) {
        // Ahead of the record, so the file reads in the order things happened
        // and the failure arrives with its run-up already on the page.
        impl_->release_backtrace();
    }

    impl_->write_record(record);
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

void Logger::set_backtrace_policy(const LogBacktracePolicy& policy) {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    impl_->backtrace.set_policy(policy);
    impl_->refresh_backend_floor();
}

LogBacktracePolicy Logger::backtrace_policy() const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->backtrace.policy();
}

void Logger::set_throttle_policy(const LogThrottlePolicy& policy) {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    // Settle what was held under the old rule before the new one applies:
    // a debt measured under one policy must not be reported under another.
    impl_->drain_throttle(impl_->clock.now_milliseconds());
    impl_->throttle.set_policy(policy);
}

LogThrottlePolicy Logger::throttle_policy() const {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    return impl_->throttle.policy();
}

void Logger::set_breadcrumb(CrashBreadcrumb* breadcrumb) noexcept {
    impl_->breadcrumb.store(breadcrumb, std::memory_order_release);
}

void Logger::flush() {
    const std::lock_guard<std::mutex> guard(impl_->mutex);
    impl_->drain_throttle(impl_->clock.now_milliseconds());
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
