# Phase 6.8 -- Startup order, fixed and tested

Status: planned, not started as a gated sub-phase. The scaffolding this
sub-phase completes already exists -- read the reconciliation note below
before writing anything, so no file is recreated by accident.

## What already exists

`src/app/startup.hpp` already declares the fixed twelve-step order:

```text
Paths, Logging, CrashHandler, SingleInstance, DatabaseConnection,
Migrations, IntegrityCheck, IdentitySession, Activation,
ModuleRegistration, Shell, Window
```

`src/app/startup_sequence.hpp/.cpp` already implements a generic
`StartupSequence` runner against a `StartupRuntime` interface (start one
step, stop one step, rollback diagnostics). `src/app/composition_root.*`
already wires the twelve modules. `src/shell/completion_gate.hpp` already
exists as the generation-based staleness guard the QML bridge sub-phases
used for step 11/12.

**What is missing is the gate, not the mechanism:** a real
`StartupRuntime` implementation wired to the actual platform services from
6.1-6.7 (not a fake), an end-to-end test that runs all twelve steps in
order against that real wiring, reverse-order rollback tests for every
failure point, and the sign-off document. Do not re-derive the step list or
the rollback shape -- extend what is there.

## Goal

Prove, with a machine-checked test, that:

1. The twelve steps always run in the declared order, never reordered by
   what happens to compile first.
2. A failure at step *N* rolls back steps *N-1* down to *1* in exact
   reverse order, and a rollback failure at any of those steps is recorded,
   never swallowed.
3. Shutdown for each `ShutdownReason` (`NormalExit`, `WindowClosed`,
   `SessionEnding`, `SystemShutdown`, `StartupFailure`,
   `FatalApplicationError`) tears down in the same fixed reverse order.
4. The `Shell`/`Window` split (steps 11/12) never runs ahead of
   `Activation`/`ModuleRegistration` (steps 9/10), matching the QML bridge
   commitment already made in the presentation-bridge plan.

## Scope

- `RealStartupRuntime` (or equivalently named): a `StartupRuntime`
  implementation that calls into the real 6.1 paths, 6.2 logging, 6.3 crash
  handler, 6.4 single-instance lock, 3.4 database gate, 3.1/3.3 migrations,
  Phase 1.6-style integrity check, engine identity/session, the activation
  controller, `composition_root::register_all_modules`, and the shell/QML
  surface -- in that fixed order.
- A fake `StartupRuntime` (already implicitly assumed by
  `startup_sequence_test.cpp` if it exists, or written fresh here) that can
  be told to fail at any one of the twelve steps, for the rollback matrix.
- Wiring `main.cpp` to construct the real runtime and call
  `StartupSequence::start()` / `::shutdown()`, replacing whatever ad hoc
  ordering exists there today.

## Non-goals

- No new startup steps. Twelve is fixed by `kStartupStepCount`; if a future
  phase needs a thirteenth step, that is a protocol-level decision, not
  something this sub-phase should improvise.
- No behavior change to any individual step's own internals (paths,
  logging, etc.) -- those are already gated in their own sub-phases. This
  sub-phase only orders and rolls them back.
- No QML-specific behavior beyond what the presentation-bridge plan already
  committed to (step 11 creates the engine, step 12 publishes the window).
  Any further QML detail belongs to Phase 7, not here.

## Files

| File | Purpose |
| --- | --- |
| `src/app/real_startup_runtime.hpp/.cpp` | Real `StartupRuntime`, wiring all twelve steps |
| `src/app/main.cpp` | Updated to construct the real runtime and drive `StartupSequence` |
| `tests/app/startup_sequence_test.cpp` | Fake-runtime order and rollback matrix |
| `tests/app/real_startup_runtime_test.cpp` | Real wiring smoke test against in-memory/fake platform doors |
| `docs/qa/phase-6.8-startup-order-gate.md` | Evidence |

## Invariants

1. `startup_order()` is the single source of truth for ordering; no file
   duplicates the sequence as a separate literal list.
2. Every step's `start()` either fully succeeds or leaves nothing for that
   step to roll back -- partial step completion is a bug in the step, not
   something `StartupSequence` should have to guess about.
3. Rollback always proceeds in exact reverse order from the last
   successfully started step; a rollback failure at one step does not skip
   rolling back the steps below it.
4. `LifecycleState` transitions are exactly: `Idle -> Starting ->
   (Running | SecondaryInstance | Failed)`, and from `Running` only to
   `Stopping -> Stopped`.
5. `SecondaryInstance` short-circuits the sequence immediately after the
   `SingleInstance` step succeeds in "secondary" mode -- no later step runs,
   and no rollback is needed because nothing after it started.
6. `Shell` (step 11) never starts before `Activation` (step 9) and
   `ModuleRegistration` (step 10) have both completed. `Window` (step 12)
   never starts before `Shell` has completed.
7. Shutdown intent originating from the QML surface (a closed window) is
   translated to `ShutdownReason::WindowClosed` and drives the same
   `StartupSequence::shutdown()` path as every other reason -- never a
   direct process exit.

## Tests

Fake-runtime matrix (`startup_sequence_test.cpp`):

- Normal: all twelve steps succeed, state ends `Running`, `completed_steps()`
  lists exactly the twelve in order.
- Failure at each of the twelve steps individually (twelve cases): steps
  before it roll back in exact reverse order, state ends `Failed`, and the
  reported `StartupFailure.step` matches.
- `SecondaryInstance` at step 4: no step 5-12 starts; no rollback recorded.
- Rollback failure injected at a rolled-back step: recorded via
  `rollback_failures()`, and rollback still continues to the remaining
  earlier steps rather than stopping.
- Shutdown from `Running` for each of the six `ShutdownReason` values: exact
  reverse-order teardown of all twelve steps.
- Concurrency: `shutdown()` called from a second thread while `start()` is
  still running on the startup thread is either refused or safely
  sequenced -- never a data race, verified under the sanitizer build.
- Idempotency: calling `shutdown()` twice with the `Stopped` state already
  reached is a no-op, not a second teardown attempt.

Real-wiring smoke test (`real_startup_runtime_test.cpp`):

- All twelve steps succeed against fake/in-memory platform doors (fake
  filesystem, fake single-instance lock, in-memory store) standing in for
  the real Windows-only pieces, since those remain unverifiable on this
  toolchain.
- `ModuleRegistration` step actually calls
  `composition_root::register_all_modules` and the resulting registry
  reports all twelve modules present.
- `Activation` step actually resolves an activation snapshot and
  `ModuleRegistration` sees it before registering.

## Gates

- Full strict gate and independent CMake gate pass with this sub-phase
  included.
- Sanitizer build passes the concurrency test above.
- `docs/qa/phase-6.8-startup-order-gate.md` written with exact assertion
  counts.

## Sequence

| Sub-step | Content |
| --- | --- |
| 6.8.0 | Preflight: read every existing file in "What already exists" above; confirm no step needs a shape change. |
| 6.8.1 | Fake-runtime order/rollback/shutdown matrix tests, against the existing `StartupSequence`. |
| 6.8.2 | `RealStartupRuntime` wiring all twelve steps to real/fake platform doors. |
| 6.8.3 | `main.cpp` update to drive the real sequence. |
| 6.8.4 | Gates and sign-off document. |

## Acceptance criteria

- `docs/plan/todo.md` row for 6.8 checked only once the gate document
  exists and both the strict and CMake gates are green with this
  sub-phase's tests included.
- Phase 6 is then fully closed except for the two lines already recorded as
  Windows-only-unverifiable in `status.md` (6.3, 6.4 runtime execution;
  6.5/6.6 runtime gates) -- this sub-phase does not need to, and cannot,
  resolve those.
