#!/usr/bin/env python3
"""Static Phase 7.7 dashboard, shell, dependency, CI, and release gate."""
from pathlib import Path
import json
import sys
import yaml

ROOT = Path(__file__).resolve().parents[2]
errors: list[str] = []


def read(path: str) -> str:
    candidate = ROOT / path
    if not candidate.exists():
        errors.append(f"{path}: missing")
        return ""
    return candidate.read_text(encoding="utf-8")


def require(path: str, tokens: tuple[str, ...]) -> None:
    text = read(path)
    for token in tokens:
        if token not in text:
            errors.append(f"{path}: missing {token}")


require("src/app/dashboard/dashboard_service.cpp", (
    "kMaximumMetrics", "std::erase_if", "activation.is_active",
    "context.permissions().has", "dashboard.error.invalid_snapshot"))
require("src/shell/dashboard_bridge.cpp", (
    "session_generation", "stale()", "OfflineState", "generation_"))
require("src/shell/notification_queue.cpp", (
    "kMaximumVisible", "deduplication_key", "pop_front", "occurrences"))
require("src/shell/shell_state.cpp", (
    "request_route", "pending_route_", "resolve_unsaved", "dirty_"))
require("src/shell/qml_surface_qt.cpp", (
    'setContextProperty("shellState"', 'setContextProperty("navigationBridge"'))
require("src/shell/navigation_bridge_qt.cpp", (
    "DashboardPresentationBridge", "DashboardBridgeQt", "current_dashboard_bridge_"))
bootstrap = read("src/ui/workstation_main_qt.cpp")
for token in ("QGuiApplication", "QmlSurfaceQt", "--smoke-test", "requestShutdown"):
    if token not in bootstrap:
        errors.append(f"src/ui/workstation_main_qt.cpp: missing {token}")
if "grant_all" in bootstrap:
    errors.append("src/ui/workstation_main_qt.cpp: unauthenticated bootstrap must not grant rights")

require("src/ui/Main.qml", (
    "Ctrl+K", "globalSearch", "unsavedDialog", "notificationPopup",
    "connectivityState", "setThemeChoice", "compactNavigation"))
require("src/ui/dashboard/DashboardPage.qml", (
    "MetricCard", "Recent activity", "Quick actions", "dashboardBridge",
    "navigationBridge", "refresh()"))
require("src/ui/Theme.qml", (
    "Application.styleHints.colorScheme", "surfaceRaised", "positive",
    "warning", "focus", "pageMarginCompact"))

qml_cmake = read("src/ui/CMakeLists.txt")
for relative in (
    "common/PageScaffold.qml", "common/CommandBar.qml",
    "common/StatusBanner.qml", "common/MetricCard.qml",
    "common/SectionCard.qml", "common/EmptyState.qml",
    "common/ErrorState.qml", "common/LoadingSkeleton.qml",
    "common/ConfirmDialog.qml", "common/UnsavedChangesDialog.qml",
    "dashboard/DashboardPage.qml"):
    if relative not in qml_cmake:
        errors.append(f"src/ui/CMakeLists.txt: {relative} is not embedded")

manifest = read("src/shell/navigation_manifest.cpp")
if '"dashboard.home"' not in manifest or "DashboardPresentationBridge" not in manifest:
    errors.append("navigation manifest: real dashboard route missing")

sys_pin = read("external/ui-fluent/qwindowkit/qmsetup/src/syscmdline/SQUIFLOW_SOURCE_PIN")
if "0c9f3de8b11bd2f33b03bea5521bf446af4ead69" not in sys_pin:
    errors.append("syscmdline: supplied commit pin missing")
qt_pin = read("external/qt/qtimageformats/SQUIFLOW_SOURCE_PIN")
if "version=6.11.1" not in qt_pin:
    errors.append("qtimageformats: Qt 6.11.1 source pin missing")
require("src/ui/CMakeLists.txt", (
    "SQUIFLOW_WITH_QWINDOWKIT", "QWINDOWKIT_BUILD_WIDGETS OFF",
    "QWINDOWKIT_BUILD_QUICK ON", "QWKQuick"))

try:
    presets = json.loads(read("CMakePresets.json"))
except Exception as failure:
    errors.append(f"CMakePresets.json: {failure}")
    presets = {}
workflows = {item.get("name") for item in presets.get("workflowPresets", [])}
for name in ("linux-check", "linux-qt-check", "windows-msvc-check",
             "windows-msvc-release-bundle"):
    if name not in workflows:
        errors.append(f"CMakePresets.json: missing workflow {name}")

for path in (".github/workflows/ci.yml", ".github/workflows/release.yml"):
    try:
        yaml.safe_load(read(path))
    except Exception as failure:
        errors.append(f"{path}: {failure}")
require(".github/workflows/ci.yml", (
    "linux-strict", "linux-qt", "windows-msvc", "--smoke-test",
    "linux-qt-check", "windows-msvc-check"))
require(".github/workflows/release.yml", (
    "windows-msvc-release-bundle", "--smoke-test", "SHA256SUMS.txt",
    "Publish GitHub release"))
require("docs/building-phase-7.7.md", (
    "Linux strict portable lane", "Linux Qt 6.11.1 lane",
    "Windows MSVC 2022 lane", "Release build", "QWindowKit",
    "qtimageformats"))

if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("Phase 7.7 policy: dashboard, shell, supplied Qt sources, CI and release passed")
