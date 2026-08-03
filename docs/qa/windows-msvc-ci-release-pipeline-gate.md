# Windows MSVC, CI and release pipeline gate

## Implemented

- Shared CMake configure/build/test/workflow presets for Linux and Windows MSVC.
- CLion Presets integration instructions with no committed IDE state.
- GitHub pull-request/push CI with a strict Linux gate and Windows 2025 MSVC/Qt 6.11.1/vcpkg gate.
- Tag-only Windows release workflow that rebuilds, tests, stages with `windeployqt`, archives, hashes and publishes artifacts.
- Runtime staging includes the executable and third-party notices and fails on missing deployment inputs.
- Placeholder vcpkg baseline removed; the workflows pin the vcpkg tree to `2026.07.29`.

## Verified locally

- Pipeline policy and JSON/YAML parsing: passed.
- `cmake --workflow --preset linux-check`: 31/31 CTest tests passed.
- Strict source gate: 5,265 assertions, 0 failed.
- Git diff/integrity check: passed.

## Environment boundary

This Linux sandbox cannot execute MSVC, `windeployqt`, or GitHub-hosted Actions. The Windows workflow is therefore statically verified here; its first real run on `windows-2025` is the authoritative executable/runtime-deployment gate. Release publication remains blocked unless that Windows build and test job succeeds.
