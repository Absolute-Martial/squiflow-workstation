# Building and releasing the Phase 7.7 workstation

Phase 7.7 adds the responsive application shell, shared page states, dashboard
read contract, permission-filtered dashboard service, bounded notifications,
unsaved-change interception, and Linux/Windows Qt runtime gates.

## Pinned inputs

- Qt 6.11.1 with Core, Gui, Qml, Quick, QuickControls2, ShaderTools,
  ImageFormats, and TaskTree packages.
- QWindowKit 1.5.1 source under `external/ui-fluent/qwindowkit`.
- qmsetup plus the supplied syscmdline commit
  `0c9f3de8b11bd2f33b03bea5521bf446af4ead69`.
- The supplied qtimageformats 6.11.1 source under
  `external/qt/qtimageformats` for offline inspection/rebuild evidence.
- The vcpkg baseline and other versions recorded in `vcpkg.json` and
  `.github/workflows/*.yml`.

Never substitute an arbitrary latest snapshot in a release build. Update the
pin, provenance, CI lane, and runtime evidence together.

## Linux strict portable lane

This is the fastest mandatory gate and requires no Qt SDK:

```bash
make -f tools/sandbox/Makefile check
export SQUIFLOW_SODIUM_ROOT="$(tools/ci/build-libsodium.sh)"
cmake --workflow --preset linux-check
```

It compiles C++23 with warnings as errors, checks every header alone, enforces
module/QML/provider/dashboard boundaries, and runs all portable tests.

## Linux Qt 6.11.1 lane

Install Qt 6.11.1 and place its `bin` directory on `PATH`, or set
`CMAKE_PREFIX_PATH`/`Qt6_DIR` to the kit. The kit must include private headers
because QWindowKit Core+Quick uses documented private Qt build targets.

```bash
export SQUIFLOW_SODIUM_ROOT="$(tools/ci/build-libsodium.sh)"
cmake --workflow --preset linux-qt-check
QT_QPA_PLATFORM=offscreen QSG_RHI_BACKEND=software \
  ./build/linux-qt-release/src/app/squiflow_workstation --smoke-test
```

This lane builds the actual QML resources and QWindowKit, runs CTest, creates the
root window offscreen, opens the dashboard, and exits through the normal shell
shutdown path. CI performs the same commands in the `linux-qt` job.

## Windows MSVC 2022 lane

Use a Visual Studio 2022 x64 Developer PowerShell with CMake, Ninja, Git, Qt
6.11.1 `win64_msvc2022_64`, and the pinned vcpkg checkout.

```powershell
$env:VCPKG_ROOT = "$PWD\.vcpkg"
$env:SQUIFLOW_VCPKG_ROOT = $env:VCPKG_ROOT
& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics
cmake --workflow --preset windows-msvc-check
$env:QT_QPA_PLATFORM = "offscreen"
$env:QSG_RHI_BACKEND = "software"
& .\build\windows-msvc-release\src\app\squiflow_workstation.exe --smoke-test
```

`/W4 /WX` is mandatory. The preset enables Qt and QWindowKit, links dynamically
to Qt, and runs all CTest programs. The GitHub `windows-msvc` job installs the
same Qt version and uses the pinned vcpkg ref.

## QWindowKit and syscmdline

`SQUIFLOW_WITH_QWINDOWKIT=ON` builds only QWindowKit Core+Quick. Widgets,
examples, documentation, and installation targets stay disabled. syscmdline is
a host-build dependency of qmsetup; it is not a runtime plugin or application
command parser.

To isolate this build during diagnosis:

```bash
cmake --preset linux-qt-release -DSQUIFLOW_WITH_QWINDOWKIT=ON
cmake --build --preset linux-qt-release --target QWKQuick
```

The actual backdrop, title-bar hit testing, snap layout, DPI, and multi-monitor
visual evidence remains part of the Phase 7.10 machine gate.

## qtimageformats and AVIF

The user-supplied `qtimageformats` tree is the official 6.11.1 source snapshot.
The normal CI kit installs Qt's prebuilt `qtimageformats` module. Rebuilding the
source separately requires the matching Qt 6.11.1 prefix and image codec
dependencies:

```bash
cmake -S external/qt/qtimageformats -B build/qtimageformats \
  -DCMAKE_PREFIX_PATH="$QT_ROOT"
cmake --build build/qtimageformats --parallel 2
```

Do not copy a plugin from another Qt minor version into the release. AVIF
runtime decoding is not claimed until a produced or installed plugin reports
support and the Phase 7.10 AVIF fixtures pass.

## Release build

A `vMAJOR.MINOR.PATCH` tag starts `.github/workflows/release.yml`. The release
job:

1. validates the tag and pinned toolchain;
2. configures MSVC RelWithDebInfo with warnings as errors;
3. builds Qt, QML, QWindowKit, and all tests;
4. runs CTest;
5. stages the executable using `windeployqt`;
6. launches the staged dashboard with the offscreen smoke test;
7. creates runtime and source archives;
8. writes `SHA256SUMS.txt`;
9. uploads an immutable workflow artifact; and
10. publishes the GitHub release.

A release is rejected if tests, deployment, staged launch, archive creation, or
checksum generation fails. Publicly trusted signing remains Phase 9.3 work; the
workflow must never silently publish an unsigned artifact once that gate is
enabled.

## Local evidence and limitations

The portable sandbox can prove domain/presentation behavior and static policy,
but it cannot claim a Qt runtime result without a Qt SDK. Record actual Linux
Qt and Windows MSVC job URLs, screenshots, plugin lists, and measurements in
`docs/qa/phase-7.7-dashboard-gate.md` after CI runs.
