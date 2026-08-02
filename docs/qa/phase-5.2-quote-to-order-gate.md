# Phase 5.2 quotation-to-order quality gate

Verified 2026-08-02 with GCC 11.5.0 in C++23 mode and warnings as errors.

## Delivered contract

- An order may be direct, or identify one complete quotation source triple: quotation ID, exact revision ID, and positive revision number. Partial provenance is invalid.
- `quote_to_order` requires quotations, orders, and pricing and executes only through `Registry::run()`.
- The payload names the source quotation and exact accepted revision; `call.record_id` is the target order identity.
- Conversion accepts only the issued revision recorded as accepted on the quotation and revalidates ownership, each frozen line, line arithmetic, and the stored revision total.
- The order and lines are copied from the frozen revision. The workflow never calls current effective-price resolution, so later price-book changes cannot rewrite history.
- Catalog-default and party-specific origins map to normal order rate sources. Agreement, manual override, and off-catalog prices remain explicit override evidence; blank off-catalog evidence receives a deterministic explanation.
- The serialized transaction rejects an existing target order, a second order for the same quotation revision, and any copied-line identity collision.
- Business rows, one workflow audit row, and one outbox row commit together. Same-key replay enters no handler; any refusal rolls all new rows back.

## Permanent harsh coverage

`tests/workflows/quote_to_order_test.cpp` runs 21 checks with zero failures. It exercises successful snapshot conversion, exact source provenance, optional fields, frozen quantity/price/description and escaped UTF-8, total equality, audit/outbox persistence, same-key replay, different-key duplicate refusal, unaccepted and wrong-revision refusal, rollback after an order write followed by a line collision, and survival of pre-existing data.

The order module adds 8 provenance checks (189 total) for direct orders, complete/partial source validation, repository round-trip, and revision lookup.

## Final evidence

Command: `make -f tools/sandbox/Makefile check`

- Exit code: 0
- Assertions: 3,363
- Strict test programs: 23
- Failures: 0
- Integrity: 233 files, all pass
- Header self-containment: 102 headers, 0 failures
- Module graph: 12 modules, acyclic, core closed
- Schema: migration 22 (no new table required)
- Independent CMake configure/build: passed with warnings as errors

Phase 5.2A, 5.2B, 5.2C, and 5.2D are complete. Overall progress is 34/69 and Phase 5 progress is 2/8.
