# Phase 5.1 workflow framework quality gate

Verified 2026-08-02 with GCC 11.5.0 in C++23 mode and warnings as errors.

## Delivered contract

- Exactly eight protocol operations are classified as workflows: `quote_to_order`, `order_to_jobs`, `issue_invoice`, `cancel_and_reissue`, `apply_agreement`, `take_payment`, `counter_sale`, and `record_purchase`.
- Every workflow has one definition, one owning operation, canonical module requirements, and one transaction-bound handler.
- `Registry::run()` is the only execution door. No module handler can bypass its capability, ownership, requirement, replay, transaction, audit, or outbox gates.
- A successful first execution commits business rows, exactly one `workflow_audit_entry` row, and exactly one outbox row in one transaction.
- Replay is detected before handler entry and cannot duplicate business, audit, or outbox state.
- Any refusal or exception rolls back business, audit, and outbox writes together; the serialized writer remains usable afterwards.
- Engine migration 22 upgrades a migration-21 database without rewriting historical module migrations.

## Canonical requirements

| Workflow | Required modules |
| --- | --- |
| `quote_to_order` | quotations, orders, pricing |
| `order_to_jobs` | orders, jobs |
| `issue_invoice` | receivables, orders, pricing |
| `cancel_and_reissue` | receivables |
| `apply_agreement` | agreements, pricing, receivables |
| `take_payment` | receivables, parties |
| `counter_sale` | orders, pricing, receivables |
| `record_purchase` | sourcing |

## Permanent harsh coverage

The workflow program runs 54 checks with zero failures. It covers exact protocol classification, near-miss refusal, ownership and duplicate registration, canonical requirements, absent/inactive/online-only capability gates, malformed audit subjects/details, replay before side effects, one-transaction persistence, rollback after an early write, writer recovery, concurrent duplicate attempts, and migration 21-to-22 upgrade.

## Whole-project regression and correction

The first whole-project gate exposed a real collision: administration migration 10 already defines `audit_entry` with a different key. Workflow audit persistence was corrected to use the engine-owned `workflow_audit_entry` table. The administration suite then passed 57 checks and the workflow suite passed 54 checks. A forced clean rebuild proved the correction across the entire project.

## Final evidence

Command: `make -f tools/sandbox/Makefile check`

- Exit code: 0
- Assertions: 3,342
- Strict test programs: 22
- Failures: 0
- Integrity: 230 files, all pass
- Header self-containment: 101 headers, 0 failures
- Module graph: 12 modules, acyclic, core closed
- Schema: migration 22
- Independent CMake configure/build: passed with warnings as errors

Phase 5.1A, 5.1B, and 5.1C are complete. Overall plan progress is 33/69 and Phase 5 progress is 1/8.
