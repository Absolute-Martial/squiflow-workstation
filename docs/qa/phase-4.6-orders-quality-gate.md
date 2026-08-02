# Phase 4.6 Orders - Production-Readiness Quality Gate

Status: **PASS - PHASE 4.6 APPROVED**

Date: 2026-08-02

Approval scope: the Phase 4.6 orders module and its integration boundaries,
not release approval for the unfinished application.

## Phase intent

Record what was agreed to be done. Orders may belong to a customer or be
walk-in orders. Every line snapshots its price, source and override reason in
the same transaction so a later pricing change cannot rewrite an earlier
agreement. Open orders may change; cancellation is one-way, explained and
separately authorised. Create/update/line-add work offline; cancellation is
online-only.

## Features covered

- Open/cancelled state and cancellation evidence.
- Customer and walk-in orders.
- Partial note and promised-date updates.
- Catalog and off-catalog lines.
- Integer minor-unit money and thousandth-scaled quantity.
- Default, party and override price resolution.
- Immutable line price/source/reason snapshots.
- Checked arithmetic and deterministic line order.
- Separate order read/write/cancel rights.
- Idempotency/outbox replay and rollback.
- Offline operation rules.
- Migration 14: `customer_order` and `order_line`.

Supplier credit is not part of orders. It remains approved for Phase 4.11
sourcing: purchases track paid/still owed and settlement date, and one screen
lists amounts still owed to suppliers.

## Files affected

The 12 files under `src/modules/orders/` plus:

- `src/modules/CMakeLists.txt`
- `tools/sandbox/Makefile`
- `tools/sandbox/cmake-verify/CMakeLists.txt`
- `docs/plan/status.md`
- `docs/plan/verification-output.txt`
- this report.

## Dependencies validated

Orders depends on protocol, money/quantity/identity, storage/transactions,
outbox, registry authorization, parties, catalog and pricing. The protocol
graph requires `orders -> catalog`, `orders -> pricing`, and
`orders -> parties`. The 1649-check protocol graph suite passed with no cycle,
missing operation, right mismatch or activation error.

## Test plan and execution order

The plan was recorded before execution and followed this order:

1. Static analysis and source hygiene.
2. Clean strict compilation, lint and type checks.
3. CMake and dependency validation.
4. Unit tests.
5. Repository/database-contract tests.
6. Registry/API integration tests.
7. State, authorization, offline and security tests.
8. Performance sanity.
9. Full regression.

The applicable API is the internal operation registry. This phase has no HTTP,
browser, UI, routing or rendering surface.

## Test results

| Area | Result |
| --- | --- |
| 12-file phase inventory | Pass |
| Integrity, whitespace, encoding and orphan-source rules | Pass - 140 files |
| Stubs/TODO/FIXME scan | Pass - none |
| Secret/private-key scan | Pass - none |
| Dangerous process/shell entry points in orders | Pass - none |
| Clean C++23 Makefile compile/link with full `-Werror` policy | Pass |
| Header self-containment | Pass - 59/59 |
| CMake 4.4.2 configure, real module linkage and orders build | Pass |
| Protocol/dependency graph | Pass - 1649 checks |
| State, validation, row mapping and checked arithmetic | Pass |
| Migration 14 and repository behavior | Pass |
| Default, party, override and no-price paths | Pass |
| Immutable price snapshot after later rate change | Pass |
| Null, missing and wrong-type fields | Pass |
| Special characters, UTF-8 bytes and 64 KiB note | Pass |
| Duplicate submission and idempotency replay | Pass |
| Same-position deterministic id tie-break | Pass |
| 16 concurrent line additions | Pass - exactly once, contiguous positions |
| Permission separation and unsigned session | Pass |
| Offline create and online-only cancel | Pass |
| 128 malformed payload shapes | Pass - generic errors, no partial writes |
| CMake-generated orders test | Pass - 181 checks |
| Clean complete regression | Pass - 2576 assertions, 0 failures |

A 5000-line performance probe completed correctly: insert 7 ms, query/sort
8 ms, checked total below 1 ms measured resolution, about 19 ms wall time and
15 MB peak child RSS in this sandbox. These are sanity measurements, not a
latency promise.

## Bugs found and fixes

### QA-4.6-01 - Major - CMake verification lane could not configure

**Reproduction**

Configure `tools/sandbox/cmake-verify` without a production SQLite toolchain.

**Observed**

Configuration stopped at `find_package(SQLite3 REQUIRED)`.

**Root cause**

The engine intentionally supports `SQUIFLOW_WITH_SQLITE=OFF` for the
non-shippable verification lane. The verifier documented that behavior but did
not default the option off.

**Fix**

The verifier now defaults `SQUIFLOW_WITH_SQLITE` to OFF before adding the
engine. It does not FORCE the setting, so an environment with SQLite >=3.51.3
can explicitly verify the persistent adapter. The production build still
requires SQLite >=3.51.3 and the verification lane emits a visible
non-shippable warning.

**Retest**

Exact configure, CMake build, CMake orders test and full regression all pass.

### QA-4.6-02 - Minor - missing permanent edge regressions

**Observed**

The 159-check suite did not explicitly lock down null optional fields,
long/UTF-8 text, same-position ties, unsigned sessions, concurrent additions or
a malformed-payload corpus.

**Fix**

Added permanent cases for all of them. The test clock and idempotency-key
counter are atomic so the concurrency test itself has no data race.

**Retest**

Orders increased to 181 checks and the complete project to 2576 assertions;
all pass through both build lanes.

## Earlier Phase 4.6 defects revalidated

The previous deep audit had already fixed four major data-integrity issues:
unknown states failing open, weak optional-field conversions, signed overflow
in next position, and contradictory cancellation/price provenance. Their
failure branches remain covered and pass.

## Regression

Previously completed protocol, engine, storage, migration, writer, outbox,
sync, payload, framework, administration, parties, catalog and pricing suites
all remain green. Final exact-tree results:

- Makefile gate exit 0;
- 140 integrity-clean files;
- 59 self-contained headers;
- 14 test programs;
- orders 181/181;
- total 2576/2576;
- CMake configure/build/test pass.

## Remaining risks

1. **Major release-environment risk, not an orders defect:** production requires
   SQLite >=3.51.3; this sandbox has only runtime 3.40 and no headers. Orders is
   verified against the shared Store contract. The persistent adapter must be
   built and tested with the required SQLite before release.
2. **Minor verification limitation:** ASan/UBSan runtimes are unavailable.
   Strict warnings, checked arithmetic, hostile payloads and concurrency tests
   passed; CI should still add sanitizer coverage.
3. **Minor:** text fields have no business-size cap. A 64 KiB value round-trips
   safely. The future external transport/UI must impose request/display limits.
4. **Optional optimization:** next-position derivation scans current lines.
   It avoids a drifting counter and is fast at unrealistic order sizes, but may
   be revisited if telemetry shows unusually large orders.

## Not applicable

UI layout, responsive behavior, accessibility, browser navigation/deep links,
HTTP timeout/partial-response handling and rendering performance do not exist
in Phase 4.6. They are neither passes nor failures. Session expiration also
does not yet exist; authentication and authorization that do exist were tested.

## Final approval

# PASS

All applicable compilation and automated gates pass. The major CMake defect is
fixed and retested. No critical or major Phase 4.6 functional defect remains,
architecture and conventions are intact, and full regression passes. The
project may proceed to Phase 4.7. This does not waive the SQLite >=3.51.3 and
sanitizer requirements before release.
