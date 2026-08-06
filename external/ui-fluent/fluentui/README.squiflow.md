# zhuzichu520/FluentUI (vendored)

- Source: https://github.com/zhuzichu520/FluentUI (`main`, uploaded as a zip snapshot by the user on 2026-08-06; no commit SHA available from the archive)
- License: MIT (see `License` in this directory)
- Scope: C++/QML Fluent Design component library. All QML types use a `Flu`
  prefix (e.g. `FluButton`, `FluWindow`), so they cannot collide with Qt's
  own control names or with the other vendored libraries here.

## What was kept
- `src/` in full: the plugin registration (`fluentuiplugin.*`, `FluentUI.*`),
  every `Flu*` component, and its bundled-in-tree helper libraries
  (`qhotkey/`, `qrcode/`, `qmlcustomplot/`) -- none of these needed a
  separate submodule fetch.
- Top-level `CMakeLists.txt`, `License`

## What was dropped
- `3rdparty/` (prebuilt Windows DLLs for mingw/msvc -- irrelevant off
  Windows, and on Windows these ship with the Qt runtime or vcpkg instead of
  being vendored as binaries)
- `example/`, `doc/`, `scripts/`, `.github/`, `.vscode/`

## Known gap -- build is not yet wired
- No CMake target in this repository references this directory yet. It
  needs a `SQUIFLOW_WITH_UI_FLUENT`-gated `add_subdirectory` call and a Qt
  6 toolchain to configure/build/verify, none of which exist in this
  sandbox.
