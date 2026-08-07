# Phase 8.3 -- Identity and tokens, portable core closed

Date: 2026-08-06

## Scope and decision rule

Phase 8 does not compile end-to-end in this sandbox (no PostgreSQL, no
HTTP framework, no network). Rather than block all of 8.3 on that, the
framework-independent half was split out and built now: bearer-token
issuance, hashing, and constant-time validation, expressed against an
abstract storage seam instead of a concrete database. Everything that
touches PostgreSQL or the HTTP layer is honestly marked `[~]` and deferred.

## What was added

- `server/src/identity/token_store.hpp/.cpp`: `TokenId`, `TokenSecret`,
  `TokenRecord`, an abstract `TokenStore` (`put`/`find`/`mark_revoked`),
  and a thread-safe `InMemoryTokenStore` used by tests and, later, as a
  substitute for the PostgreSQL-backed store until 8.2 lands.
- `server/src/identity/token_issuer.hpp/.cpp`: `TokenFault`
  (`None/Malformed/NotFound/Expired/Revoked`, for internal tests and
  logging only -- the future HTTP layer must collapse every non-ok result
  to one generic response, so this enum never reaches a client),
  `IssuedToken`, `ValidationResult`, an injectable `ClockFn` plus
  `system_clock_now()`, and `TokenIssuer` itself. Bearer format is
  `"<id>.<secret>"`: a 128-bit non-secret lookup id and a 256-bit CSPRNG
  secret (libsodium `randombytes_buf`) that is never persisted directly --
  only its BLAKE2b hash (`crypto_generichash`) is stored, compared with
  constant-time `sodium_memcmp`. Reuses `squiflow::engine::PersonId`,
  `DeviceId`, and `Timestamp` from `src/engine/records/identity.hpp`
  instead of re-deriving a second identity system.
- `server/tests/identity/token_issuer_test.cpp`: 22 checks covering the
  normal issue/validate/revoke cycle, expired-token rejection, immediate
  revoked-token rejection, malformed-token rejection without an oracle for
  guessing valid tokens, concurrent revocation and validation of the same
  token, and that re-registering a device issues a genuinely new token
  rather than un-revoking the old one. One assertion is worth calling out:
  checking that the bearer string never leaks the raw person id must
  compare against `squiflow::engine::to_string(person)` (the full 32-hex
  encoding), not a short substring like `std::to_string(person.low)` --
  short numeric substrings turn up in random hex constantly and produce a
  flaky false failure.
- Wired into `tools/sandbox/Makefile` (`SERVER_IDENTITY_SRC`,
  `server_identity_token_issuer_test`, added to the `tests` aggregate and
  the strict-gate run/echo list) so this lane runs on every gate, not just
  on request.

## What is still open

- `TokenStore` has no PostgreSQL-backed implementation yet -- that is
  8.2's job, and this seam exists specifically so 8.2 can slot a real
  store in without touching `TokenIssuer`.
- Token rotation/revocation HTTP endpoints, and device registration wired
  to 6.4/6.5's per-machine device concept, are HTTP-layer work and wait on
  8.1.
- **D1 was open when 8.3's portable core was built; it is now resolved as
  Hical**, not Oat++ (see `docs/plan/phase-8-server-plan.md`). That
  changes nothing about the design here -- `TokenIssuer`/`TokenStore` were
  already framework-independent by construction -- it only means 8.1 can
  now actually start.

Marked `[~]` in `docs/plan/todo.md`, not `[x]`: the portable core is
tested and gated, but 8.3 as a whole isn't done until the PostgreSQL store
and the HTTP endpoints exist too.
