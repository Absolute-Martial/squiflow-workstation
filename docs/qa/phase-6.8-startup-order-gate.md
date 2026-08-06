# Phase 6.8 -- Startup order, fixed and tested

Date: 2026-08-06

## Scope and decision rule

This gate extends the existing `startup.hpp` / `startup_sequence.*` /
`composition_root.*` scaffolding rather than re-deriving it. No step list,
rollback shape, or shutdown-reason set was changed. Work was limited to: (1)
closing the production-completeness gap that blocked a real registry from
composing, and (2) adding a concrete `StartupRuntime` implementation and its
lifecycle/rollback proof. `main.cpp` was not rewired to the concrete runtime;
the reason is recorded below as a discovered blocker rather than worked
around.

## Already implemented at the start

- `startup_order()` is the single fixed twelve-step sequence and the only
  place that sequence is written down.
- `StartupSequence` already runs a `StartupRuntime` through that order,
  rolls back completed steps in exact reverse order on failure, records
  rollback failures without stopping earlier rollback, short-circuits after
  `SingleInstance` for a secondary instance, and drives every
  `ShutdownReason` through the same reverse-order path.
- `composition_root::register_all_modules` already registered all twelve
  modules.

## Discovered blocker: production registry was not actually completable

**Concern.** The protocol declares ten workflow operations. Before this
gate, factories existed for only eight; `counter_sale` and `record_purchase`
had no workflow, and no function anywhere installed the workflows that did
exist into a production registry. `composition_root::register_all_modules`
called `Registry::require_complete()`, which fails for any unhandled
operation. So the composition root that Phase 6.8 was asked to wire a real
runtime to could not, in fact, complete -- the gap was in application
completeness, not in startup ordering.

**Root cause.** `counter_sale` and `record_purchase` were added to the
protocol's operation table without matching workflow implementations, and
workflow installation into the registry had never been assembled into one
production entry point -- every workflow test built its own private
registry.

**Change.**
- Added `src/workflows/record_purchase.*`: parses and validates the request,
  calls the existing `SourcingService::record_purchase()` seam inside the
  registry's transaction, and preserves actor/time evidence. Tests cover
  owed and paid purchases, new/existing material, unknown supplier, unknown
  fields, replay, and rights enforcement (18 checks).
- Added `src/workflows/counter_sale.*`: atomically creates an order, one
  priced line (remembered rate or an explicitly justified override), and a
  payment/receipt in one transaction; opens no invoice or credit cycle;
  accepts an empty walk-in party. Tests cover the remembered-price path, an
  explicit off-catalog price with its required reason, replay, and refusals
  leaving no partial order/payment (17 checks).
- Added `src/workflows/registration.cpp` (`register_all_workflows`): the one
  production function that installs all ten protocol workflows. It is
  invoked by `composition_root::register_all_modules` before
  `require_complete()`.
- Added `tests/app/composition_root_test.cpp`: builds the real production
  registry through `register_all_modules` and asserts all twelve modules are
  present, `unhandled()` is empty, and every one of the ten protocol
  workflow operations is available (13 checks).

**Why this solution.** Each missing workflow is implemented once, against
its already-documented module seam and canonical module requirements
(`orders`/`pricing`/`receivables` for `counter_sale`, `sourcing` for
`record_purchase`), matching the shape of the eight workflows already in the
tree. Registration was centralized into one function instead of letting
`composition_root` keep enumerating workflow factories inline, which is the
same reasoning already applied to module registration.

**Trade-off.** None accepted beyond the two new small workflow files; no
existing workflow, module, or registry API changed shape.

**Evidence.**
```text
record_purchase_test:    18 checks, 0 failed
counter_sale_test:       17 checks, 0 failed
app_composition_root_test: 13 checks, 0 failed
```

## Concrete `StartupRuntime`

**Change.** Added `src/app/real_startup_runtime.hpp/.cpp`:
`RealStartupRuntime` implements `StartupRuntime` and owns exactly the
sequencing, resource lifetime, and rollback logic that is this sub-phase's
job -- it does not reimplement any platform mechanism. Host-specific work is
delegated through a small `StartupServices` seam (paths, logging, crash
handler, single instance, store connection, integrity check, session
loading, shell/window) so the runtime's own logic is testable without a real
platform.

`RealStartupRuntime` resolves the migration/module-registration ordering
tension named in the plan explicitly: at the `Migrations` step it builds a
short-lived `Registry` purely to collect migration definitions from the
module factories, opens the database against that migration set, and
discards the short-lived registry immediately; the live registry that
`Shell`/`Window` and every later operation use is built fresh at the fixed
`ModuleRegistration` step. Migration metadata and live handler registration
are therefore explicitly two different objects with two different
lifetimes, not one registry reordered.

`Activation` is resolved from the database's disabled-module list and
checked before `ModuleRegistration` builds the live registry, so a shell
cannot see a module the activation check rejected. `Shell` receives the live
registry's resolved activation, full rights, and registered-module list only
after both `Activation` and `ModuleRegistration` have completed, matching
invariant 6 in the plan.

**Tests (`tests/app/real_startup_runtime_test.cpp`, 16 checks):**
- All twelve steps run through `StartupSequence` against a fake
  `StartupServices` and reach `Running`; `completed_steps()` matches
  `startup_order()` exactly.
- The opened database carries every module's migrations (version comparison
  against the known highest migration number) and the live registry has all
  twelve modules with `unhandled()` empty -- proving the production
  registration path, not a test-only one, ran.
- The identity session survives into `Shell`, and `Shell` receives the
  resolved activation, full rights, and the registered-module count from the
  *live* registry.
- `ShutdownReason::WindowClosed` tears down window, shell, registry, and
  session, then releases the single-instance lock, logging, and paths, in
  reverse order.
- A secondary-instance acquisition starts no database, registry, or later
  step, and still releases logging/paths.
- An integrity-check failure at step 7 leaves no live database or registry
  and still unwinds every platform resource acquired before it.

**Evidence.**
```text
app_real_startup_runtime_test: 16 checks, 0 failed
```

## Discovered blocker: `main.cpp` cannot be wired to a real `IdentitySession` yet

**Concern.** `StartupServices::load_session` is the one remaining seam a
concrete, shipped `main.cpp` would need to fill honestly. Searching the tree
for any existing credential verification, session persistence, or sign-in
mechanism found none: `engine::Session` is a plain data holder, and no
module, service, or platform file authenticates a person or restores a
previous session. The feature plan (`04-feature-set-and-usage-specification.md`,
`03-runtime-architecture-sync-and-delivery.md`) describes sign-in,
per-person rights, and "nothing touching the network starts before sign-in"
as a real, still-unbuilt feature, not something Phase 6.8's startup-ordering
scope defined.

**Decision.** `main.cpp` was left as the existing placeholder rather than
wired to `RealStartupRuntime` with an invented `load_session`. Inventing
credential handling or a "restore the sole owner account, no password"
shortcut under an ordering sub-phase would be a genuine, unreviewed security
decision smuggled into unrelated work, and the plan's own non-goals rule out
behavior changes to individual steps' internals here. `IdentitySession` in
`RealStartupRuntime` is proven correct against a realistic fake in the test
above; wiring it to a real login/session mechanism is now a precondition of
the sign-in feature phase, not of Phase 6.8, and `main.cpp` wiring should
follow immediately after that feature lands rather than before it exists.

**What remains for that later wiring, so it is not rediscovered:** a
concrete `StartupServices` needs an implementation for every method other
than `load_session` (paths via `PathResolver`/`LocalDirectoryProbe`, logging
via `RotatingLogFile`/`LocalLogStorage`, `make_crash_handler()`,
`make_single_instance_lock()`, a `SqliteStore`-backed `connect_store()`, and
an integrity check reusing the existing Phase-1.6-style pass); those are all
straightforward compositions of already-gated Phase 6.1-6.7 pieces and carry
no open design question.

## Verification

```text
make -f tools/sandbox/Makefile check: exit 0
51 test programs, 5,632 assertions, 0 failures
400 integrity files, all pass
178 self-contained headers, 0 not self-contained
application architecture policy passed
composition root policy: 12 modules passed
module boundary policy: 12 modules, 13 edges passed
QML/UI/Qt-bridge/navigation policies passed
git diff --check passed
git fsck --full passed, with unreachable recovery objects only
```

CMake/CTest and a live Qt runtime remain unavailable in this sandbox
(`cmake: command not found`), matching every prior phase's gate in this
repository.

## Remaining Phase 6.8 work

1. Implement the sign-in feature (credential verification and session
   persistence) as its own reviewed unit of work.
2. Implement a concrete `StartupServices` using the composition sketched
   above and wire `main.cpp` to `RealStartupRuntime` and the Qt event loop.
3. Re-run this gate's evidence against that concrete wiring once both exist.
