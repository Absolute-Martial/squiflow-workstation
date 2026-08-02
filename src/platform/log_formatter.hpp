#pragma once

// One record in, exactly one line out.
//
// Three properties matter more than prettiness:
//
//   One line always. A message containing a newline must not be able to forge
//   a second log entry, so every control character is escaped rather than
//   written. Log injection is input validation, not formatting taste.
//
//   Bounded always. A hostile or accidental megabyte-long value must not
//   become a megabyte-long line, so message, field names, field values, and
//   the whole line are capped and truncation is visible.
//
//   Never a secret. Fields whose names look like credentials are written with
//   their value replaced. The code of conduct forbids logging passwords,
//   tokens, and keys, and a rule that depends on every future caller
//   remembering it is not a rule.

#include <cstddef>
#include <string>
#include <string_view>

#include "platform/log_record.hpp"

namespace squiflow::platform {

inline constexpr std::size_t kMaxLogMessageLength = 2000;
inline constexpr std::size_t kMaxLogCategoryLength = 64;
inline constexpr std::size_t kMaxLogFieldNameLength = 64;
inline constexpr std::size_t kMaxLogFieldValueLength = 1000;
inline constexpr std::size_t kMaxLogFieldCount = 32;
inline constexpr std::size_t kMaxLogLineLength = 8000;

inline constexpr char kRedactedValue[] = "[redacted]";
inline constexpr char kTruncationMarker[] = "...";

// True when a field name suggests a credential. Matching is case-insensitive
// and by substring, so "api_token", "Token", and "refreshTokenValue" are all
// caught. False positives cost a redacted diagnostic; false negatives cost a
// leaked secret in a file the shop emails to support.
bool is_sensitive_field_name(std::string_view name);

// Formats a UTC instant as YYYY-MM-DDTHH:MM:SS.mmmZ.
//
// Written by hand rather than with gmtime, which returns a shared buffer and
// is not safely reentrant, or with std::format, which this compiler does not
// provide. Instants before the epoch are clamped to the epoch: a machine with
// a wrong clock should not produce a negative-looking timestamp that sorts
// above everything else.
std::string format_log_timestamp(std::int64_t milliseconds_since_epoch);

// Escapes and quotes a value so that it is one safe token on one line.
std::string escape_log_value(std::string_view value);

// The whole line, without a trailing newline. The sink owns line endings.
std::string format_log_record(const LogRecord& record);

}  // namespace squiflow::platform
