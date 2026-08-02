#include "platform/log_record.hpp"

#include <array>
#include <cstddef>

namespace squiflow::platform {
namespace {

constexpr std::array<const char*, kLogLevelCount> kLevelNames = {
    "DEBUG", "INFO ", "WARN ", "ERROR", "FATAL"};

constexpr std::array<const char*, kLogLevelCount> kLevelKeywords = {
    "debug", "info", "warning", "error", "fatal"};

char to_lower_ascii(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<char>(value - 'A' + 'a');
    }
    return value;
}

bool equals_ignoring_case(std::string_view text, const char* keyword) {
    std::size_t index = 0;
    while (keyword[index] != '\0') {
        if (index >= text.size()) {
            return false;
        }
        if (to_lower_ascii(text[index]) != keyword[index]) {
            return false;
        }
        ++index;
    }
    return index == text.size();
}

}  // namespace

const char* level_name(LogLevel level) {
    const auto index = static_cast<std::size_t>(level);
    if (index >= kLevelNames.size()) {
        return "?????";
    }
    return kLevelNames[index];
}

bool parse_log_level(std::string_view text, LogLevel& out) {
    for (std::size_t index = 0; index < kLevelKeywords.size(); ++index) {
        if (equals_ignoring_case(text, kLevelKeywords[index])) {
            out = static_cast<LogLevel>(index);
            return true;
        }
    }
    return false;
}

}  // namespace squiflow::platform
