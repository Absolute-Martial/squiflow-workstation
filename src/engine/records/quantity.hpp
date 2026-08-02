#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace squiflow::engine {

// A quantity, held as thousandths. Three decimal places covers square feet of
// flex to the nearest thousandth and every countable thing exactly.
//
// Never a floating point number. A shop that adds up a hundred line items with
// doubles will eventually print a total that is one paisa wrong, and the
// customer will be the one who notices.
struct Quantity {
    std::int64_t scaled = 0;

    static constexpr std::int64_t kScale = 1000;

    static constexpr Quantity from_whole(std::int64_t whole) noexcept {
        return Quantity{whole * kScale};
    }

    constexpr bool is_zero() const noexcept { return scaled == 0; }
    constexpr bool is_negative() const noexcept { return scaled < 0; }

    friend constexpr auto operator<=>(const Quantity&, const Quantity&) = default;
    friend constexpr bool operator==(const Quantity&, const Quantity&) = default;
};

struct QuantityResult {
    bool ok = false;
    Quantity value;
};

QuantityResult quantity_add(Quantity a, Quantity b) noexcept;
QuantityResult quantity_subtract(Quantity a, Quantity b) noexcept;

// Trailing zeros are trimmed, so 5.000 prints as 5 and 2.500 as 2.5. A job
// ticket reading "5.000 pieces" looks like a machine wrote it.
std::string format(Quantity value);

// Accepts "12", "12.5", "12.500", "-3.25". More than three decimals is a
// refusal, not a silent round: the person typed something the shop cannot
// represent and should be told.
QuantityResult parse_quantity(std::string_view text);

}  // namespace squiflow::engine
