#include "engine/records/numbering.hpp"

namespace squiflow::engine {

NumberBlock::NumberBlock(std::uint64_t first, std::uint64_t last) noexcept
    : next_(first), last_(last) {}

std::optional<std::uint64_t> NumberBlock::allocate() noexcept {
    if (next_ > last_) {
        return std::nullopt;
    }
    return next_++;
}

std::uint64_t NumberBlock::remaining() const noexcept {
    if (next_ > last_) {
        return 0;
    }
    return last_ - next_ + 1U;
}

std::string format_number(std::string_view series, std::uint64_t number,
                          std::size_t width) {
    std::string digits = std::to_string(number);
    while (digits.size() < width) {
        digits.insert(digits.begin(), '0');
    }
    std::string out;
    out.reserve(series.size() + 1U + digits.size());
    out.append(series);
    if (!series.empty()) {
        out.push_back('-');
    }
    out += digits;
    return out;
}

}  // namespace squiflow::engine
