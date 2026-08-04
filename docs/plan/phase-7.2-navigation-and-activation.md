# Phase 7.2 — Navigation and module visibility from activation

## Status

**Ready to implement.** This plan starts from commit `94e76dd`, after the QML presentation bridge gate. Phase 7.2 will refine the existing `ScreenRegistry` prototype into the production navigation system; it will not create a second registry.

## Goal

Build a deterministic workstation navigation shell whose visible destinations are the intersection of:

1. modules registered in this executable;
2. modules active in the resolved `protocol::Activation` graph;
3. rights present in the signed-in session snapshot; and
4. valid shell-owned screen contributions.

A hidden or unauthorized destination must not be selectable, instantiated, restored from history, or reachable through a shortcut. Activation and permission changes must reconcile the current route immediately on the GUI thread.

## Existing foundations

- `modules::Registry` owns the authoritative resolved activation and exposes `activation()` and `active()`.
- `protocol::resolve_activation()` already enforces core-module permanence and transitive deactivation of dependent modules.
- `engine::RightsSet` is the authoritative per-person permission set. Grants for inactive modules are retained and filtered at use time.
- `shell::ScreenRegistry` already validates stable IDs, filters contributions, and constructs bridges lazily.
- The QML bridge policy confines Qt to `src/shell/` and presentation-safe data to QML.
- `GuiDispatcher`, completion generations, immutable `RequestContext`, and Phase 6 Step 11/12 surface lifecycle are available.

## Required corrections before feature work

The current prototype is deliberately small and needs these corrections rather than parallel abstractions:

- Replace `ScreenContribution::required_right` from untyped `std::uint32_t` with `std::optional<protocol::RightId>`.
- Replace callback-based `ScreenAccess` with an immutable `NavigationAccess` snapshot containing `protocol::Activation`, `engine::RightsSet`, and `session_generation`.
- Validate that a required right is valid and belongs to the contribution owner module.
- Add explicit group and rank fields; navigation order must not depend on localized titles or registration order.
- Keep `modules::Registry` out of QML and out of long-lived view models. The application composition root copies its activation into the shell snapshot.
- Nest the Qt-only test target under `SQUIFLOW_WITH_QT`; a Qt-off configure must never reference Qt targets.

## Architectural decisions

### One-way dependency

Domain modules do not include shell or Qt headers. Presentation contributions live under `src/shell/modules/<module>/` and are assembled by a shell composition function. Each contribution is keyed by the same `protocol::ModuleId` used by the domain registry, but the domain module does not know that a screen exists.

```text
protocol/module graph ─┐
modules::Registry ─────┼─> application snapshot ─> shell navigation ─> QML
engine::Session ───────┘
```

QML receives strings, stable IDs, booleans, integer ranks, and URLs. It never receives `Session`, `RightsSet`, `Activation`, `Module`, or domain entities.

### Authoritative access rule

A screen is visible only when all of these are true:

```text
registered(owner)
&& activation.is_active(owner)
&& (!required_right || rights.has(*required_right))
```

Visibility is not authorization. Every command still enters `modules::Registry::run()`, which repeats module, right, connection, and offline-rule checks. Navigation filtering prevents misleading UI; it does not replace application security.

### Immutable snapshots

`NavigationAccess` owns copies of activation and rights and carries `session_generation` plus a monotonically increasing `navigation_revision`. Asynchronous work captures the complete value. A stale revision cannot overwrite newer visibility or selection.

### Stable route identity

Route identity is the non-localized screen ID, for example `orders.list`. Titles are translation keys and may change without changing history, selection, automation, or persisted window state.

### Selection reconciliation

After every access or contribution change:

1. retain the current route if it remains visible;
2. otherwise discard its bridge and remove it from forward history;
3. select the first visible route by `(group_rank, screen_rank, stable_id)`;
4. if no route is visible, show the intentional `NoAccessibleModules` shell state; and
5. never instantiate a bridge until the route passes the current snapshot again.

No hidden route is kept alive in a cache.

### Activation changes

The initial snapshot is published after Phase 6 Step 10 module registration and before Step 12 window publication. Runtime activation changes are emitted only after the administration transaction commits. The composition-root event subscriber copies the new activation and queues one navigation refresh through the GUI dispatcher.

Permission/session changes use the same refresh path with a new `session_generation`. Logout clears history and all bridges before replacing the snapshot.

## Initial navigation manifest

The first production manifest has one primary route per module. Later phases may add detail routes without changing the access model.

| Group | Stable ID | Owner | Required right |
|---|---|---|---|
| System | `administration.home` | administration | `right_person_manage` |
| Work | `parties.list` | parties | `right_party_read` |
| Work | `catalog.list` | catalog | `right_product_read` |
| Work | `pricing.rates` | pricing | `right_rate_read` |
| Work | `orders.list` | orders | `right_order_read` |
| Finance | `receivables.invoices` | receivables | `right_invoice_read` |
| Work | `jobs.list` | jobs | `right_job_read` |
| Sales | `quotations.list` | quotations | `right_quotation_read` |
| Sales | `agreements.list` | agreements | `right_agreement_read` |
| Purchasing | `sourcing.suppliers` | sourcing | `right_supplier_read` |
| Work | `companion.tasks` | companion | `right_task_read` |
| Files | `files.search` | files | `right_file_search` |

Administration sub-routes for devices, rights, module activation, shop settings, and audit will be added only with their exact rights; `administration.home` must not grant access to those commands.

## Implementation sequence

### 7.2.0 — Build and contract preflight

Files:

- `src/ui/CMakeLists.txt`
- `src/shell/screen_registry.hpp`
- `src/shell/screen_registry.cpp`
- `tests/shell/screen_registry_test.cpp`

Work:

- Correct Qt-off test scoping.
- Introduce typed optional rights, group rank, screen rank, and immutable access snapshots.
- Reject invalid module IDs, invalid rights, cross-module rights, empty/oversized IDs, malformed component URLs, duplicate IDs, missing factories, and registry overflow.
- Preserve the 128-screen hard bound.

Gate: portable registry tests and independent Qt-off CMake configure/build/CTest.

### 7.2.1 — Production manifest and completeness policy

Files:

- `src/shell/navigation_manifest.hpp`
- `src/shell/navigation_manifest.cpp`
- `src/shell/modules/*/navigation.cpp`
- `tools/sandbox/check_navigation_manifest.py`
- `tests/shell/navigation_manifest_test.cpp`

Work:

- Register the twelve primary contributions in an explicit composition order.
- Keep each bridge factory in the shell adapter for its owning module.
- Verify every compiled module is either represented or explicitly declared non-navigable.
- Verify every component URL is compiled into the QML module.
- Reject duplicate ranks within a group only when they would make ordering ambiguous.

Gate: manifest completeness, exact module/right ownership, resource existence, and no Qt/domain leakage.

### 7.2.2 — Portable navigation state machine

Files:

- `src/shell/navigation_controller.hpp`
- `src/shell/navigation_controller.cpp`
- `tests/shell/navigation_controller_test.cpp`

Work:

- Compute visible rows from the registry and immutable access snapshot.
- Select by stable ID and return explicit `Result` failures for unknown, inactive, unauthorized, or stale routes.
- Own exactly one active `PresentationBridge`.
- Reconcile selection and bounded back/forward history on access changes.
- Clear bridge and history on session generation change.
- Make repeated refreshes idempotent.

Gate: normal, empty, malformed, disabled, transitive-disabled, unauthorized, stale-generation, logout, repeated-refresh, and factory-failure cases.

### 7.2.3 — Qt navigation model and GUI-thread bridge

Files:

- `src/shell/navigation_model_qt.hpp`
- `src/shell/navigation_model_qt.cpp`
- `src/shell/navigation_bridge_qt.hpp`
- `src/shell/navigation_bridge_qt.cpp`
- `tests/shell/qt_navigation_test.cpp`

Model roles:

- `stableId`
- `titleKey`
- `iconName`
- `componentUrl`
- `groupKey`
- `groupRank`
- `screenRank`
- `selected`

Work:

- Derive the model from `QAbstractListModel`.
- Apply every mutation on the model thread; worker-originated refreshes use queued delivery and `QPointer` lifetime protection.
- Expose commands by stable ID, not row number.
- Recheck access immediately before bridge creation.
- Emit bounded, deterministic model changes; use reset only for session replacement or broad activation changes.

Gate: Qt test for roles, ordering, queued refresh, deleted receiver, stale generation, selection changes, and zero-visible-screen behavior.

### 7.2.4 — Responsive QML navigation shell

Files:

- `src/ui/navigation/NavigationRail.qml`
- `src/ui/navigation/NavigationDrawer.qml`
- `src/ui/navigation/NavigationHost.qml`
- `src/ui/navigation/NoAccessibleModules.qml`
- `src/ui/Main.qml`
- `src/ui/CMakeLists.txt`

Work:

- Use a permanent rail at wide widths and a drawer at compact widths.
- Drive delegates exclusively from `NavigationModelQt` roles.
- Load the selected component through the controlled navigation bridge.
- Provide keyboard focus, visible focus indicators, accessible names, tooltips in collapsed mode, and deterministic shortcut handling.
- Persist only the stable selected ID. Never persist row numbers or component pointers.
- Keep close handling routed to `applicationSurface`; no `Qt.quit()`.

Gate: QML resource policy plus offscreen Qt tests at wide/compact boundaries, keyboard navigation, inaccessible-route rejection, and root creation failure.

### 7.2.5 — Activation, rights, and lifecycle integration

Files:

- `src/app/composition_root.hpp`
- `src/app/composition_root.cpp`
- `src/app/events/` activation/session event contracts
- shell startup wiring for Step 11
- integration tests under `tests/app/` and `tests/shell/`

Work:

- Publish the initial immutable navigation snapshot from the registered module set, resolved activation, and current session.
- Freeze contribution registration before the window is created.
- Publish committed activation and rights changes through the existing synchronous domain event bus.
- Queue shell refreshes without exposing the event bus or module registry to QML.
- Disconnect subscribers and destroy the active bridge during Step 11 rollback/shutdown.

Gate: startup order, rollback, activation closure, rights revocation while selected, logout, and window-close races.

### 7.2.6 — Complete quality gate

Work:

- Add all portable tests to the strict sandbox gate.
- Register Qt tests only when `SQUIFLOW_WITH_QT=ON`.
- Run Linux strict, independent CMake/CTest, and Qt-capable Linux/MSVC lanes.
- Record the hosted Qt/MSVC run separately if it cannot execute in the local image.
- Create a `.git`-inclusive checkpoint only after a clean worktree and `git fsck --full`.

## Test matrix

### Visibility and security

- Core active + right held: visible.
- Core active + right missing: hidden.
- Extra disabled + right held: hidden while grant remains unchanged.
- Dependency disabled transitively: dependent screen hidden.
- Module active + cross-module right: contribution rejected at startup.
- Hidden route requested by ID, shortcut, persisted state, or history: rejected without factory invocation.
- Rights revoked while selected: bridge destroyed and route reconciled.

### State and concurrency

- Empty visible set produces the explicit no-access state.
- Duplicate refresh is a no-op.
- Older navigation revision cannot overwrite a newer one.
- Worker refresh after model destruction is dropped safely.
- Session generation change clears route history.
- Activation update during root creation either completes before publication or fails startup and unwinds.

### Boundaries and malformed input

- 128 contributions accepted; 129th rejected.
- IDs at the maximum accepted length pass; empty, oversized, unsafe, and duplicate IDs fail.
- Invalid module and right enum values fail before indexing arrays or bitsets.
- Missing QML component, missing title key, invalid rank, and null factory fail startup.

### Build and resources

- Qt-off configuration never creates Qt tests or references Qt targets.
- Every QML destination is listed in `qt_add_qml_module`.
- No screen selects a style or calls `Qt.quit()`.
- No Qt token appears below `src/shell/`.

## Acceptance criteria

Phase 7.2 is complete only when:

- all twelve primary module routes are declared once;
- visible routes exactly match registered modules, resolved activation, and rights;
- unauthorized and inactive routes cannot instantiate bridges through any path;
- selection reconciles deterministically after activation, rights, and session changes;
- all model mutations are GUI-thread confined;
- QML uses only the navigation model and bridge surface;
- portable strict and CMake gates pass;
- Qt-capable Linux and MSVC navigation tests pass; and
- the final worktree is clean and checkpoint integrity is verified.

## Commit sequence

1. `fix(ui): scope Qt tests to Qt builds`
2. `refactor(shell): type navigation access contracts`
3. `feat(shell): register the module navigation manifest`
4. `feat(shell): reconcile activation-aware navigation`
5. `feat(shell): expose the Qt navigation model`
6. `feat(ui): add responsive module navigation`
7. `feat(app): publish navigation access snapshots`
8. `test(ui): gate Phase 7.2 navigation and visibility`
9. `docs: record the Phase 7.2 quality gate`

Each commit must compile and pass its focused tests before the next begins. No placeholder pages, disabled checks, or deferred TODOs are permitted.
