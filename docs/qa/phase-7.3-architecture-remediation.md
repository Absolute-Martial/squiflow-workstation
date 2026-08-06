# Phase 7.3 architecture remediation

Date: 2026-08-06

## Scope and decision rule

This pass reviewed the architecture implemented through Phase 7.3 before
changing it. It retained the established protocol, module, storage,
workflow, platform, shell, and Qt seams and applied only changes that fix a
demonstrated compatibility, integrity, dependency, build-governance, or
lifecycle problem. Broad redesigns were deferred when their owning phase is
not yet implemented or when the existing design has no measured failure.

## Already implemented at the start

- Protocol-owned module, right, operation, capability, and offline-rule
  enumerations already prevent stringly typed runtime registration.
- Module and workflow writes already use capability enforcement, one
  transaction, audit/outbox persistence, rollback, and idempotency tests.
- Platform resources use RAII and test seams; time-sensitive logic accepts
  injected clocks or explicit calendar input.
- Background work already uses two bounded `std::jthread` lanes, cooperative
  cancellation, coalescing, failure budgets, and bounded diagnostics.
- Phase 7.2 navigation and Phase 7.3 lists already separate portable state
  from Qt adapters, validate requests, reject stale generations, and bound
  retained pages and row sizes.

## Completed improvements

### GCC 11 background snapshot compatibility

**Concern and risk.** `std::atomic<std::shared_ptr<T>>` is not supported by
the reference GCC 11 standard library and stopped the strict build at header
self-containment.

**Change.** The supervisor now stores a plain shared pointer and publishes or
loads it with the C++ atomic shared-pointer free functions.

**Why this solution.** It preserves immutable lock-free-facing snapshots and
the existing memory ordering without changing the public API.

**Trade-off.** The syntax is less compact than the C++20 specialization, but
it is portable to the declared compiler floor.

**Evidence.** 174 headers are self-contained; the background-service suite
passes with 33 checks.

### Source archive truncation protection

**Concern and risk.** An extracted Windows DPAPI source file contained
literal `[...]` archive/transcript elision markers. Such a file can evade
review on a non-Windows host and fail or weaken a later platform build.

**Change.** The complete committed DPAPI implementation was restored and the
integrity gate now rejects literal `[...]` markers in source inputs.

**Why this solution.** A small source-integrity rule prevents recurrence on
every platform without attempting to infer missing code.

**Trade-off.** A deliberate literal marker in source would need different
wording; no production use exists.

**Evidence.** The gate passed all 388 files and a temporary injected marker
was rejected before the source was restored.

### Declared module dependency governance

**Concern and risk.** The protocol graph allowed 13 dependency edges, CMake
did not declare all of them, and architecture prose claimed that modules
could never include another module. Direct target links partly hid the
disagreement. This allowed source, protocol, and build graphs to drift.

**Change.** All protocol edges are represented by module `REQUIRES`
declarations. Redundant direct links were removed. A strict checker compares
the protocol and CMake graphs and rejects production cross-module includes
without a protocol edge. The architecture rule now permits declared reads
while reserving cross-module mutations for workflows.

**Why this solution.** It enforces the architecture already encoded by the
protocol instead of introducing a new dependency model.

**Trade-off.** The checker intentionally parses the project's constrained
CMake declaration form rather than implementing a general CMake parser.

**Evidence.** All 12 modules and 13 edges pass; orders (189 checks) and
receivables (40 checks) pass after direct links were removed.

### Active application architecture gates

**Concern and risk.** Application and composition-root policy scripts
existed but were not part of the primary strict gate, so architectural drift
could pass CI.

**Change.** The architecture target now runs application shape, composition
completeness, and module boundary checks before the Qt/QML policies.

**Why this solution.** Reusing focused existing checks adds enforcement with
no runtime abstraction or production-code churn.

**Trade-off.** These are structural checks, not a substitute for the Phase
6.8 startup smoke test or the pending Qt runtime lane.

**Evidence.** The final strict gate reports the application policy passed,
all 12 composition factories present, and all 13 module edges aligned.

### Canonical CMake module helper

**Concern and risk.** An obsolete `cmake/modules.cmake` helper remained
included beside the active `SquiflowModule.cmake` and `ModuleGraph.cmake`
implementation. Two build-graph implementations create ambiguous ownership
and future maintenance drift.

**Change.** The obsolete include and helper were removed and the phase plan
now names the canonical pair.

**Why this solution.** Deletion removes ambiguity without changing targets
or adding another compatibility layer.

**Trade-off.** Local CMake configure/build execution is still unavailable in
this image (`cmake: command not found`); workflow and preset policy is
statically verified and CMake remains a required hosted gate.

### Background supervisor lifecycle and duplicate admission

**Concern and risk.** Work could be triggered before registration was
sealed, allowing an `unordered_map` mutation while submission temporarily
released the registry mutex. Concurrent gate updates could also submit one
gated service more than once, and a shutdown/submission race could restore an
idle status after stopping began.

**Change.** Trigger and timer execution now require a sealed registry;
network/idle state may be staged but cannot dispatch before sealing;
registration remains closed after shutdown. An internal submission marker
prevents duplicate admission, and failed submission re-resolves the entry
and preserves the stopping state.

**Why this solution.** It makes the existing two-phase lifecycle explicit
and fixes the races locally without changing the executor, service API, or
thread count.

**Trade-off.** Calling execution methods before `seal()` is now a loud
configuration error. Startup composition must follow the documented order.

**Evidence.** The focused suite passes 33 checks, including pre-seal
refusal, staged network state, shutdown closure, coalescing, gating, failure
budgets, timers, and concurrent snapshots.

## Deferred recommendations

- **Phase 6.8:** concrete startup composition, production workflow
  registration, database/platform/shell lifecycle wiring, and smoke tests.
  This is the next planned work item and must be implemented as a coherent
  startup slice rather than an ad-hoc `main.cpp` edit.
- **Phase 3.7 / hosted build:** real SQLite parity, migration behavior, WAL,
  and busy-timeout evidence. Replacing the storage seam or introducing an ORM
  is not justified by the current tests.
- **Before durable interchange:** explicit payload/schema versioning and
  compatibility tests. There is no implemented server transport to evolve in
  this phase.
- **Phase 8:** operational synchronization transport, server sequencing,
  identity, and endpoint behavior. The local outbox/cursor/conflict contracts
  are retained rather than pre-implementing the server.
- **Phase 9:** backup/restore, update rollback, packaging, signing, and
  machine-wide permission hardening belong to their delivery phases.
- **Growth-triggered refactoring:** split `Registry`, generic query rows, or
  repeated request helpers only when ownership, compile-time, or defect data
  demonstrates a concrete boundary. No plugin system, service locator, ORM,
  or event-sourcing layer was introduced.
- **Hosted runtime evidence:** Windows DPAPI, lock/crash behavior, Qt 6.11.1
  adapter/model signals, QML loading and focus behavior, and CMake/CTest still
  require their declared Linux/MSVC lanes.

## Final verification

```text
make -f tools/sandbox/Makefile check: exit 0
47 test programs, 5,489 assertions, 0 failed
174 self-contained headers, 0 failed
integrity: 388 files checked, all pass
application architecture policy passed
composition root policy: 12 modules passed
module boundary policy: 12 modules, 13 edges passed
pipeline policy: presets, workflows, packaging and manifest passed
git diff --check: passed
git fsck --full: passed (unreachable recovery objects reported only)
```

No CMake/CTest or Qt runtime result is claimed from this image because those
tools are not installed. The portable strict gate and affected focused suites
are green; remaining hosted evidence stays explicitly pending.

## Commits

```text
3bd4dc5 fix(platform): support shared snapshots on GCC 11
78046c3 test(sandbox): reject truncated source placeholders
a380c3d build: enforce declared module dependencies
20dd1d2 build: remove obsolete module helper
b97a813 fix(platform): enforce supervisor lifecycle
```
