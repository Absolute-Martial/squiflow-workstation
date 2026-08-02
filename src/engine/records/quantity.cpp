#include "engine/records/quantity.hpp"

#include <cstdlib>
#include <limits>

namespace squiflow::engine {
namespace {

bool add_checked(std::int64_t a, std::int64_t b, std::int64_t& out) noexcept {
    if (b > 0 && a > std::numeric_limits<std::int64_t>::max() - b) {
        return false;
    }
    if (b < 0 && a < std::numeric_limits<std::int64_t>::min() - b) {
        return false;
    }
    out = a + b;
    return true;
}

}  // namespace

QuantityResult quantity_add(Quantity a, Quantity b) noexcept {
    std::int64_t sum = 0;
    if (!add_checked(a.scaled, b.scaled, sum)) {
        return {};
    }
    return {true, Quantity{sum}};
}

QuantityResult quantity_subtract(Quantity a, Quantity b) noexcept {
    if (b.scaled == std::numeric_limits<std::int64_t>::min()) {
        return {};
    }
    return quantity_add(a, Quantity{-b.scaled});
}

std::string format(Quantity value) {
    const bool negative = value.scaled < 0;
    // Taking the absolute value of the minimum would overflow, so work in
    // unsigned space.
    std::uint64_t magnitude =
        negative ? (~static_cast<std::uint64_t>(value.scaled) + 1U)
                 : static_cast<std::uint64_t>(value.scaled);

    const std::uint64_t scale = static_cast<std::uint64_t>(Quantity::kScale);
    const std::uint64_t whole = magnitude / scale;
    std::uint64_t fraction = magnitude % scale;

    std::string out;
    if (negative) {
        out.push_back('-');
    }
    out += std::to_string(whole);

    if (fraction != 0) {
        std::string digits = std::to_string(fraction);
        while (digits.size() < 3) {
            digits.insert(digits.begin(), '0');
        }
        while (!digits.empty() && digits.back() == '0') {
            digits.pop_back();
        }
        out.push_back('.');
        out += digits;
    }
    return out;
}

QuantityResult parse_quantity(std::string_view text) {
    if (text.empty()) {
        return {};
    }

    std::size_t i = 0;
    bool negative = false;
    if (text[i] == '-' || text[i] == '+') {
        negative = text[i] == '-';
        ++i;
    }
    if (i >= text.size()) {
        return {};
    }

    std::uint64_t whole = 0;
    bool any_digit = false;
    for (; i < text.size() && text[i] != '.'; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return {};
        }
        any_digit = true;
        whole = whole * 10U + static_cast<std::uint64_t>(text[i] - '0');
        if (whole > 9000000000000ULL) {
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
            if (decimals == 3) {
                // More precision than the shop can represent. Refuse rather
                // than round behind the person's back.
                return {};
            }
            fraction = fraction * 10U + static_cast<std::uint64_t>(text[i] - '0');
            ++decimals;
        }
    }
    while (decimals < 3) {
        fraction *= 10U;
        ++decimals;
    }

    const std::uint64_t magnitude =
        whole * static_cast<std::uint64_t>(Quantity::kScale) + fraction;
    if (magnitude > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        return {};
    }

    const std::int64_t scaled = static_cast<std::int64_t>(magnitude);
    return {true, Quantity{negative ? -scaled : scaled}};
}

}  // namespace squiflow::engine
