#pragma once

// Which categories are allowed to speak, and how loudly.
//
// One global minimum level is the wrong instrument for support work. When a
// shop reports that stock counts drift after a sync, the useful answer is to
// turn `sync` up to Debug for an afternoon and leave everything else at Info.
// Turning the whole application up instead buries the interesting lines under
// thousands of routine ones and burns the log budget before the problem
// happens again.
//
// Matching is by dotted prefix, longest wins. A rule on `storage` covers
// `storage.migrate` and `storage.backup`; a rule on `storage.migrate` beats it
// for that one category. The prefix must end on a dot boundary, so `storage`
// never accidentally captures `storagecleanup`.
//
// This class holds no lock. It is owned by `Logger`, which serialises access,
// and it is deliberately usable on its own in a test without a logger, a sink,
// a clock, or a file.

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "platform/log_record.hpp"

namespace squiflow::platform {

// A support engineer needs a handful of overrides, not a routing table. The
// bound exists so that a corrupted or hostile settings string cannot make the
// logger allocate without limit, and so the lookup stays a short linear scan
// over cache-friendly data rather than a hash map on the logging path.
inline constexpr std::size_t kMaxLogCategoryRules = 32;

struct CategoryLevelRule {
    std::string category;
    LogLevel level = LogLevel::Info;
};

// The outcome of applying a configuration string. Reported rather than thrown,
// because a mistyped setting must never stop the application from starting;
// the unreadable parts are dropped and named, and the rest still applies.
struct LevelConfigurationResult {
    std::size_t applied = 0;
    std::size_t rejected = 0;
    // Every term that could not be used, in the order it was met, so the
    // startup log can say exactly what was ignored. Bounded by the number of
    // terms in the input, which the parser itself bounds.
    std::vector<std::string> rejected_terms;

    bool fully_understood() const { return rejected == 0; }
};

class LogLevelPolicy {
public:
    explicit LogLevelPolicy(LogLevel default_level = LogLevel::Info);

    void set_default_level(LogLevel level);
    LogLevel default_level() const;

    // Adds or replaces one rule. Returns false, changing nothing, when the
    // category is empty, longer than `kMaxLogCategoryLength`, contains a
    // character that cannot appear in a category, or when the table is full
    // and this would be a new entry. A refused rule is never silently a
    // partial rule.
    bool set_category_level(std::string_view category, LogLevel level);

    // Removes one rule. False when there was nothing to remove.
    bool clear_category_level(std::string_view category);

    void clear_all_category_levels();

    // The level in force for this category: the longest matching dotted
    // prefix, or the default when nothing matches.
    LogLevel level_for(std::string_view category) const;

    // The question the logger actually asks.
    bool is_enabled(std::string_view category, LogLevel level) const;

    std::size_t rule_count() const;

    // Rules in category order, so that reports and round-trips are stable
    // rather than dependent on the order they were configured in.
    std::vector<CategoryLevelRule> rules() const;

    // Applies a settings string such as "sync=debug, storage.migrate=warn".
    // Separators are commas or semicolons; whitespace around terms is ignored;
    // level names are case-insensitive. A term without `=` sets the default
    // level, so "debug" alone means what a reader expects it to mean.
    //
    // Existing rules are replaced only if the string is applied in full or in
    // part: this is an edit, not a reset, so call `clear_all_category_levels`
    // first if a clean slate is wanted.
    LevelConfigurationResult apply_configuration(std::string_view text);

    // The inverse: a string that `apply_configuration` would turn back into
    // this policy. Written to the log at startup so that a support file states
    // its own verbosity, and used by the tests to prove the round trip.
    std::string to_configuration() const;

private:
    // Linear search over at most `kMaxLogCategoryRules` entries. Measured
    // against nothing, because at this size a map would be slower and would
    // allocate per node; if the bound ever rises, this is the place to look.
    const CategoryLevelRule* find_exact(std::string_view category) const;

    LogLevel default_level_;
    std::vector<CategoryLevelRule> rules_;
};

// True when a category is well formed: non-empty, within
// `kMaxLogCategoryLength`, made of lowercase letters, digits, underscore and
// dots, with no leading, trailing, or doubled dot. Exposed because the same
// question is asked when parsing configuration and when a caller registers a
// rule directly.
bool is_valid_log_category(std::string_view category);

}  // namespace squiflow::platform
