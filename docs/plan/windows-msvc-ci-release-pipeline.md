# Windows MSVC, CLion, CI and release pipeline

## Objective

Provide one CMake Presets contract used without divergent flags by CLion, local command lines and GitHub Actions. Build a real Windows executable with MSVC, run automated tests, stage runtime dependencies and publish immutable release artifacts.

## Ordered implementation

1. Replace the machine-specific presets with portable Linux and Windows-MSVC configure, build, test, workflow and package presets.
2. Make dependency selection explicit: a pinned vcpkg checkout supplies C/C++ packages; Qt 6.11.1 supplies the Qt runtime.
3. Add a Windows staging target that copies the executable, runs `windeployqt`, includes notices and fails when required tools are missing.
4. Add CI policy validation and two GitHub Actions lanes: the fast strict Linux gate and the production Windows MSVC gate.
5. Add a tag-only release workflow that rebuilds, retests, stages, archives, hashes and publishes the Windows bundle.
6. Document CLion import, build, test, debug and package operations using the same presets.
7. Validate JSON, YAML and CMake locally; run the complete portable strict/CMake tests; retain Windows runtime verification for the hosted Windows runner.

## Gates

- No unpinned placeholder baseline remains in `vcpkg.json`.
- Every configure preset has matching build and test presets where applicable.
- Release publication is tag-only, least-privilege and never publishes a failed test build.
- Artifacts contain the executable, Qt runtime when enabled, license notices and SHA-256 checksums.
- CLion needs no generated project files or committed `.idea` directory.
