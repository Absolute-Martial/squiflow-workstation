# Phase 5.6 Take-Payment Quality Gate

Status: **PASS — PHASE 5.6 APPROVED**

Verified: 2026-08-02 with GCC 11.5.0, C++23 warnings as errors, and CMake 4.4.2.

## Delivered contract

`take_payment` requires exactly `receivables` and `parties`, requires `right_payment_record`, is synchronizable, and retains the approved staff offline exception. The ordinary fast path records an active customer, positive amount, paid time, and simple method such as cash, bank, cheque, or wallet. No tracking number, bank reference, cheque number, or receipt number is required.

The database keeps its internal record identity for integrity, replay, audit, and sync without making the shopkeeper type or read it. `external_reference` is optional free text for any evidence the shop already has. A manually entered receipt is optional; series and number must either both be absent or both be coherent. The workflow never generates a payment tracking number and never automatically allocates money.

Every new payment begins completely unallocated. Existing `payment_allocate` remains a separate explicit action protected by `right_payment_allocate`, so recording valid incoming money cannot fail merely because the shopkeeper has not selected an invoice yet. Printed payment records and statements accept unnumbered payments and use the external reference or a friendly method description instead of displaying `-0`.

Supplier credit remains independent in sourcing: purchase records remain paid or still owed, settlement evidence/date is retained when cleared, and the outstanding-suppliers list remains the single owed view. No supplier ledger, aging engine, or automatic supplier payment was added.

## Test results

- Isolated take-payment suite: **42 checks, 0 failed**.
- Receivables regression: **40/40**.
- Workflow framework: **54/54**.
- Cancel-and-reissue regression: **44/44**.
- Full strict gate: **3,552 assertions across 27 programs, 0 failed**.
- Integrity: **247 files, all pass**.
- Header self-containment: **107 headers, 0 failures**.
- Independent CMake configure/build: passed.
- CTest: **27/27 passed**.

Coverage includes cash with no reference, bank/cheque/wallet references, Unicode reference text, optional manual receipt pairs, unallocated balances, separate manual allocation, over-allocation refusal, unnumbered printing, offline staff recording, rights, supplier-only and archived-party refusal, future/zero/negative values, malformed payloads, replay, identity collision, maximum signed amount, and failure audit/outbox absence.

## Defects resolved

1. Payment validation previously required a final receipt number. It now accepts the intended unnumbered payment while still rejecting half-formed receipt evidence.
2. Statement generation previously rendered an unnumbered payment as `-0`. It now prefers optional external evidence and otherwise uses a friendly method description.
3. Payment recording had no concrete workflow. The new workflow validates the customer and money atomically and leaves all money unallocated.

## Residual boundaries

- There is no automatic payment matching or allocation.
- There is no generated payment tracking number.
- Refunds, credit notes, payment deletion, and payment editing remain outside scope.
- Card processors and bank integrations are not included.
- The CMake lane uses `SQUIFLOW_WITH_SQLITE=OFF`; it is not the later native release build.

## Verdict

**PASS.** No critical or major Phase 5.6 defect remains open. The low-overhead unnumbered path, optional reference path, explicit allocation boundary, offline behavior, replay, integrity, strict compilation, CMake, and CTest gates are green.
