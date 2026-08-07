#include "identity/token_issuer.hpp"

#include <sodium.h>

#include <array>
#include <chrono>
#include <mutex>

namespace squiflow::server::identity {

namespace {

constexpr std::size_t kIdBytes = 16;      // 128 bits: enough to make lookup
                                           // collisions negligible; not the
                                           // secret, so this is not the
                                           // entropy budget that matters.
constexpr std::size_t kSecretBytes = 32;  // 256 bits of CSPRNG entropy.
constexpr std::size_t kHashBytes = 32;    // BLAKE2b-256 digest.
constexpr char kHex[] = "0123456789abcdef";

void ensure_sodium_initialized() {
    static std::once_flag once;
    std::call_once(once, [] {
        // sodium_init() returns 0 on first success, 1 if already
        // initialized, -1 on failure. Either non-negative result is fine;
        // failure here would mean the CSPRNG cannot be trusted, which every
        // caller of this module needs to know about immediately rather than
        // silently issuing a weak token.
        if (sodium_init() < 0) {
            std::terminate();
        }
    });
}

std::string hex_encode(const unsigned char* data, std::size_t size) {
    std::string result;
    result.resize(size * 2);
    for (std::size_t i = 0; i < size; ++i) {
        result[2 * i] = kHex[(data[i] >> 4) & 0x0F];
        result[2 * i + 1] = kHex[data[i] & 0x0F];
    }
    return result;
}

bool hex_decode(std::string_view text, unsigned char* out, std::size_t out_size) {
    if (text.size() != out_size * 2) {
        return false;
    }
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        return -1;
    };
    for (std::size_t i = 0; i < out_size; ++i) {
        const int hi = nibble(text[2 * i]);
        const int lo = nibble(text[2 * i + 1]);
        if (hi < 0 || lo < 0) {
            return false;
        }
        out[i] = static_cast<unsigned char>((hi << 4) | lo);
    }
    return true;
}

std::string hash_secret(const TokenSecret& secret) {
    std::array<unsigned char, kHashBytes> digest{};
    // Unkeyed BLAKE2b is fine here: the input already has 256 bits of CSPRNG
    // entropy, so this hash's job is only "never store the bearer secret
    // itself", not "resist a dictionary".
    crypto_generichash(digest.data(), digest.size(),
                        reinterpret_cast<const unsigned char*>(secret.value.data()),
                        secret.value.size(), nullptr, 0);
    return hex_encode(digest.data(), digest.size());
}

bool secret_matches(const TokenSecret& candidate, const std::string& stored_hash) {
    if (stored_hash.size() != kHashBytes * 2) {
        return false;
    }
    const std::string candidate_hash = hash_secret(candidate);
    if (candidate_hash.size() != stored_hash.size()) {
        return false;
    }
    // Constant-time compare: validation time must not leak how many leading
    // bytes of a guessed secret were correct.
    return sodium_memcmp(candidate_hash.data(), stored_hash.data(), stored_hash.size()) == 0;
}

engine::Timestamp add_seconds(engine::Timestamp start, std::chrono::seconds seconds) {
    return engine::Timestamp{start.ms + static_cast<std::int64_t>(seconds.count()) * 1000};
}

}  // namespace

engine::Timestamp system_clock_now() {
    const auto now = std::chrono::system_clock::now();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return engine::Timestamp{ms.count()};
}

bool parse_bearer_token(std::string_view bearer, TokenId& out_id, TokenSecret& out_secret) {
    const auto separator = bearer.find('.');
    if (separator == std::string_view::npos) {
        return false;
    }
    const auto id_part = bearer.substr(0, separator);
    const auto secret_part = bearer.substr(separator + 1);
    if (id_part.size() != kIdBytes * 2 || secret_part.size() != kSecretBytes * 2) {
        return false;
    }
    std::array<unsigned char, kIdBytes> id_bytes{};
    std::array<unsigned char, kSecretBytes> secret_bytes{};
    if (!hex_decode(id_part, id_bytes.data(), id_bytes.size())) {
        return false;
    }
    if (!hex_decode(secret_part, secret_bytes.data(), secret_bytes.size())) {
        return false;
    }
    out_id = TokenId{std::string(id_part)};
    out_secret = TokenSecret{std::string(secret_part)};
    return true;
}

TokenIssuer::TokenIssuer(TokenStore& store, std::chrono::seconds ttl, ClockFn clock)
    : store_(store), ttl_(ttl), clock_(std::move(clock)) {
    ensure_sodium_initialized();
}

IssuedToken TokenIssuer::issue(engine::PersonId person, engine::DeviceId device) {
    ensure_sodium_initialized();

    std::array<unsigned char, kIdBytes> id_bytes{};
    std::array<unsigned char, kSecretBytes> secret_bytes{};
    randombytes_buf(id_bytes.data(), id_bytes.size());
    randombytes_buf(secret_bytes.data(), secret_bytes.size());

    TokenId id{hex_encode(id_bytes.data(), id_bytes.size())};
    TokenSecret secret{hex_encode(secret_bytes.data(), secret_bytes.size())};

    const auto now = clock_();
    TokenRecord record;
    record.id = id;
    record.person = person;
    record.device = device;
    record.secret_hash = hash_secret(secret);
    record.issued_at = now;
    record.expires_at = add_seconds(now, ttl_);
    record.revoked = false;
    store_.put(record);

    return IssuedToken{id, id.value + "." + secret.value, record.expires_at};
}

ValidationResult TokenIssuer::validate(std::string_view bearer) const {
    TokenId id;
    TokenSecret secret;
    if (!parse_bearer_token(bearer, id, secret)) {
        return ValidationResult{false, TokenFault::Malformed, {}, {}};
    }

    const auto record = store_.find(id);
    if (!record.has_value()) {
        return ValidationResult{false, TokenFault::NotFound, {}, {}};
    }
    if (!secret_matches(secret, record->secret_hash)) {
        // Deliberately the same fault as "not found" would be, from the
        // caller's point of view, once the HTTP layer collapses both to one
        // generic response; kept distinct here only so tests can tell which
        // code path ran.
        return ValidationResult{false, TokenFault::NotFound, {}, {}};
    }
    if (record->revoked) {
        return ValidationResult{false, TokenFault::Revoked, {}, {}};
    }
    if (clock_().ms >= record->expires_at.ms) {
        return ValidationResult{false, TokenFault::Expired, {}, {}};
    }
    return ValidationResult{true, TokenFault::None, record->person, record->device};
}

void TokenIssuer::revoke(const TokenId& id) { store_.mark_revoked(id); }

}  // namespace squiflow::server::identity
