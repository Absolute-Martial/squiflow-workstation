# Phase 4.7 Receivables Quality Gate

Status: **PASS - PHASE 4.7 APPROVED**

Verified: 2026-08-02 with GCC 11.5.0, C++23, full warnings as errors.

## Scope

Migration 15; invoice drafts and immutable issue/cancel evidence; independent payments; manual allocation and release; customer credit accounts, exposure and holds; reasoned hold-override evidence; statements, aging, receipts, print read models, and confirmed statement-delivery evidence.

Workflow-owned `issue_invoice`, `cancel_and_reissue`, and `take_payment` remain Phase 5 and are deliberately not registered here.

Supplier credit remains Phase 4.11 sourcing: purchase paid/still owed state, settlement date once cleared, and one outstanding-suppliers screen. It is not customer receivables or a supplier ledger.

## Test plan and execution

1. Strict static compilation with warnings as errors.
2. Header-alone-twice checks.
3. Direct adversarial domain probes for invoices, payments, credit, statements, and aging.
4. Migration/repository and all-eight-operation integration suite.
5. Declared-versus-exercised protocol audit.
6. Full regression through the strict Makefile lane.
7. Independent CMake configure/build/test lane.
8. Archive integrity, extracted Git status, and plan-export verification.

## Results

- Invoice probe: 23 checks, 0 failed.
- Payment/allocation probe: 31 checks, 0 failed.
- Credit probe: 39 checks, 0 failed.
- Statement/aging probe: 37 checks, 0 failed.
- Permanent receivables suite: 40 checks, 0 failed.
- Strict full regression: 2,621 assertions, 0 failed.
- Integrity: 157 files, all clean.
- Header self-containment: 67 headers, 0 failures.
- Strict test programs: 15.
- CMake: 14/14 tests passed.
- Protocol: 12 modules, 44 rights, 67 operations.
- Every declared receivables operation is registered and exercised.
- Phase 5 workflow operations are not prematurely registered.

## Issues found and fixed

### QA-4.7-01 — Major — confirmed delivery evidence was mutable

**Root cause:** statement delivery used the repository's generic upsert path. Reusing a delivery id could replace recipient, content hash, and transport confirmation.

**Fix:** the send handler now refuses an existing delivery id. A permanent regression proves a rewrite is refused and the first evidence remains byte-for-byte exact.

### QA-4.7-02 — Major — named credit-hold override right missing

**Root cause:** the product specification required a separately named override right, but the protocol exposed only credit-account management.

**Fix:** added `right_credit_hold_override` to the receivables protocol rights surface. The right count is pinned at 44. Phase 5 will require it when an on-hold job is accepted.

### QA-4.7-03 — Minor — statement test requested an unfinished future period

**Root cause:** a fixed test timestamp was ahead of the injected clock.

**Fix:** the test now closes the period at the injected current time. The production refusal of future periods was correct.

### QA-4.7-04 — QA execution error — timing gate run beside another build

**Root cause:** strict writer-contention tests and the CMake compiler build were launched concurrently on a two-core sandbox, invalidating the timing assumptions.

**Fix:** reran the complete strict gate alone. All writer checks and the full suite passed. Parallel output is not used as approval evidence.

## Remaining risks and limits

- Production SQLite remains unapproved in this sandbox: SQLite headers and the required production version are unavailable. The CMake lane intentionally builds with SQLite off and labels itself non-shippable.
- Actual email transport belongs to Phase 8. Phase 4.7 records delivery only after a transport reference and exact-content hash are supplied; it never claims a prepared statement was sent.
- Printing returns deterministic document data. Native PDF/printing integration remains Phase 7.
- Credit-hold override evidence exists; the job-acceptance workflow that consumes the new right remains Phase 5.
- Jobs are not implemented until Phase 4.8, so job-target allocations can be represented but cross-module existence checks arrive with that module/workflow.

## Final verdict

**PASS.** No critical or major Phase 4.7 functional defects remain open. Required gates are green, evidence mutations are closed, authorization is named, and deferred work is outside the approved module boundary.
