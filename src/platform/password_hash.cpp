#include "platform/password_hash.hpp"

#include <array>
#include <mutex>
#include <sodium.h>
#include <stdexcept>

namespace squiflow::platform {
namespace {

void initialize_sodium() {
    static std::once_flag once;
    static int result = -1;
    std::call_once(once, [] { result = sodium_init(); });
    if (result < 0) {
        throw std::runtime_error("Secure memory initialization failed.");
    }
}

}  // namespace

PasswordHashResult hash_password(std::string_view plaintext) {
    PasswordHashResult result;
    if (plaintext.empty()) {
        result.fault = PasswordHashFault::EmptyPassword;
        result.message = "Password must not be empty.";
        return result;
    }
    if (plaintext.size() > kMaxPasswordBytes) {
        result.fault = PasswordHashFault::PasswordTooLong;
        result.message = "Password exceeds the maximum accepted length.";
        return result;
    }
    initialize_sodium();
    static_assert(kMaxPasswordHashBytes >= crypto_pwhash_STRBYTES,
                  "password hash buffer must fit the libsodium encoding");
    std::array<char, crypto_pwhash_STRBYTES> encoded{};
    const int rc = crypto_pwhash_str(
        encoded.data(), plaintext.data(), plaintext.size(),
        crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE);
    if (rc != 0) {
        result.fault = PasswordHashFault::HashingFailed;
        result.message =
            "Password hashing failed; the machine may be low on memory.";
        return result;
    }
    result.ok = true;
    result.hash.assign(encoded.data());
    return result;
}

bool verify_password(std::string_view plaintext, std::string_view encoded_hash) {
    if (plaintext.empty() || encoded_hash.empty()) {
        return false;
    }
    if (plaintext.size() > kMaxPasswordBytes ||
        encoded_hash.size() >= kMaxPasswordHashBytes) {
        return false;
    }
    initialize_sodium();
    // crypto_pwhash_str_verify requires a NUL-terminated C string; encoded
    // hashes are always short, so a stack-independent std::string round trip
    // is negligible next to the hashing work itself.
    const std::string terminated(encoded_hash);
    return crypto_pwhash_str_verify(terminated.c_str(), plaintext.data(),
                                    plaintext.size()) == 0;
}

}  // namespace squiflow::platform
