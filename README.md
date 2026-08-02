# SquiFlow Workstation

Windows desktop client for the shop. Qt Quick interface, embedded local engine,
local SQLite cache, syncing to the shop server over HTTPS and WebSockets.

## Layout

    cmake/          all build logic; no target file contains a flag
    external/       submodules (the protocol repository)
    src/app/        startup and composition; the only place that knows every module
    src/platform/   the Windows boundary, plus fakes
    src/engine/     shared mechanisms: storage, records, sync, identity, services
    src/workflows/  the only layer allowed to depend on several modules
    src/modules/    the twelve modules, all laid out identically
    src/shell/      window, navigation, search, attention list
    src/ui/         QML vocabulary, compiled into the binary
    tests/          cross-boundary tests
    packaging/      staging, manifest, signing, installer, updater
    docs/           decisions that must survive

## Building for real

See `docs/building.md`. Requires CMake, a pinned Qt, and the dependency manager.

## The verification lane

A plain makefile at `tools/sandbox/Makefile` builds the parts that depend on
neither Qt nor SQLite, at C++20, so they can be compiled and executed anywhere.
It is a check, not the build.

    make -f tools/sandbox/Makefile check
