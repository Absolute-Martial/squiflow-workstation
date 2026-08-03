# QML presentation bridge architecture

## Decision

QML is the visual bridge, not a domain or infrastructure boundary. Domain, engine, workflows and Phase 6 lifecycle remain pure C++23. Qt types terminate in `src/shell`; QML receives only presentation-safe properties, stable identifiers, commands and `QAbstractItemModel` roles.

## Ordered implementation

1. Add an enforceable architecture policy rejecting Qt/QObject usage below the shell boundary and rejecting direct domain access from QML.
2. Add primitive mapping and a GUI dispatcher in `src/shell`, including exact minor-unit money formatting without floating point.
3. Add a bounded `QAbstractListModel` adapter that owns presentation snapshots and marshals every mutation to the GUI thread.
4. Add a Qt surface wrapper for Phase 6.8: create the QML engine at Step 11, treat root creation failure as `StepDisposition::Failed`, expose controlled activation, and map close intent to lifecycle shutdown.
5. Compile QML/resources into the executable, centralize theme/style configuration, and prohibit `Qt.quit()`.
6. Gate pure-C++ behavior locally and Qt behavior in the Qt-capable MSVC/Linux CI lanes.

## Corrections to the proposed seam

- Money is exposed as signed 64-bit minor units and/or a formatted `QString`; it is never exposed as `double`.
- `FluentWinUI3` is selected centrally with `Fusion` fallback, but no screen may import or select either style directly.
- QML engine creation is Step 11 (`Shell`); Step 12 only publishes/shows the root window. Reverse shutdown closes the window before destroying the engine.
