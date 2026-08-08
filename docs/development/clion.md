# CLion setup

CLion reads `CMakePresets.json` directly. Open the repository root, enable **Settings → Build, Execution, Deployment → CMake → Enable CMake Presets integration**, then select a host-compatible preset.

## Windows

1. Install Visual Studio 2022 with **Desktop development with C++** and a Windows SDK.
2. Set `VCPKG_ROOT` to the pinned vcpkg checkout and bootstrap it.
3. Install/build Qt 6.11.1 and put its prefix on `CMAKE_PREFIX_PATH`; ensure `windeployqt` is on `PATH`.
4. Select `windows-msvc-debug` for development or `windows-msvc-release` for release-equivalent debugging.
5. Build `squiflow_workstation`, run CTest through CLion, and build `squiflow_archive` for the deployable ZIP.

## Linux

The committed `linux-gcc-debug` preset is the Qt-free verification lane. To debug the QML surface on a machine with the Qt online installer kit, create a `CMakeUserPresets.json` (kept out of git) that turns Qt on and points at the kit:

```json
{
  "version": 6,
  "configurePresets": [
    {
      "name": "linux-clion-qt-debug",
      "displayName": "Linux GCC Debug (with Qt 6.11 CLion)",
      "inherits": "linux-gcc-debug",
      "cacheVariables": {
        "SQUIFLOW_WITH_QT": "ON",
        "CMAKE_PREFIX_PATH": "$env{HOME}/Qt/6.11.1/gcc_64"
      }
    }
  ]
}
```

Point `CMAKE_PREFIX_PATH` at the directory that actually contains
`lib/cmake/Qt6/Qt6Config.cmake`, then select `linux-clion-qt-debug` and
reconfigure. If a previous profile left a stale `CMAKE_PREFIX_PATH` or an
earlier Qt cache value, delete that build directory before reconfiguring.

No `.idea` directory or CLion-specific compiler flags are required. The IDE, command line and GitHub Actions use the same configure/build/test presets.
