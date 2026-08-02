# Phase 5.4 Issue-Invoice Quality Gate

Status: **PASS — PHASE 5.4 APPROVED**

Verified: 2026-08-02 with GCC 11.5.0, C++23, warnings as errors, and CMake 4.4.2.

## Scope and contract

`issue_invoice` requires exactly `receivables`, `orders`, and `pricing`; is synchronizable and offline-allowed; and executes through the shared workflow registry transaction. `Call::record_id` identifies one existing invoice draft. The payload contains only the selected series and the human-confirmed expected line count and total. The caller cannot provide the final number.

Issuance loads and revalidates the complete draft and each frozen line. It refuses empty, stale, malformed, discarded, cancelled, replaced, or already-issued evidence. It does not query current pricing, change the source order, rewrite a line, create a payment, send a message, or print automatically. Customer credit terms determine the due date when an account exists. Credit hold does not block billing already-completed work and no credit-hold override is silently consumed.

Migration 23 adds `receivable_number_block`. Each retained block names its document kind, visible series, device, inclusive range, next number, exhaustion state, assignment time, and server assignment reference. Blocks never overlap within one series, exhausted ranges remain evidence, and only the signed-in device's lowest available block is consumed. Final number consumption, invoice transition, workflow audit, and outbox insertion commit or roll back together. Gaps are valid; cancelled numbers remain on their invoices and are never returned.

## Test plan and results

- Strict production/header compile: passed.
- Isolated `issue_invoice` suite: **41 checks, 0 failed**.
- Workflow framework regression: **54/54**.
- Quote-to-order regression: **21/21**.
- Order-to-jobs regression: **54/54**.
- Receivables regression: **40/40**.
- Full strict gate: **3,466 assertions across 25 programs, 0 failed**.
- Integrity: **241 files, all pass**.
- Header self-containment: **105 headers, 0 failures**.
- Protocol: 12 modules, 44 rights, 67 operations.
- Independent CMake configure/build: passed.
- CMake module graph: **12 modules, acyclic, core closed**.
- CTest: **25/25 passed**.

The permanent suite covers migration and table presence, reversed and overlapping ranges, one-number and `INT64_MAX` boundaries, offline issuance, due-date calculation, immutable line evidence, same-key replay, different-key duplicate refusal, audit/outbox counts, multi-block rollover, wrong-device isolation, stale count and total, caller-supplied-number rejection, malformed payloads, missing rights, empty drafts, past due dates, missing series blocks, duplicate-number corruption, and rollback after block consumption.

## Defects found and fixed

### QA-5.4-01 — Build integration — missing parties dependency

The first strict workflow target linked the complete receivables service but omitted its existing parties-domain dependency, producing an undefined reference to `party_from_row`.

**Fix:** added the parties source set to the isolated `issue_invoice_test` target. The strict target then linked and ran.

### QA-5.4-02 — Test-fixture identity collision

The first multi-invoice fixture derived line IDs by replacing the final invoice-ID character. All invoices shared the same 31-character prefix, so later drafts overwrote earlier line rows and appeared empty.

**Fix:** retained the invoice discriminator in each derived test line ID. The rollover tests then passed and now prove three distinct drafts consume 10, 11, and 12 across two blocks.

### QA-5.4-03 — Compile failure — repository exception visibility

The new overlap guard threw `RuleViolation` without including the module context declaration in `repository.cpp`.

**Fix:** added the exact context include; warnings-as-errors compilation passed.

### QA-5.4-04 — Tool execution wrapper failure

The first targeted regression command failed before execution because the tool wrapper rejected its request shape. No build or test had run.

**Fix:** reran the command directly and recorded only the real successful execution as evidence.

## Residual limits

- Phase 8 server work must allocate and synchronize disjoint number blocks. This phase safely consumes blocks but does not invent a local server allocator.
- CMake verification uses `SQUIFLOW_WITH_SQLITE=OFF`; the persisted design is verified through the store seam but this is not a shippable SQLite build.
- Invoice drafting remains the existing receivables responsibility. Phase 5.4 intentionally issues an existing snapshot rather than auto-generating lines from jobs or orders.
- Cancellation and replacement remain Phase 5.5; payment recording remains Phase 5.6.
- Credit-hold override remains a work-acceptance decision, not a barrier to billing completed work.

## Final verdict

**PASS.** No critical or major Phase 5.4 defect remains open. Required strict, authorization, stale-confirmation, numbering-boundary, rollback, replay, integrity, header, CMake, and CTest gates are green.
