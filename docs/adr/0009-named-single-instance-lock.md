# ADR 0009: Named single-instance ownership

Status: Accepted, 2026-08-03

## Decision

SquiFlow acquires a machine-wide named mutex before opening the database. The name contains a bounded hash of the normalized data directory, so different databases do not block each other and customer paths are not disclosed. A named auto-reset event carries second-launch activation requests. The second process signals and exits without touching the database; the primary consumes the request without a dedicated thread. Phase 6.8 connects it to window raising.

Windows grants authenticated machine users access to these kernel objects because the database is machine-wide. `WAIT_ABANDONED` transfers ownership with an explicit recovery finding; indeterminate errors fail closed. The Linux verification adapter uses `flock`, close-on-exec descriptors, no-follow opens and a bounded activation sidecar. Ownership is always a kernel lock, never file existence.

## Consequences

The lock is startup step 4, after crash handling and before database open. Windows code is structurally reviewed but cannot be executed in this Linux lane. The POSIX adapter and fake provide deterministic lifecycle, exclusion, activation, process-death and hostile-path verification.
