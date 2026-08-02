#pragma once

// What one logged event is made of.
//
// A log line is evidence. Months after a disputed invoice, the only thing left
// of that afternoon is this record, so it carries structure rather than a
// sentence: a level, the part of the application that spoke, a short fixed
// message, and named fields. Fixed messages with named fields can be searched
// and counted; interpolated sentences cannot.
//
// This header knows nothing about files, Qt, or where logs are stored.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace squiflow::platform {

// Five levels, deliberately few. More levels only move the argument from "what
// happened" to "which level is this", and a shop counter has nobody to have
// that argument with.
//
// Debug   - only useful while chasing a specific problem; off in a shipped build
// Info    - something the shop did: started, issued, saved, synced
// Warning - recovered, but somebody should know
// Error   - the operation failed and the user was told
// Fatal   - the application cannot continue
enum class LogLevel : std::uint8_t {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3,
    Fatal = 4,
};

inline constexpr std::size_t kLogLevelCount = 5;

// Fixed width so that columns line up in a text editor, which is where these
// files are actually read.
const char* level_name(LogLevel level);

// Case-insensitive, exact match only. Returns false and leaves `out`
// untouched for anything unrecognised, because a mistyped configured level
// must fall back to a known default rather than silently disabling the log.
bool parse_log_level(std::string_view text, LogLevel& out);

struct LogField {
    std::string name;
    std::string value;
};

struct LogRecord {
    LogLevel level = LogLevel::Info;
    // The part of the application speaking, e.g. "startup", "storage.migrate".
    std::string category;
    // Short, fixed, and free of interpolated values. Values go in fields.
    std::string message;
    std::vector<LogField> fields;
    // Milliseconds since the Unix epoch, UTC.
    std::int64_t timestamp_milliseconds = 0;
};

}  // namespace squiflow::platform
