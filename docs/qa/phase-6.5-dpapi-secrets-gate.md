# Phase 6.5 DPAPI secrets gate

Verified 2026-08-03.

## Implemented

- current-user DPAPI; machine scope prohibited;
- bounded versioned protected envelope;
- validated non-path secret identities;
- same-directory write, flush, atomic replacement, and rollback;
- move-only libsodium guarded plaintext memory;
- deterministic failure-injectable fake;
- dependency-free tests plus GoogleTest 1.17.0;
- offline-pinned libsodium and GoogleTest archives with packaging notices.

## Evidence

- secret gate: 30 checks, 0 failed;
- full strict gate: 5,152 assertions, 0 failed;
- independent CMake build completed;
- CTest: 31/31 passed;
- libsodium SHA-256: `adbdd8f16149e81ac6078a03aca6fc03b592b89ef7b5ed83841c086191be3349`;
- GoogleTest SHA-256: `65fab701d9829d38cb77c14acdc431d2108bfdbf8979e40eb8ae567edf10b27c`.

Windows DPAPI execution is not claimed from the Linux host. Windows CI remains mandatory before release.
