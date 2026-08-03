# CI entry points

- `python tools/ci/check_pipeline.py` validates presets, workflows and release policy.
- `cmake --workflow --preset linux-check` is the portable local/CLion gate.
- `cmake --workflow --preset windows-msvc-check` is the MSVC gate.
- `cmake --workflow --preset windows-msvc-release-bundle` tests and stages the release ZIP.

The Windows presets require `VCPKG_ROOT`, an MSVC developer environment and Qt 6.11.1 on `CMAKE_PREFIX_PATH`/`PATH`.
