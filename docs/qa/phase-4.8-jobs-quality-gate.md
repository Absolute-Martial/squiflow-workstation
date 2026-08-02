# Phase 4.8 Jobs Quality Gate

Status: **PASS - PHASE 4.8 APPROVED**

Verified: 2026-08-02 with GCC 11.5.0, C++23, full warnings as errors.

## Scope

Migration 16; job records that may exist with no order; thin-job visibility; printable ticket identity; remembered selling snapshot; five independent progress axes; delivery evidence; reprint evidence; strict payload decoding; idempotent synchronizable writes; and exact registration of the four declared jobs operations.

No workflow-owned order-to-job, quotation-to-job, proof-approval workflow, phase-billing workflow, or files module behavior is registered here. Those remain later phases.

Supplier credit remains Phase 4.11 sourcing: purchase paid/still-owed state, settlement date once cleared, and one outstanding-suppliers screen. It is not a jobs or receivables feature.

## Test plan and execution

1. Strict domain compile with warnings as errors.
2. Header-alone-twice checks.
3. Adversarial jobs domain probe.
4. Migration/repository/service/module compile gates.
5. Permanent jobs module suite covering all four operations.
6. Full strict regression through the sandbox Makefile lane.
7. Independent CMake configure/build/test lane.
8. Recovery verification after sandbox reset using the verified Phase 4.7 checkpoint.

## Results

- Jobs domain probe: 23 checks, 0 failed.
- Permanent jobs suite: 32 checks, 0 failed.
- Strict full regression: 2,653 assertions, 0 failed.
- Integrity: 168 files, all clean.
- Header self-containment: 72 headers, 0 failures.
- Strict test programs: 16.
- CMake: 15/15 tests passed.
- Protocol: 12 modules, 44 rights, 67 operations.
- Jobs module migration: 16.
- Every declared jobs operation is registered and exercised.
- No Phase 5 workflow operation was added or registered.

## Issues found and fixed

### QA-4.8-01 — Minor — boolean payload reader used the wrong store accessor

**Root cause:** the jobs service initially used a non-existent boolean accessor instead of the store's documented boolean decoding path.

**Fix:** switched to the documented value-kind check plus `boolean_or(...)`. The strict service compile gate then passed.

### QA-4.8-02 — Minor — permanent jobs test used outdated harness names

**Root cause:** the test initially used `duplicate` instead of `replayed`, and `done()` instead of `report()`.

**Fix:** patched the test to the live `Outcome` and `support/check.hpp` APIs. The strict jobs module gate then passed at 32 checks, 0 failed.

### QA-4.8-03 — Environment recovery — sandbox reset removed the workspace

**Root cause:** the sandbox reset mid-phase, removing `/data/squiflow-workstation` and preventing the first full Phase 4.8 strict run.

**Fix:** restored the verified `squiflow-phase4.7-receivables-QA-approved.zip` checkpoint, re-applied the complete Phase 4.8 jobs work, reran the module gate, reran the full strict regression, and reran the CMake lane. Only post-recovery green runs are counted as approval evidence.

## Remaining risks and limits

- Jobs are intentionally a module-only phase here. Cross-module workflows such as quotation-to-job, order-to-job, proof approval, and phase billing remain later work.
- The current jobs module preserves references like `source_order_id`, `source_quotation_id`, and `proof_approval_ref` as evidence fields without owning those richer workflows yet.
- The CMake verification lane remains non-shippable for storage because SQLite is off in this sandbox (`SQUIFLOW_WITH_SQLITE=OFF`).
- The provided native-workstation build sources are preserved under `third_party/provided-build-sources/`, but Qt/Windows shell and native workstation build integration remain later platform and UI phases.

## Final verdict

**PASS.** No critical or major Phase 4.8 jobs defects remain open. Required gates are green, the four jobs operations are registered and exercised, the full strict regression passes, and the independent CMake lane passes.
