#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "engine/records/quantity.hpp"

namespace squiflow::engine {

// Money, in the smallest unit of the currency. One currency, decided already:
// there is no multi-currency support and adding it later is a schema change,
// not a setting.
//
// Integer only. Every arithmetic operation is checked and returns whether it
// succeeded, because an overflow that silently wraps is a wrong invoice.
struct Money {
    std::int64_t minor = 0;

    static constexpr std::int64_t kMinorPerUnit = 100;

    static constexpr Money from_units(std::int64_t units) noexcept {
        return Money{units * kMinorPerUnit};
    }

    constexpr bool is_zero() const noexcept { return minor == 0; }
    constexpr bool is_negative() const noexcept { return minor < 0; }

    friend constexpr auto operator<=>(const Money&, const Money&) = default;
    friend constexpr bool operator==(const Money&, const Money&) = default;
};

struct MoneyResult {
    bool ok = false;
    Money value;
};

MoneyResult money_add(Money a, Money b) noexcept;
MoneyResult money_subtract(Money a, Money b) noexcept;
MoneyResult money_negate(Money a) noexcept;

// rate x quantity, rounded half away from zero to the smallest unit.
//
// Half away from zero is the rule a shopkeeper applies by hand, and matching
// the hand calculation matters more than matching a banker's convention that
// nobody at the counter would recognise.
MoneyResult money_multiply(Money rate, Quantity quantity) noexcept;

// Always two decimals, always grouped, never a currency symbol: the symbol is
// a display decision belonging to the branding package.
std::string format(Money value);

// Accepts "1200", "1200.50", "-40.05". Refuses more than two decimals.
MoneyResult parse_money(std::string_view text);

}  // namespace squiflow::engine
