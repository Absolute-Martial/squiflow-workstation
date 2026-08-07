# QWindowKit (vendored)

- Source: https://github.com/stdware/qwindowkit (user-supplied snapshot)
- License: Apache-2.0 (`LICENSE`)
- Version in source: 1.5.1.0
- Scope: Qt Core+Quick frameless window and native backdrop support.

## SquiFlow integration

`SQUIFLOW_WITH_QWINDOWKIT=ON` adds the source as an isolated subdirectory,
links `QWKQuick` to the workstation, and disables Widgets, examples, docs, and
install targets. The application continues to own its public QML component
layer; pages never import QWindowKit directly.

The user-supplied qmsetup snapshot and its formerly missing `src/syscmdline`
submodule are now present. Linux Qt 6.11.1 and Windows MSVC CI build this lane.
Private Qt targets are a qualified build dependency, so a Qt upgrade must rerun
the native window, DPI, snap, and backdrop gates.
