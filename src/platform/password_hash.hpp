#pragma once

// Turning a shop password into something safe to keep.
//
// This is the one file in the application that is allowed to know an Argon2id
// parameter. Everything above it (administration's Person record, the sign-in
// flow) only ever holds the opaque encoded string this file produces, and only
// ever asks this file to compare a fresh attempt against it. Password
// strength policy (minimum length, banned values) is a product decision that
// belongs to administration; this file's only job is that the same plaintext
// always verifies and a different one never does, and that a stolen database
// cannot be turned back into plaintexts without spending real machine time on
// every guess.

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace squiflow::platform {

// A shop machine, not a server farm: the length that would already be an
// unreasonable passphrase for a till login, chosen so a caller error (for
// example accidentally hashing a whole document) fails loudly instead of
// spending seconds of Argon2id time on it.
inline constexpr std::size_t kMaxPasswordBytes = 256;

// crypto_pwhash_str encodes algorithm id, cost parameters, salt and digest
// into one self-describing string no longer than crypto_pwhash_STRBYTES (128
// including the terminator). Doubled for headroom against a future algorithm
// choice without this constant becoming a second place parameters live.
inline constexpr std::size_t kMaxPasswordHashBytes = 256;

enum class PasswordHashFault : std::uint8_t {
    None,
    EmptyPassword,
    PasswordTooLong,
    HashingFailed,
};

struct PasswordHashResult {
    bool ok{false};
    std::string hash{};
    PasswordHashFault fault{PasswordHashFault::None};
    std::string message{};
};

// Hashes a plaintext password with Argon2id (libsodium crypto_pwhash_str).
// The returned string embeds the algorithm, cost parameters, and a random
// salt, so nothing outside this file ever generates or stores a salt.
PasswordHashResult hash_password(std::string_view plaintext);

// Verifies a plaintext password against a previously produced hash. Returns
// false for empty input, oversized input, and any hash that is not a genuine
// libsodium encoding, rather than throwing: every one of those is a
// "credentials do not match" outcome for a sign-in caller, not a programming
// error.
bool verify_password(std::string_view plaintext, std::string_view encoded_hash);

}  // namespace squiflow::platform
