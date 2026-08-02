#include "platform/log_level_policy.hpp"

#include <algorithm>
#include <cctype>

#include "platform/log_formatter.hpp"

namespace squiflow::platform {
namespace {

// A settings string arrives from a file a human edited, so it is treated as
// hostile input. These bounds stop a corrupted or malicious file from making
// the parser allocate for as long as the string is long.
constexpr std::size_t kMaxTermsExamined = 128;
constexpr std::size_t kMaxRejectedTermLength = 80;

char lowered(char character) {
    const auto value = static_cast<unsigned char>(character);
    return static_cast<char>(std::tolower(value));
}

std::string to_lower(std::string_view text) {
    std::string result;
    result.reserve(text.size());
    for (const char character : text) {
        result.push_back(lowered(character));
    }
    return result;
}

bool is_space(char character) {
    const auto value = static_cast<unsigned char>(character);
    return std::isspace(value) != 0;
}

std::string_view trimmed(std::string_view text) {
    while (!text.empty() && is_space(text.front())) {
        text.remove_prefix(1);
    }
    while (!text.empty() && is_space(text.back())) {
        text.remove_suffix(1);
    }
    return text;
}

// The configuration vocabulary, which is deliberately not the display
// vocabulary. `level_name` is padded for column alignment in a text editor and
// spells Warning as "WARN "; `parse_log_level` accepts "warning". Deriving one
// from the other produced a string this class could not read back, so the two
// vocabularies are now stated separately and the round-trip test holds them
// together.
std::string configured_level_name(LogLevel level) {
    switch (level) {
        case LogLevel::Debug:
            return "debug";
        case LogLevel::Info:
            return "info";
        case LogLevel::Warning:
            return "warning";
        case LogLevel::Error:
            return "error";
        case LogLevel::Fatal:
            return "fatal";
    }
    // Unreachable for a value from the enum. A value from outside it is
    // reported as the most serious level rather than as an empty term that
    // would silently vanish from the configuration string.
    return "fatal";
}

// A rule matches when the category is the rule itself, or begins with the rule
// followed by a dot. The dot requirement is what keeps `storage` from
// capturing `storagecleanup`, which would be a silent and very confusing way
// to lose log lines.
bool covers(std::string_view rule, std::string_view category) {
    if (category.size() < rule.size()) {
        return false;
    }
    if (category.compare(0, rule.size(), rule) != 0) {
        return false;
    }
    return category.size() == rule.size() || category[rule.size()] == '.';
}

std::string clipped(std::string_view text) {
    if (text.size() <= kMaxRejectedTermLength) {
        return std::string(text);
    }
    std::string result(text.substr(0, kMaxRejectedTermLength));
    result.append(kTruncationMarker);
    return result;
}

}  // namespace

bool is_valid_log_category(std::string_view category) {
    if (category.empty() || category.size() > kMaxLogCategoryLength) {
        return false;
    }
    if (category.front() == '.' || category.back() == '.') {
        return false;
    }

    bool previous_was_dot = false;
    for (const char character : category) {
        const bool is_lower_letter = character >= 'a' && character <= 'z';
        const bool is_digit = character >= '0' && character <= '9';
        const bool is_separator = character == '_' || character == '-';
        const bool is_dot = character == '.';

        if (!is_lower_letter && !is_digit && !is_separator && !is_dot) {
            return false;
        }
        if (is_dot && previous_was_dot) {
            return false;
        }
        previous_was_dot = is_dot;
    }
    return true;
}

LogLevelPolicy::LogLevelPolicy(LogLevel default_level)
    : default_level_(default_level) {}

void LogLevelPolicy::set_default_level(LogLevel level) {
    default_level_ = level;
}

LogLevel LogLevelPolicy::default_level() const { return default_level_; }

const CategoryLevelRule* LogLevelPolicy::find_exact(
    std::string_view category) const {
    for (const CategoryLevelRule& rule : rules_) {
        if (rule.category == category) {
            return &rule;
        }
    }
    return nullptr;
}

bool LogLevelPolicy::set_category_level(std::string_view category,
                                        LogLevel level) {
    if (!is_valid_log_category(category)) {
        return false;
    }

    for (CategoryLevelRule& rule : rules_) {
        if (rule.category == category) {
            rule.level = level;
            return true;
        }
    }

    if (rules_.size() >= kMaxLogCategoryRules) {
        return false;
    }

    CategoryLevelRule rule;
    rule.category.assign(category);
    rule.level = level;
    rules_.push_back(std::move(rule));
    return true;
}

bool LogLevelPolicy::clear_category_level(std::string_view category) {
    for (std::size_t index = 0; index < rules_.size(); ++index) {
        if (rules_[index].category == category) {
            rules_.erase(rules_.begin() +
                         static_cast<std::ptrdiff_t>(index));
            return true;
        }
    }
    return false;
}

void LogLevelPolicy::clear_all_category_levels() { rules_.clear(); }

LogLevel LogLevelPolicy::level_for(std::string_view category) const {
    const CategoryLevelRule* best = nullptr;
    for (const CategoryLevelRule& rule : rules_) {
        if (!covers(rule.category, category)) {
            continue;
        }
        // Longest wins, so a specific rule always beats the family rule it
        // sits inside, whatever order they were configured in.
        if (best == nullptr || rule.category.size() > best->category.size()) {
            best = &rule;
        }
    }
    return best == nullptr ? default_level_ : best->level;
}

bool LogLevelPolicy::is_enabled(std::string_view category,
                                LogLevel level) const {
    return static_cast<std::uint8_t>(level) >=
           static_cast<std::uint8_t>(level_for(category));
}

std::size_t LogLevelPolicy::rule_count() const { return rules_.size(); }

std::vector<CategoryLevelRule> LogLevelPolicy::rules() const {
    std::vector<CategoryLevelRule> ordered = rules_;
    std::sort(ordered.begin(), ordered.end(),
              [](const CategoryLevelRule& left,
                 const CategoryLevelRule& right) {
                  return left.category < right.category;
              });
    return ordered;
}

LevelConfigurationResult LogLevelPolicy::apply_configuration(
    std::string_view text) {
    LevelConfigurationResult result;

    std::size_t examined = 0;
    std::size_t position = 0;
    while (position <= text.size()) {
        const std::size_t separator = text.find_first_of(",;", position);
        const std::size_t end =
            separator == std::string_view::npos ? text.size() : separator;
        const std::string_view term =
            trimmed(text.substr(position, end - position));
        position = end + 1;

        if (term.empty()) {
            // A trailing comma or a blank between separators is sloppy, not
            // wrong. Nothing is applied and nothing is complained about.
            if (separator == std::string_view::npos) {
                break;
            }
            continue;
        }

        ++examined;
        if (examined > kMaxTermsExamined) {
            ++result.rejected;
            result.rejected_terms.emplace_back("[too many terms]");
            break;
        }

        const std::size_t equals = term.find('=');
        if (equals == std::string_view::npos) {
            LogLevel level = LogLevel::Info;
            if (parse_log_level(term, level)) {
                default_level_ = level;
                ++result.applied;
            } else {
                ++result.rejected;
                result.rejected_terms.push_back(clipped(term));
            }
        } else {
            const std::string category = to_lower(trimmed(term.substr(0, equals)));
            const std::string_view level_text = trimmed(term.substr(equals + 1));

            LogLevel level = LogLevel::Info;
            if (!parse_log_level(level_text, level) ||
                !set_category_level(category, level)) {
                ++result.rejected;
                result.rejected_terms.push_back(clipped(term));
            } else {
                ++result.applied;
            }
        }

        if (separator == std::string_view::npos) {
            break;
        }
    }

    return result;
}

std::string LogLevelPolicy::to_configuration() const {
    std::string text = configured_level_name(default_level_);
    for (const CategoryLevelRule& rule : rules()) {
        text.append(", ");
        text.append(rule.category);
        text.push_back('=');
        text.append(configured_level_name(rule.level));
    }
    return text;
}

}  // namespace squiflow::platform
