// Phase 7.7 foundation: turning a shop password into something safe to keep.
//
// The interesting cases are the ones a login screen actually depends on: the
// same plaintext verifies every time, a different plaintext never does, two
// hashes of the same plaintext are never byte-identical (a fresh salt every
// time), and every malformed or foreign input a caller could hand back after
// a database round trip is rejected rather than crashing the sign-in screen.

#include <string>

#include "platform/password_hash.hpp"
#include "support/check.hpp"

namespace {

namespace platform = squiflow::platform;
using squiflow::testing::check;
using squiflow::testing::section;

void same_plaintext_round_trips() {
    section("same plaintext round-trips");
    const platform::PasswordHashResult hashed = platform::hash_password("correct horse battery staple");
    check(hashed.ok, "hashing a reasonable password succeeds");
    check(!hashed.hash.empty(), "a successful hash is not empty");
    check(platform::verify_password("correct horse battery staple", hashed.hash),
          "the same plaintext verifies against its own hash");
}

void different_plaintext_is_rejected() {
    section("a different plaintext never verifies");
    const platform::PasswordHashResult hashed = platform::hash_password("shop-owner-password");
    check(hashed.ok, "hashing succeeds");
    check(!platform::verify_password("shop-owner-passwore", hashed.hash),
          "a one-character difference is rejected");
    check(!platform::verify_password("", hashed.hash), "an empty attempt is rejected");
    check(!platform::verify_password("Shop-Owner-Password", hashed.hash),
          "case differences are rejected");
}

void every_hash_carries_a_fresh_salt() {
    section("hashing the same password twice never repeats a salt");
    const platform::PasswordHashResult first = platform::hash_password("repeat-me");
    const platform::PasswordHashResult second = platform::hash_password("repeat-me");
    check(first.ok && second.ok, "both attempts succeed");
    check(first.hash != second.hash, "two hashes of the same password are never identical");
    check(platform::verify_password("repeat-me", first.hash), "the first hash still verifies");
    check(platform::verify_password("repeat-me", second.hash), "the second hash still verifies");
}

void empty_and_oversized_passwords_are_refused() {
    section("empty and oversized passwords are refused before hashing");
    const platform::PasswordHashResult empty_result = platform::hash_password("");
    check(!empty_result.ok, "an empty password is refused");
    check(empty_result.fault == platform::PasswordHashFault::EmptyPassword,
          "the fault names the empty password");

    const std::string oversized(platform::kMaxPasswordBytes + 1, 'a');
    const platform::PasswordHashResult oversized_result = platform::hash_password(oversized);
    check(!oversized_result.ok, "a password past the accepted length is refused");
    check(oversized_result.fault == platform::PasswordHashFault::PasswordTooLong,
          "the fault names the oversized password");

    const std::string boundary(platform::kMaxPasswordBytes, 'a');
    check(platform::hash_password(boundary).ok, "a password at exactly the limit is accepted");
}

void malformed_hashes_fail_closed() {
    section("malformed or foreign hashes fail closed rather than crashing");
    check(!platform::verify_password("anything", ""), "an empty stored hash never verifies");
    check(!platform::verify_password("anything", "not-an-argon2-string"),
          "a non-libsodium string never verifies");
    check(!platform::verify_password("anything", "$2y$10$notArgon2EitherJustBcryptShaped"),
          "a foreign-algorithm hash never verifies");
    const std::string oversized_hash(platform::kMaxPasswordHashBytes, 'x');
    check(!platform::verify_password("anything", oversized_hash),
          "a hash past the accepted length is rejected outright");
}

}  // namespace

int main() {
    same_plaintext_round_trips();
    different_plaintext_is_rejected();
    every_hash_carries_a_fresh_salt();
    empty_and_oversized_passwords_are_refused();
    malformed_hashes_fail_closed();
    return squiflow::testing::report();
}
