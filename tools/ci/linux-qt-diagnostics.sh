#!/usr/bin/env bash
set -u
set -o pipefail

BUILD_DIR="${BUILD_DIR:-build/linux-qt-release}"
REPORT_DIR="${REPORT_DIR:-build/linux-qt-diagnostics}"
EXE="$BUILD_DIR/src/ui/squiflow_workstation"
mkdir -p "$REPORT_DIR"

status=0

# The repository's policy/integrity checks use PyYAML. Keep this dependency
# isolated to the diagnostic runner rather than relying on a preinstalled
# package on the GitHub-hosted runner.
PYTHON_VENV="$REPORT_DIR/python-venv"
if [ ! -x "$PYTHON_VENV/bin/python" ]; then
  python3 -m venv "$PYTHON_VENV"
  "$PYTHON_VENV/bin/python" -m pip install --disable-pip-version-check --quiet PyYAML
fi
export PATH="$PYTHON_VENV/bin:$PATH"

run_capture() {
  local name="$1"; shift
  local log="$REPORT_DIR/${name}.log"
  echo "[linux-qt-diagnostics] $name: $*"
  {
    printf '$'
    printf ' %q' "$@"
    printf '\n\n'
    "$@"
  } 2>&1 | tee "$log"
  local rc=${PIPESTATUS[0]}
  printf '\n[exit=%s]\n' "$rc" >> "$log"
  if [ "$rc" -ne 0 ]; then
    echo "[linux-qt-diagnostics] $name failed with exit code $rc"
    status=$rc
  fi
  return 0
}

cat > "$REPORT_DIR/environment.txt" <<EOF2
=== GitHub runner ===
runner_os=${RUNNER_OS:-unknown}
runner_arch=${RUNNER_ARCH:-unknown}
os=${RUNNER_OS:-unknown}

=== Tool versions ===
$(cmake --version 2>&1 || true)
$(ninja --version 2>&1 || true)
$(gcc --version 2>&1 | head -1 || true)
$(g++ --version 2>&1 | head -1 || true)

=== Relevant environment ===
$(env | sort | grep -E '^(QT|QML|QSG|CMAKE|SQUIFLOW|CC|CXX|LD_|VCPKG|PATH=)' || true)
EOF2

run_capture "qt_environment" bash -lc '
  echo "QT_ROOT=${QT_ROOT:-}"
  echo "Qt6_DIR=${Qt6_DIR:-}"
  command -v qmake6 || true
  command -v qmake || true
  command -v qtpaths6 || true
  command -v qtpaths || true
  qmake6 -query 2>&1 || qmake -query 2>&1 || true
  qtpaths6 --qt-version 2>&1 || qtpaths --qt-version 2>&1 || true
'

run_capture "pipeline_policy" python3 tools/ci/check_pipeline.py
run_capture "source_integrity" make -f tools/sandbox/Makefile check

if [ ! -d "$BUILD_DIR" ]; then
  echo "[linux-qt-diagnostics] build directory does not exist before workflow" > "$REPORT_DIR/prebuild-state.txt"
else
  find "$BUILD_DIR" -maxdepth 2 -type f \( -name 'CMakeCache.txt' -o -name 'CMakeError.log' -o -name 'CMakeOutput.log' \) -print > "$REPORT_DIR/prebuild-state.txt" 2>&1 || true
fi

run_capture "cmake_workflow" cmake --workflow --preset linux-qt-check

# Preserve the exact CMake state even when the workflow fails.
if [ -d "$BUILD_DIR" ]; then
  cmake -LAH -N "$BUILD_DIR" > "$REPORT_DIR/cmake-cache.txt" 2>&1 || true
  cp -f "$BUILD_DIR/CMakeCache.txt" "$REPORT_DIR/CMakeCache.txt" 2>/dev/null || true
  cp -f "$BUILD_DIR/CMakeFiles/CMakeError.log" "$REPORT_DIR/CMakeError.log" 2>/dev/null || true
  cp -f "$BUILD_DIR/CMakeFiles/CMakeOutput.log" "$REPORT_DIR/CMakeOutput.log" 2>/dev/null || true
  find "$BUILD_DIR" -type f \( -name '*.qmlc' -o -name '*.qmltypes' -o -name '*qmldir*' \) -print > "$REPORT_DIR/qml-generated-files.txt" 2>&1 || true
fi

run_capture "ctest" ctest --test-dir "$BUILD_DIR" --output-on-failure --no-tests=error

if [ -x "$EXE" ]; then
  run_capture "executable_file" file "$EXE"
  run_capture "executable_dependencies" bash -lc "ldd '$EXE' 2>&1 || true"
  run_capture "executable_qt_strings" bash -lc "strings '$EXE' | grep -E 'Qt|QML|QWindowKit|QGuiApplication|qml' | head -200 || true"
else
  echo "[linux-qt-diagnostics] executable missing: $EXE" | tee "$REPORT_DIR/executable-missing.log"
  status=${status:-1}
  [ "$status" -eq 0 ] && status=127
fi

if [ -x "$EXE" ]; then
  # Never let a GUI/runtime hang consume the entire CI timeout.
  run_capture "smoke_test" timeout --signal=TERM --kill-after=10s 90s env \
    QT_QPA_PLATFORM=offscreen \
    QSG_RHI_BACKEND=software \
    QT_DEBUG_PLUGINS=1 \
    QML_IMPORT_TRACE=1 \
    QSG_INFO=1 \
    "$EXE" --smoke-test
fi

if [ -d "$BUILD_DIR/Testing/Temporary" ]; then
  cp -a "$BUILD_DIR/Testing/Temporary/." "$REPORT_DIR/ctest-temporary/" 2>/dev/null || true
fi

find "$BUILD_DIR" -maxdepth 4 -type f \( \
  -name '*.log' -o -name '*.json' -o -name '*.xml' -o -name '*.txt' \
\) -print > "$REPORT_DIR/build-diagnostic-files.txt" 2>&1 || true

cat > "$REPORT_DIR/README.txt" <<EOF2
SquiFlow linux-qt diagnostic bundle

The bundle records:
- runner/tool/environment information
- Qt discovery and Qt installation metadata
- pipeline/source integrity checks
- exact cmake --workflow output
- CMake cache/error/output state
- explicit CTest output with --output-on-failure
- executable type and dynamic dependencies
- QML/Qt strings present in the executable
- offscreen smoke-test output with QT_DEBUG_PLUGINS, QML_IMPORT_TRACE and QSG_INFO
- CTest temporary diagnostics

A non-zero exit from any required phase causes the diagnostic step to fail.
EOF2

exit "$status"
