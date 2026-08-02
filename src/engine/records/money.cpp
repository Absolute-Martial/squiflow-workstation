#include "engine/records/money.hpp"

#include <limits>

namespace squiflow::engine {
namespace {

constexpr std::int64_t kMax = std::numeric_limits<std::int64_t>::max();
constexpr std::int64_t kMin = std::numeric_limits<std::int64_t>::min();

bool add_checked(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if (b > 0 && a > kMax - b) {
        return false;
    }
    if (b < 0 && a < kMin - b) {
        return false;
    }
    out = a + b;
    return true;
}

// Portable checked multiply. No compiler builtin and no 128-bit type, because
// this has to compile with the Windows compiler too.
bool multiply_checked(std::uint64_t a, std::uint64_t b, std::uint64_t& out) noexcept {
    if (a != 0 && b > std::numeric_limits<std::uint64_t>::max() / a) {
        return false;
    }
    out = a * b;
    return true;
}

std::uint64_t magnitude_of(std::int64_t value) noexcept {
    return value < 0 ? (~static_cast<std::uint64_t>(value) + 1U)
                     : static_cast<std::uint64_t>(value);
}

}  // namespace

MoneyResult money_add(Money a, Money b) noexcept {
    std::int64_t sum = 0;
    if (!add_checked(a.minor, b.minor, sum)) {
        return {};
    }
    return {true, Money{sum}};
}

MoneyResult money_negate(Money a) noexcept {
    if (a.minor == kMin) {
        return {};
    }
    return {true, Money{-a.minor}};
}

MoneyResult money_subtract(Money a, Money b) noexcept {
    const MoneyResult negated = money_negate(b);
    if (!negated.ok) {
        return {};
    }
    return money_add(a, negated.value);
}

MoneyResult money_multiply(Money rate, Quantity quantity) noexcept {
    const bool negative = (rate.minor < 0) != (quantity.scaled < 0);

    std::uint64_t product = 0;
    if (!multiply_checked(magnitude_of(rate.minor), magnitude_of(quantity.scaled),
                          product)) {
        return {};
    }

    const std::uint64_t scale = static_cast<std::uint64_t>(Quantity::kScale);
    const std::uint64_t whole = product / scale;
    const std::uint64_t remainder = product % scale;

    // Half away from zero.
    std::uint64_t rounded = whole;
    if (remainder * 2U >= scale) {
        if (rounded == std::numeric_limits<std::uint64_t>::max()) {
            return {};
        }
        ++rounded;
    }

    const std::uint64_t limit =
        negative ? magnitude_of(kMin) : static_cast<std::uint64_t>(kMax);
    if (rounded > limit) {
        return {};
    }

    if (negative) {
        if (rounded == magnitude_of(kMin)) {
            return {true, Money{kMin}};
        }
        return {true, Money{-static_cast<std::int64_t>(rounded)}};
    }
    return {true, Money{static_cast<std::int64_t>(rounded)}};
}

std::string format(Money value) {
    const bool negative = value.minor < 0;
    const std::uint64_t magnitude = magnitude_of(value.minor);

    const std::uint64_t unit = static_cast<std::uint64_t>(Money::kMinorPerUnit);
    const std::uint64_t whole = magnitude / unit;
    const std::uint64_t fraction = magnitude % unit;

    std::string digits = std::to_string(whole);

    // Grouping in threes. South Asian lakh grouping is a display convention
    // that belongs in the interface, not in the value type, so this stays
    // plain and the interface may regroup it.
    std::string grouped;
    const std::size_t count = digits.size();
    for (std::size_t i = 0; i < count; ++i) {
        if (i != 0 && ((count - i) % 3) == 0) {
            grouped.push_back(',');
        }
        grouped.push_back(digits[i]);
    }

    std::string out;
    if (negative) {
        out.push_back('-');
    }
    out += grouped;
    out.push_back('.');
    std::string fraction_digits = std::to_string(fraction);
    while (fraction_digits.size() < 2) {
        fraction_digits.insert(fraction_digits.begin(), '0');
    }
    out += fraction_digits;
    return out;
}

MoneyResult parse_money(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    std::size_t i = 0;
    bool negative = false;
    if (text[i] == '-' || text[i] == '+') {
        negative = text[i] == '-';
        ++i;
    }

    std::uint64_t whole = 0;
    bool any_digit = false;
    for (; i < text.size() && text[i] != '.'; ++i) {
        if (text[i] == ',') {
            continue;  // people paste grouped numbers
        }
        if (text[i] < '0' || text[i] > '9') {
            return {};
        }
        any_digit = true;
        whole = whole * 10U + static_cast<std::uint64_t>(text[i] - '0');
        if (whole > 90000000000000ULL) {
            return {};
        }
    }
    if (!any_digit) {
        return {};
    }

    std::uint64_t fraction = 0;
    std::size_t decimals = 0;
    if (i < text.size() && text[i] == '.') {
        ++i;
        for (; i < text.size(); ++i) {
            if (text[i] < '0' || text[i] > '9') {
                return {};
            }
            if (decimals == 2) {
                return {};
            }
            fraction = fraction * 10U + static_cast<std::uint64_t>(text[i] - '0');
            ++decimals;
        }
    }
    while (decimals < 2) {
        fraction *= 10U;
        ++decimals;
    }

    const std::uint64_t magnitude =
        whole * static_cast<std::uint64_t>(Money::kMinorPerUnit) + fraction;
    if (magnitude > static_cast<std::uint64_t>(kMax)) {
        return {};
    }

    const std::int64_t minor = static_cast<std::int64_t>(magnitude);
    return {true, Money{negative ? -minor : minor}};
}

}  // namespace squiflow::engine
