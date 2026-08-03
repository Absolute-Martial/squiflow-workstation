# ADR 0010: User-bound DPAPI secrets with guarded memory

Status: Accepted

## Decision

Persist credentials only after current-user `CryptProtectData` protection, recover them with `CryptUnprotectData`, always pass `CRYPTPROTECT_UI_FORBIDDEN`, and never use `CRYPTPROTECT_LOCAL_MACHINE`. Persist a bounded, versioned envelope through same-directory temporary files, `WriteFile`, `FlushFileBuffers`, and write-through atomic replacement.

Plaintext lives in the move-only `SecretBuffer`, allocated with libsodium 1.0.22 `sodium_malloc`, erased with `sodium_memzero`, and released with `sodium_free`. GoogleTest 1.17.0 supplements the dependency-free security gate.

## Consequences

Secrets are bound to the current Windows account. Failed writes or replacement preserve the previous credential. Linux verifies guarded memory, envelope validation, fakes, rollback, and build wiring; the DPAPI path still requires Windows CI execution before release.
