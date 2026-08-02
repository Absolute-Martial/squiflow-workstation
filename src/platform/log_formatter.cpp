#include "platform/log_formatter.hpp"

#include <array>
#include <cstdint>

namespace squiflow::platform {
namespace {

constexpr std::int64_t kMillisecondsPerSecond = 1000;
constexpr std::int64_t kSecondsPerMinute = 60;
constexpr std::int64_t kSecondsPerHour = 3600;
constexpr std::int64_t kSecondsPerDay = 86400;

constexpr std::array<const char*, 7> kSensitiveMarkers = {
    "password", "secret", "token", "credential", "passphrase", "apikey",
    "privatekey"};

char to_lower_ascii(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

std::string lowered(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(to_lower_ascii(character));
    }
    return result;
}

// "key" alone is too common to ban outright: "invoice_key" and "sort_key" are
// not secrets. Only the compound forms are treated as credentials, plus a bare
// name that is exactly "key".
bool looks_like_key_field(const std::string& name) {
    if (name == "key") {
        return true;
    }
    return name.find("signingkey") != std::string::npos ||
           name.find("secretkey") != std::string::npos ||
           name.find("accesskey") != std::string::npos;
}

std::string two_digits(std::int64_t value) {
    std::string text;
    text.push_back(static_cast<char>('0' + (value / 10) % 10));
    text.push_back(static_cast<char>('0' + value % 10));
    return text;
}

std::string four_digits(std::int64_t value) {
    std::string text;
    text.push_back(static_cast<char>('0' + (value / 1000) % 10));
    text.push_back(static_cast<char>('0' + (value / 100) % 10));
    text.push_back(static_cast<char>('0' + (value / 10) % 10));
    text.push_back(static_cast<char>('0' + value % 10));
    return text;
}

std::string three_digits(std::int64_t value) {
    std::string text;
    text.push_back(static_cast<char>('0' + (value / 100) % 10));
    text.push_back(static_cast<char>('0' + (value / 10) % 10));
    text.push_back(static_cast<char>('0' + value % 10));
    return text;
}

struct CivilDate {
    std::int64_t year;
    std::int64_t month;
    std::int64_t day;
};

// Howard Hinnant's days-to-civil algorithm, shifted to an era beginning on
// 0000-03-01 so that the leap day is the last day of the era and February
// needs no special case. Proven correct for the whole range of days this
// application can produce.
CivilDate civil_from_days(std::int64_t days) {
    days += 719468;
    const std::int64_t era = (days >= 0 ? days : days - 146096) / 146097;
    const std::int64_t day_of_era = days - era * 146097;
    const std::int64_t year_of_era =
        (day_of_era - day_of_era / 1460 + day_of_era / 36524 -
         day_of_era / 146096) /
        365;
    const std::int64_t year = year_of_era + era * 400;
    const std::int64_t day_of_year =
        day_of_era - (365 * year_of_era + year_of_era / 4 - year_of_era / 100);
    const std::int64_t shifted_month = (5 * day_of_year + 2) / 153;
    const std::int64_t day = day_of_year - (153 * shifted_month + 2) / 5 + 1;
    const std::int64_t month =
        shifted_month + (shifted_month < 10 ? 3 : -9);
    return CivilDate{year + (month <= 2 ? 1 : 0), month, day};
}

void append_escaped_character(std::string& out, char character) {
    const auto raw = static_cast<unsigned char>(character);
    switch (character) {
        case '"':
            out += "\\\"";
            return;
        case '\\':
            out += "\\\\";
            return;
        case '\n':
            out += "\\n";
            return;
        case '\r':
            out += "\\r";
            return;
        case '\t':
            out += "\\t";
            return;
        default:
            break;
    }
    if (raw < 0x20 || raw == 0x7F) {
        static constexpr char kHexDigits[] = "0123456789ABCDEF";
        out += "\\x";
        out.push_back(kHexDigits[(raw >> 4) & 0x0F]);
        out.push_back(kHexDigits[raw & 0x0F]);
        return;
    }
    out.push_back(character);
}

std::string_view clipped(std::string_view text, std::size_t limit) {
    if (text.size() <= limit) {
        return text;
    }
    return text.substr(0, limit);
}

bool needs_truncation(std::string_view text, std::size_t limit) {
    return text.size() > limit;
}

// Category and field names are identifiers, not prose. Anything outside the
// permitted set becomes an underscore so that a name can never introduce a
// separator and split one entry into two apparent fields.
std::string sanitised_name(std::string_view name, std::size_t limit) {
    std::string result;
    const std::string_view source = clipped(name, limit);
    result.reserve(source.size());
    for (const char character : source) {
        const bool acceptable =
            (character >= 'a' && character <= 'z') ||
            (character >= 'A' && character <= 'Z') ||
            (character >= '0' && character <= '9') || character == '.' ||
            character == '_' || character == '-';
        result.push_back(acceptable ? character : '_');
    }
    if (result.empty()) {
        result = "unnamed";
    }
    return result;
}

}  // namespace

bool is_sensitive_field_name(std::string_view name) {
    const std::string lower = lowered(name);
    for (const char* marker : kSensitiveMarkers) {
        if (lower.find(marker) != std::string::npos) {
            return true;
        }
    }
    return looks_like_key_field(lower);
}

std::string format_log_timestamp(std::int64_t milliseconds_since_epoch) {
    const std::int64_t clamped =
        milliseconds_since_epoch < 0 ? 0 : milliseconds_since_epoch;
    const std::int64_t total_seconds = clamped / kMillisecondsPerSecond;
    const std::int64_t milliseconds = clamped % kMillisecondsPerSecond;
    const std::int64_t days = total_seconds / kSecondsPerDay;
    const std::int64_t second_of_day = total_seconds % kSecondsPerDay;
    const CivilDate date = civil_from_days(days);

    std::string text;
    text.reserve(24);
    text += four_digits(date.year);
    text.push_back('-');
    text += two_digits(date.month);
    text.push_back('-');
    text += two_digits(date.day);
    text.push_back('T');
    text += two_digits(second_of_day / kSecondsPerHour);
    text.push_back(':');
    text += two_digits((second_of_day / kSecondsPerMinute) % kSecondsPerMinute);
    text.push_back(':');
    text += two_digits(second_of_day % kSecondsPerMinute);
    text.push_back('.');
    text += three_digits(milliseconds);
    text.push_back('Z');
    return text;
}

std::string escape_log_value(std::string_view value) {
    const bool truncate = needs_truncation(value, kMaxLogFieldValueLength);
    const std::string_view source = clipped(value, kMaxLogFieldValueLength);
    std::string result;
    result.reserve(source.size() + 2);
    result.push_back('"');
    for (const char character : source) {
        append_escaped_character(result, character);
    }
    if (truncate) {
        result += kTruncationMarker;
    }
    result.push_back('"');
    return result;
}

std::string format_log_record(const LogRecord& record) {
    std::string line;
    line.reserve(128);
    line += format_log_timestamp(record.timestamp_milliseconds);
    line.push_back(' ');
    line += level_name(record.level);
    line.push_back(' ');
    line += sanitised_name(record.category.empty()
                               ? std::string_view("general")
                               : std::string_view(record.category),
                           kMaxLogCategoryLength);
    line.push_back(' ');
    line += escape_log_value(clipped(record.message, kMaxLogMessageLength));

    std::size_t written_fields = 0;
    for (const LogField& field : record.fields) {
        if (written_fields >= kMaxLogFieldCount) {
            line += " fields_omitted=";
            line += escape_log_value(
                std::to_string(record.fields.size() - written_fields));
            break;
        }
        line.push_back(' ');
        line += sanitised_name(field.name, kMaxLogFieldNameLength);
        line.push_back('=');
        line += is_sensitive_field_name(field.name)
                    ? escape_log_value(kRedactedValue)
                    : escape_log_value(field.value);
        ++written_fields;
    }

    if (line.size() > kMaxLogLineLength) {
        line.resize(kMaxLogLineLength);
        line += kTruncationMarker;
    }
    return line;
}

}  // namespace squiflow::platform
