# QWindowKit (vendored)

- Source: https://github.com/stdware/qwindowkit (`main`, uploaded as a zip snapshot by the user on 2026-08-06; no commit SHA available from the archive)
- License: Apache-2.0 (see `LICENSE` in this directory)
- Scope: cross-platform frameless window + native Mica/Acrylic/Blur backdrop support (Windows/macOS/Linux code paths), Qt Quick and Qt Core modules only.

## What was kept
- `src/core` and `src/quick` (the Widgets module was dropped; this project is Qt Quick only)
- Top-level `CMakeLists.txt`, `src/CMakeLists.txt`, `src/QWindowKitConfig.cmake.in`, `LICENSE`, `README.md`

## What was dropped
- `src/widgets` (QWidgets module, not used by this Qt Quick shell)
- `examples/`, `docs/`, `share/` (not needed to build the library)

## Build integration status
- `qmsetup` is now vendored from the user-provided snapshot.
- That snapshot itself has an empty `src/syscmdline` git submodule. `qmsetup` builds its host utility against `syscmdline`, so QWindowKit still cannot configure honestly until that final nested dependency is supplied or installed.
- No CMake target in this repository references this directory yet; it is
  vendored source only, gated to be wired once `qmsetup` is available and a
  Qt 6.8+ toolchain can actually build/verify it.
