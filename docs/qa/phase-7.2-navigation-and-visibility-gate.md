# Phase 7.2 -- navigation and module visibility gate

Date: 2026-08-04

## Result

The production navigation contracts, manifest, reconciliation controller,
Qt model/bridge source, responsive rail/drawer QML, and QML surface wiring
are implemented. The portable lane is green. The Qt runtime lane is
registered but could not be executed in this image because no built Qt
6.11.1 SDK is installed; this sub-phase therefore remains `[~]` rather
than being represented as fully verified.

## Delivered contracts

- `ScreenContribution::required_right` is now
  `std::optional<protocol::RightId>` and is rejected if invalid or owned by
  another module.
- `NavigationAccess` owns copies of activation, rights, registered-module
  membership, session generation, and navigation revision. It retains no
  `Session` or `modules::Registry` pointer.
- Visibility is exactly: registered owner, active owner, and required right
  granted.
- Ordering is exactly `(group_rank, screen_rank, stable_id)`.
- The production manifest declares one primary route for each of the twelve
  modules and validates completeness against the registered module set.
- `NavigationController` owns one bridge only, rejects unknown/hidden/stale
  routes before factory invocation, destroys a revoked bridge before
  selecting a fallback, and clears bridge/history on session replacement.
- `NavigationModelQt` exposes presentation-safe roles only; worker access
  snapshots are delivered through `Qt::QueuedConnection` with `QPointer`
  lifetime protection.
- The QML shell uses a wide-window rail and compact-window drawer, routes by
  stable ID, provides keyboard/accessibility labels, and routes close intent
  through `applicationSurface` instead of `Qt.quit()`.
- Qt-only tests are nested inside `if(SQUIFLOW_WITH_QT)`, so Qt-off builds do
  not reference Qt targets.

## Focused evidence

```text
screen registry:       25 checks, 0 failed
navigation manifest:   79 checks, 0 failed
navigation controller: 28 checks, 0 failed
QML boundary fixtures: 6 cases passed
```

The focused cases cover invalid owner/right, cross-module right, unsafe and
oversized IDs, malformed component URLs, null factories, 128/129 capacity,
activation filtering with retained grants, deterministic ordering,
manifest completeness, unauthorized non-construction, revoked current
routes, session replacement, stale revisions, empty visible sets, bounded
history, and bridge-construction failure.

## Full local gate

```text
47 test programs
5,484 assertions, 0 failed
174 self-contained headers, 0 failed
integrity: 388 files checked, all pass
Navigation/list policy passed
UI resource policy passed
Qt bridge policy passed
QML boundary policy and six fixtures passed
```

Command:

```text
make -f tools/sandbox/Makefile check
```

## Deferred runtime evidence

The local image reports `cmake: command not found` and has no built Qt
6.11.1 SDK. Consequently these already-registered tests still need to run
on the Qt-capable Linux/MSVC lane before changing 7.2 from `[~]` to `[x]`:

- exact Qt role values and model signals;
- worker-thread queued refresh and receiver destruction;
- compiled QML component resolution;
- wide/compact layout behavior and keyboard/focus restoration;
- QML load failure and no unauthorized component instantiation.

## Commits

```text
0ac4074 refactor(shell): type navigation access contracts
8f0c4f7 feat(shell): register the module navigation manifest
a0354de feat(shell): reconcile activation-aware navigation
641e1b3 feat(ui): add responsive activation-aware navigation
```
