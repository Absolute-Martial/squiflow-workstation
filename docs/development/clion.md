# CLion setup

CLion reads `CMakePresets.json` directly. Open the repository root, enable **Settings → Build, Execution, Deployment → CMake → Enable CMake Presets integration**, then select a host-compatible preset.

## Windows

1. Install Visual Studio 2022 with **Desktop development with C++** and a Windows SDK.
2. Set `VCPKG_ROOT` to the pinned vcpkg checkout and bootstrap it.
3. Install/build Qt 6.11.1 and put its prefix on `CMAKE_PREFIX_PATH`; ensure `windeployqt` is on `PATH`.
4. Select `windows-msvc-debug` for development or `windows-msvc-release` for release-equivalent debugging.
5. Build `squiflow_workstation`, run CTest through CLion, and build `squiflow_archive` for the deployable ZIP.

No `.idea` directory or CLion-specific compiler flags are required. The IDE, command line and GitHub Actions use the same configure/build/test presets.
