#include "engine/records/identity.hpp"

namespace squiflow::engine {
namespace {

constexpr char kHex[] = "0123456789abcdef";

void append_hex64(std::string& out, std::uint64_t value) {
    for (int shift = 60; shift >= 0; shift -= 4) {
        const auto nibble = static_cast<std::size_t>((value >> shift) & 0xFULL);
        out.push_back(kHex[nibble]);
    }
}

bool hex_value(char c, std::uint64_t& out) {
    if (c >= '0' && c <= '9') {
        out = static_cast<std::uint64_t>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        out = static_cast<std::uint64_t>(c - 'a') + 10U;
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        out = static_cast<std::uint64_t>(c - 'A') + 10U;
        return true;
    }
    return false;
}

}  // namespace

std::string to_string(const RecordId& id) {
    std::string out;
    out.reserve(32);
    append_hex64(out, id.high);
    append_hex64(out, id.low);
    return out;
}

RecordId record_id_from_string(std::string_view text) {
    if (text.size() != 32) {
        return {};
    }
    RecordId id;
    for (std::size_t i = 0; i < 32; ++i) {
        std::uint64_t nibble = 0;
        if (!hex_value(text[i], nibble)) {
            return {};
        }
        if (i < 16) {
            id.high = (id.high << 4) | nibble;
        } else {
            id.low = (id.low << 4) | nibble;
        }
    }
    return id;
}

}  // namespace squiflow::engine
