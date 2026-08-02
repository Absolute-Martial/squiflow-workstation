#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace squiflow::engine {

// Document numbers come from a block reserved for this device by the server.
//
// The consequence, decided deliberately: gaps are legitimate. Device A holds
// 1-500 and device B holds 501-1000, so the shop will see 12, 13, 504, 14. A
// numbering scheme that never has gaps would need a round trip to the server
// for every document, which means no invoice can be written while the
// connection is down. Gaps are the cheaper price.
class NumberBlock {
  public:
    NumberBlock() = default;
    NumberBlock(std::uint64_t first, std::uint64_t last) noexcept;

    // Nothing left means the device must top up before issuing again.
    std::optional<std::uint64_t> allocate() noexcept;

    std::uint64_t remaining() const noexcept;
    bool exhausted() const noexcept { return remaining() == 0; }

    // A block running low is an attention item, raised early enough that the
    // top-up happens while the connection is still up.
    bool low(std::uint64_t threshold) const noexcept {
        return remaining() <= threshold;
    }

    std::uint64_t next() const noexcept { return next_; }
    std::uint64_t last() const noexcept { return last_; }

  private:
    std::uint64_t next_ = 1;
    std::uint64_t last_ = 0;
};

// A number that was taken and then cancelled. It is recorded rather than
// released: reusing it would make two different documents share a number, and
// one of them is in a customer's hands.
struct BurnedNumber {
    std::string series;
    std::uint64_t number = 0;
};

// "INV", 42, 6 -> "INV-000042"
std::string format_number(std::string_view series, std::uint64_t number,
                          std::size_t width);

}  // namespace squiflow::engine
