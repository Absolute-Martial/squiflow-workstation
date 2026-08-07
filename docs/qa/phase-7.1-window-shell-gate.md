# Phase 7.1 -- Window and shell, geometry closed

Date: 2026-08-06

## Scope and decision rule

`src/ui/Main.qml`, `src/shell/qml_surface_qt.{hpp,cpp}`, and
`src/shell/surface_lifecycle.{hpp,cpp}` already existed from earlier
QML-bridge work and already satisfied two of 7.1's three invariants: root
window construction failure maps to a `PresentationError`/reverse rollback,
and window close always routes through `lifecycleBridge.requestShutdown()`,
never `Qt.quit()`. The concrete gap was the third invariant and the file the
plan names for it: persisted window geometry, validated before use, with a
safe fallback for malformed or off-screen state. That file did not exist
anywhere in the repository before this gate.

## What was added

- `src/shell/window_state.hpp/.cpp`: `WindowGeometry`, `kDefaultWindowGeometry`
  (1180x760), minimum-size constants, `is_valid_window_geometry(...)`, an
  abstract `WindowStateStore` (`load()`/`save()`, never throws), a one-file
  `FileWindowStateStore`, and `resolve_window_geometry(...)` which validates
  and falls back to the default. Portable C++, no Qt dependency, so it is
  fully covered by the sandbox lane.
- `tests/shell/window_state_test.cpp`: 21 checks covering geometry
  validation (too small, too large, off-screen, negative-but-reachable,
  maximized ignores bounds, unknown screen size), `resolve_window_geometry`
  fallback behavior, a simulated-restart round-trip, and real-disk
  `FileWindowStateStore` behavior including missing file, empty file,
  too-few-fields, non-numeric fields, and a bad maximized flag -- none of
  which crash or restore a bad geometry.
- `src/shell/qml_surface_qt.{hpp,cpp}` extended to accept an optional
  `WindowStateStore*`: when present, `startWindow()` resolves and applies
  the stored geometry (or `showMaximized()`) before `show()`, and
  `stopWindow()` captures the window's current geometry back into the
  store. This half cannot be exercised in the portable lane -- it is Qt
  API surface (`QWindow::setGeometry`, `QScreen::availableGeometry`) behind
  `SQUIFLOW_WITH_QT` -- and is honestly marked `[~]` here until the Qt lane
  runs it. No other call site constructs `QmlSurfaceQt` yet, so the
  constructor signature change is safe.

## Gate result (portable lane)

- 52 test programs, 5,653+ assertions, 0 failures.
- 179 self-contained headers, 0 not self-contained.
- 403 integrity files, all pass.
- Architecture/module-boundary/QML-boundary/Qt-bridge/navigation policies:
  all passed.
- `git diff --check` clean.

## What is still open

- The Qt-only half of `qml_surface_qt.cpp` (geometry application on
  `startWindow`/capture on `stopWindow`) has not run against a real Qt
  build; it needs the Windows/Qt runtime lane the rest of Phase 7 is also
  waiting on.
- `main.cpp` still does not construct a real `QmlSurfaceQt` or
  `WindowStateStore` -- that wiring depends on the same sign-in/session
  blocker already recorded in the Phase 6.8 gate doc.
- Phases 7.2 and 7.3 were already at `[~]` (portable/static gates passed,
  Qt runtime lane pending) before this gate and are unchanged by it; no
  new concrete gap like the missing `window_state` file was found in them.
