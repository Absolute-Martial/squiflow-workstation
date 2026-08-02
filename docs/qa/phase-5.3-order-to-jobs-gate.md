# Phase 5.3 Order-to-Jobs Quality Gate

Status: **PASS — PHASE 5.3 APPROVED**

Verified: 2026-08-02 with GCC 11.5.0, C++23, warnings as errors, and CMake 4.4.2.

## Scope and contract

`order_to_jobs` requires exactly `orders` and `jobs`, is synchronizable and offline-allowed, and executes through the shared workflow registry transaction. The payload names one open source order and either all lines or an explicit non-empty unique subset. By default, one order line becomes one draft job.

Each job preserves `source_order_id`, exact `source_order_line_id`, and the order's `source_quotation_id` when present. Quantity, unit price, total, and price-origin evidence are copied from the frozen order line. No pricing lookup or repricing occurs. Direct jobs remain valid with no order or order-line provenance. No table and no jobs-to-orders module dependency were added.

`Call::record_id` is the conversion identity. A deterministic 128-bit derived job id is produced from it and the exact source-line id. Same-key replay is handled before the workflow. Different-key attempts are refused by `(source_order_id, source_order_line_id)`. Explicit later subsets may convert only lines that have not already produced jobs.

## Test plan and results

- Strict production/header compile: passed.
- Isolated workflow suite: **54 checks, 0 failed**.
- Workflow framework regression: **54/54**.
- Quote-to-order regression: **21/21**.
- Orders regression: **189/189**.
- Jobs regression: **32/32**.
- Full clean strict gate: **3,425 assertions across 24 programs, 0 failed**.
- Integrity: **236 files, all pass**.
- Header self-containment: **103 headers, 0 failures**.
- Protocol: 12 modules, 44 rights, 67 operations.
- Independent CMake configure/build: passed.
- CMake module graph: **12 modules, acyclic, core closed**.
- CTest: **24/24 passed**.

The permanent suite covers one-line and multi-line conversions, direct and quotation-derived orders, selected subsets, empty/cancelled/missing orders, malformed and unknown payload fields, same-key replay, different-key duplicates, later target collision rollback, large Unicode text, offline use, audit/outbox counts, unchanged source orders, workflow-created job invariants, inactive required modules, and direct jobs without orders.

## Defects found and fixed

### QA-5.3-01 — Major — derived identity collision

The first deterministic-id implementation used nibble-wise XOR. Distinct pairs such as `(root 1, line 2)` and `(root 2, line 1)` could collide, incorrectly blocking valid later subsets.

**Fix:** replaced XOR with two independently seeded, delimited FNV-1a passes forming a deterministic 128-bit record id. The subset regression then passed.

### QA-5.3-02 — Build integration — missing pricing link in strict target

The first strict `order_to_jobs_test` target linked the full orders module without its existing pricing dependency.

**Fix:** added `PRICING_SRC` to the target. The isolated binary then linked and ran.

### QA-5.3-03 — Test assumption — attempted to disable a core module

The activation test attempted to disable core `orders`, which the protocol correctly refuses.

**Fix:** the test disables optional `jobs`, proving workflow unavailability through a valid activation state.

### QA-5.3-04 — Major inherited checkpoint defect — missing CMake aggregators

The supplied Phase 5.2 checkpoint's root build required `src/CMakeLists.txt` and `tests/CMakeLists.txt`, but neither existed in the current tree or Git history. Independent CMake could not configure.

**Fix:** reconstructed complete source and test aggregators from the existing subproject contracts. CMake now configures the 12-module graph, builds every target, and passes 24/24 tests.

## Residual limits

- Jobs are created as unnumbered drafts; ticket assignment remains the existing job state-change responsibility.
- Explicit subsets use a comma-separated `line_ids` text field because the current payload codec supports scalar row values, not arrays.
- The CMake verification lane uses `SQUIFLOW_WITH_SQLITE=OFF`; it is not a shippable SQLite build.
- Hash collisions are no longer structurally induced, but—as with any finite 128-bit derived identifier—are theoretically possible and are still safely refused by the target-collision check.

## Final verdict

**PASS.** No critical or major Phase 5.3 defect remains open. Required strict, regression, rollback, integrity, header, CMake, and CTest gates are green.
