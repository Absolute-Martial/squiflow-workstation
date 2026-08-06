#pragma once

#include "identity/token_store.hpp"

#include "engine/records/identity.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>

namespace squiflow::server::identity {

enum class TokenFault : std::uint8_t {
    None,
    Malformed,
    NotFound,
    Expired,
    Revoked,
};

struct IssuedToken {
    TokenId id;
    // The bearer string the caller stores and presents on every request.
    // Never logged, never persisted anywhere in this form.
    std::string bearer;
    engine::Timestamp expires_at;
};

struct ValidationResult {
    bool ok = false;
    TokenFault fault = TokenFault::None;
    engine::PersonId person{};
    engine::DeviceId device{};
};

using ClockFn = std::function<engine::Timestamp()>;

// Real wall-clock time, in the same UTC-millisecond form every other Phase 2
// timestamp uses.
engine::Timestamp system_clock_now();

// Issues, validates, and revokes opaque bearer tokens tied to a person and
// device, per Phase 8.3.
//
// Design notes (why this shape, not another):
// - The bearer string is "<id>.<secret>", where `id` is a non-secret lookup
//   key and `secret` is the actual 256-bit random value. Splitting them lets
//   validate() find the candidate record in O(1) before doing any hashing or
//   comparison, and lets revoke() work from `id` alone without ever needing
//   the secret again.
// - The secret is never derivable from the person or device id: it comes
//   from a CSPRNG (libsodium randombytes_buf), never from hashing identity
//   fields.
// - Only a hash of the secret is stored (TokenRecord::secret_hash). A stolen
//   TokenStore backup cannot be replayed as a bearer token.
// - The stored hash is a fast cryptographic hash (BLAKE2b via libsodium
//   crypto_generichash), not a slow password KDF: the secret already has 256
//   bits of entropy from a CSPRNG, so brute-forcing the hash is infeasible,
//   and a slow KDF would only add cost to every request for no security
//   benefit here. This is the standard shape for opaque API/session tokens
//   (as opposed to low-entropy user passwords, which do need a slow KDF).
// - Comparison of the presented secret's hash against the stored hash uses a
//   constant-time compare (libsodium sodium_memcmp) so validation timing
//   cannot be used as an oracle to guess a valid secret one byte at a time.
class TokenIssuer {
public:
    TokenIssuer(TokenStore& store, std::chrono::seconds ttl,
                ClockFn clock = system_clock_now);

    IssuedToken issue(engine::PersonId person, engine::DeviceId device);

    // Never throws and never returns a fault-specific message the caller can
    // use to distinguish "no such id" from "wrong secret" from "expired" by
    // response shape alone; `fault` is for server-side logging/tests only.
    // The HTTP layer (deferred to when 8.1 unblocks) must map every non-ok
    // result to the same generic 401 response.
    ValidationResult validate(std::string_view bearer) const;

    void revoke(const TokenId& id);

private:
    TokenStore& store_;
    std::chrono::seconds ttl_;
    ClockFn clock_;
};

// Exposed for tests: splits "<id>.<secret>" into its parts. Returns false
// (never throws) when `bearer` is not well-formed.
bool parse_bearer_token(std::string_view bearer, TokenId& out_id, TokenSecret& out_secret);

}  // namespace squiflow::server::identity
