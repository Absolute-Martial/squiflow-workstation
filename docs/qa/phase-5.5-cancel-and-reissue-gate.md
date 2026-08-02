# Phase 5.5 Cancel-and-Reissue Quality Gate

Status: **PASS — PHASE 5.5 APPROVED**

Verified: 2026-08-02 with GCC 11.5.0, C++23, warnings as errors, and CMake 4.4.2.

## Delivered contract

`cancel_and_reissue` requires exactly `receivables`, requires `right_invoice_cancel`, is synchronizable, and is online-only. The source invoice remains permanently numbered. The first execution records immutable cancellation evidence, releases active invoice allocations, and copies the exact commercial snapshot into an editable unnumbered replacement draft. No current pricing is queried, no refund or credit note is created, and released money is never automatically reallocated.

The draft names its source through `replaces_invoice_id`. The source remains `Cancelled` until the draft is issued. Phase 5.4 issuance now atomically moves the source to `Replaced`, writes `replacement_invoice_id`, gives the replacement a new reserved number, and preserves the reverse link. A discarded draft remains historical evidence and permits a later replacement attempt without rewriting the original reason or time.

## Test results

- Isolated workflow suite: **44 checks, 0 failed**.
- Issue-invoice regression: **41/41**.
- Workflow framework: **54/54**.
- Receivables: **40/40**.
- Full strict gate: **3,510 assertions across 26 programs, 0 failed**.
- Integrity: **244 files, all pass**.
- Header self-containment: **106 headers, 0 failures**.
- Protocol: 12 modules, 44 rights, 67 operations.
- Independent CMake configure/build: passed.
- CTest: **26/26 passed**.

Coverage includes copied Unicode and large text, frozen manual rates, deterministic line identities, allocation release and unallocated balances, replay, active-replacement uniqueness, discard-and-retry, two-way final links, separate numbers, rights, online-only enforcement, stale confirmation, malformed payloads, state boundaries, empty evidence, late target collision rollback, duplicate-number rollback, audit/outbox counts, and number-block restoration.

## Defects and disruptions resolved

1. The initial full strict invocation outlived its tool wrapper. The process state was inspected rather than blindly restarted.
2. The sandbox reset removed the uncommitted test file. The verified Phase 5.4 checkpoint was restored, implementation state was audited, and the permanent suite was regenerated and rerun.
3. A late derived-line collision is intentionally checked during insertion so the permanent suite proves rollback after source cancellation, allocation release, replacement creation, and an earlier copied line.

## Residual limits

- Replacement issuance still requires a valid reserved number block from Phase 5.4.
- The CMake verification lane uses `SQUIFLOW_WITH_SQLITE=OFF`; it is not a shippable native database build.
- Payment recording and new manual allocation remain Phase 5.6.
- Agreement-cap release remains Phase 5.7.
- No refunds, credit notes, automatic payment matching, or automatic reallocation exist.

## Verdict

**PASS.** No critical or major Phase 5.5 defect remains open. Authorization, connectivity, immutable evidence, allocation release, replay, rollback, strict, integrity, CMake, and CTest gates are green.
