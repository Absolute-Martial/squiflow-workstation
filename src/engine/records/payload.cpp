#include "engine/records/payload.hpp"

#include <cstring>
#include <limits>

namespace squiflow::engine {
namespace {

constexpr unsigned char kMagic[4] = {'S', 'Q', 'F', '1'};

void put_byte(Blob& out, unsigned char byte) { out.push_back(byte); }

void put_u16(Blob& out, std::uint16_t value) {
    put_byte(out, static_cast<unsigned char>(value & 0xFFU));
    put_byte(out, static_cast<unsigned char>((value >> 8) & 0xFFU));
}

void put_u32(Blob& out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8) {
        put_byte(out, static_cast<unsigned char>((value >> shift) & 0xFFU));
    }
}

void put_u64(Blob& out, std::uint64_t value) {
    for (int shift = 0; shift < 64; shift += 8) {
        put_byte(out, static_cast<unsigned char>((value >> shift) & 0xFFU));
    }
}

void put_bytes(Blob& out, const void* data, std::size_t length) {
    const unsigned char* start = static_cast<const unsigned char*>(data);
    out.insert(out.end(), start, start + length);
}

// A cursor that refuses to read past the end. Every read goes through it, so
// there is exactly one place where a truncated payload is caught.
class Reader {
public:
    explicit Reader(const Blob& bytes) : bytes_(bytes) {}

    void need(std::size_t count) const {
        if (bytes_.size() - position_ < count) {
            throw PayloadError("payload ends in the middle of a field");
        }
    }

    unsigned char byte() {
        need(1);
        return bytes_[position_++];
    }

    std::uint16_t u16() {
        const std::uint32_t low = byte();
        const std::uint32_t high = byte();
        return static_cast<std::uint16_t>(low | (high << 8));
    }

    std::uint32_t u32() {
        std::uint32_t value = 0;
        for (int shift = 0; shift < 32; shift += 8) {
            value |= static_cast<std::uint32_t>(byte()) << shift;
        }
        return value;
    }

    std::uint64_t u64() {
        std::uint64_t value = 0;
        for (int shift = 0; shift < 64; shift += 8) {
            value |= static_cast<std::uint64_t>(byte()) << shift;
        }
        return value;
    }

    // A length is checked against what is actually left before anything is
    // allocated. Otherwise a four-byte length field in a corrupted file asks
    // for four gigabytes and the shop's machine, which has eight, stops.
    std::size_t length() {
        const std::size_t claimed = static_cast<std::size_t>(u32());
        need(claimed);
        return claimed;
    }

    std::string text(std::size_t count) {
        need(count);
        const char* start = reinterpret_cast<const char*>(bytes_.data() + position_);
        position_ += count;
        return std::string(start, count);
    }

    Blob binary(std::size_t count) {
        need(count);
        Blob out(bytes_.begin() + static_cast<std::ptrdiff_t>(position_),
                 bytes_.begin() + static_cast<std::ptrdiff_t>(position_ + count));
        position_ += count;
        return out;
    }

    void skip(std::size_t count) {
        need(count);
        position_ += count;
    }

    std::size_t remaining() const { return bytes_.size() - position_; }

private:
    const Blob& bytes_;
    std::size_t position_{0};
};

}  // namespace

PayloadError::PayloadError(const std::string& message) : std::runtime_error(message) {}

Blob encode_payload(const Row& row) {
    Blob out;
    put_bytes(out, kMagic, sizeof(kMagic));
    put_u32(out, static_cast<std::uint32_t>(row.size()));

    for (const Row::Field& field : row.fields()) {
        if (field.first.empty()) {
            throw PayloadError("a field with no name cannot be encoded");
        }
        if (field.first.size() > std::numeric_limits<std::uint16_t>::max()) {
            throw PayloadError("field name is absurdly long: " + field.first.substr(0, 40));
        }
        put_u16(out, static_cast<std::uint16_t>(field.first.size()));
        put_bytes(out, field.first.data(), field.first.size());

        const Value& value = field.second;
        put_byte(out, static_cast<unsigned char>(value.kind()));
        switch (value.kind()) {
            case ValueKind::Null:
                break;
            case ValueKind::Integer: {
                const std::int64_t number = value.integer_or(0);
                std::uint64_t bits = 0;
                std::memcpy(&bits, &number, sizeof(bits));
                put_u64(out, bits);
                break;
            }
            case ValueKind::Real: {
                const double number = value.as_real().value_or(0.0);
                std::uint64_t bits = 0;
                std::memcpy(&bits, &number, sizeof(bits));
                put_u64(out, bits);
                break;
            }
            case ValueKind::Text: {
                const std::string* text = value.as_text();
                const std::string empty;
                const std::string& stored = text != nullptr ? *text : empty;
                put_u32(out, static_cast<std::uint32_t>(stored.size()));
                put_bytes(out, stored.data(), stored.size());
                break;
            }
            case ValueKind::Binary: {
                const Blob* binary = value.as_binary();
                const Blob empty;
                const Blob& stored = binary != nullptr ? *binary : empty;
                put_u32(out, static_cast<std::uint32_t>(stored.size()));
                put_bytes(out, stored.data(), stored.size());
                break;
            }
        }
    }

    return out;
}

Row decode_payload(const Blob& bytes) {
    Reader reader(bytes);
    reader.need(sizeof(kMagic));
    for (const unsigned char expected : kMagic) {
        if (reader.byte() != expected) {
            throw PayloadError("this is not a call payload");
        }
    }

    const std::uint32_t count = reader.u32();

    // A field costs at least four bytes on the wire, so a count larger than
    // what remains could divide by is a lie and can be refused before a single
    // allocation. Cheap, and it turns a memory exhaustion into an error
    // message.
    if (static_cast<std::size_t>(count) > reader.remaining()) {
        throw PayloadError("payload claims more fields than it could contain");
    }

    Row row;
    for (std::uint32_t index = 0; index < count; ++index) {
        const std::uint16_t name_length = reader.u16();
        if (name_length == 0) {
            throw PayloadError("payload contains a field with no name");
        }
        const std::string name = reader.text(name_length);
        if (row.has(name)) {
            throw PayloadError("payload names the field '" + name + "' twice");
        }

        const unsigned char kind = reader.byte();
        switch (kind) {
            case static_cast<unsigned char>(ValueKind::Null):
                row.set(name, Value::null());
                break;
            case static_cast<unsigned char>(ValueKind::Integer): {
                const std::uint64_t bits = reader.u64();
                std::int64_t number = 0;
                std::memcpy(&number, &bits, sizeof(number));
                row.set(name, Value::integer(number));
                break;
            }
            case static_cast<unsigned char>(ValueKind::Real): {
                const std::uint64_t bits = reader.u64();
                double number = 0.0;
                std::memcpy(&number, &bits, sizeof(number));
                row.set(name, Value::real(number));
                break;
            }
            case static_cast<unsigned char>(ValueKind::Text): {
                const std::size_t length = reader.length();
                row.set(name, Value::text(reader.text(length)));
                break;
            }
            case static_cast<unsigned char>(ValueKind::Binary): {
                const std::size_t length = reader.length();
                row.set(name, Value::binary(reader.binary(length)));
                break;
            }
            default:
                throw PayloadError("payload field '" + name + "' has an unknown kind");
        }
    }

    if (reader.remaining() != 0) {
        throw PayloadError("payload has bytes after the last field");
    }

    return row;
}

Blob encode_field(const std::string& column, const std::string& value) {
    Row row;
    row.set(column, Value::text(value));
    return encode_payload(row);
}

}  // namespace squiflow::engine
