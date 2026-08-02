# Phase 5.7 quality gate — agreement quantity caps

**Result: PASS**  
**Verified:** 2026-08-02  
**Compiler:** GCC 11.5.0, C++23, warnings as errors  
**Schema:** migration 24

## Feature decision

Agreement quantity is staged on an invoice draft without consumption. Quantity is consumed atomically only when that invoice is issued and released atomically when the issued invoice is cancelled. Jobs never consume agreement caps.

## Delivered behavior

- `apply_agreement` requires an explicit agreement and agreement-line identity.
- The draft line freezes agreement, line, rate, and quantity provenance.
- Applying a rate does not change the agreement counter or create usage evidence.
- Invoice issue validates the agreement is open, in force, for the same customer, and still matches product, rate, and quantity provenance.
- Issue refuses any quantity beyond the cap and rolls back the counter, usage evidence, invoice state, final number, audit, and outbox together.
- Successful issue increments `consumed_scaled` and creates deterministic active consumption evidence.
- Cancellation releases the exact active quantity even if the agreement later closes, expires, or is superseded, and permanently marks the evidence released.
- A replacement draft copies exact agreement provenance; issuing it consumes a new usage record.
- Agreement amendment cannot remove an actively consumed line, rewrite its product identity, or reduce a cap below consumption. Safe rate changes and cap increases retain the counter.
- Same-key replay is idempotent, and released evidence cannot release twice.

## Permanent tests

The Phase 5.7 workflow suite has **31 checks** covering:

- draft staging and exact structured provenance;
- no draft-time consumption;
- issue-time consumption and evidence;
- cancellation release and permanent release evidence;
- replacement provenance and re-consumption;
- over-cap refusal and final-number rollback;
- stale quantity refusal;
- rights, malformed payload, closed agreement, audit, and outbox failure boundaries.

The agreements suite now has **214 checks**, including active-line removal and consumed-product rewrite refusal.

## Regression result

```text
make -f tools/sandbox/Makefile check
```

- exit code: 0
- integrity: 252 files checked
- headers: 109 self-contained, 0 failures
- strict programs: 28
- assertions: 3,585
- failed assertions: 0

Independent CMake 4.4.2 verification:

```text
SQUIFLOW_WITH_SQLITE=OFF
SQUIFLOW_BUILD_TESTS=ON
ctest --output-on-failure
```

- configured and built successfully
- CTest: 28/28 passed
- failed tests: 0

## Defects found and fixed

1. **Strict compiler failure in the new workflow** — compact statements caused misleading-indentation diagnostics and an extra namespace close hid the exported workflow definition. The source was corrected and rebuilt under warnings-as-errors.
2. **Test shadowing failure** — a request row shadowed the store parameter. The identity was renamed and strict compilation rerun.
3. **Amendment-history gap** — a restatement could remove an actively consumed line or reuse its identity for another product. The agreement service now protects both invariants before replacing line rows; permanent regression checks prove rollback.

## Residual constraints

- SQLite remains disabled in this sandbox because the native SQLite development dependency is unavailable here; the in-memory transactional lane and independent CMake lane pass.
- Phase 5.8 document approval/email preparation remains unimplemented.

## Approval

No critical, major, or required-gate defect remains. Phase 5.7 is QA approved.
