#!/usr/bin/env python3
"""Static Qt/QML gate for Phase 7.2 navigation and Phase 7.3 lists."""
from pathlib import Path
import re
import sys

root = Path(__file__).resolve().parents[2]
errors = []

def read(path):
    candidate = root / path
    if not candidate.exists():
        errors.append(f"{path}: missing")
        return ""
    return candidate.read_text(encoding="utf-8")

def require(path, tokens):
    value = read(path)
    for token in tokens:
        if token not in value:
            errors.append(f"{path}: missing {token}")

manifest = read("src/shell/navigation_manifest.cpp")
if manifest.count("manifest.add(primary_route(") != 12:
    errors.append("navigation manifest: expected exactly twelve primary routes")
require("src/shell/navigation_model_qt.cpp", [
    '"stableId"', '"titleKey"', '"iconName"', '"componentUrl"',
    '"groupKey"', '"groupRank"', '"screenRank"', '"selected"',
    '"moduleId"', "beginResetModel", "QThread::currentThread"])
require("src/shell/navigation_bridge_qt.cpp", [
    "Qt::QueuedConnection", "QPointer", "apply_access", "select",
    "ListScreenBridgeQt", "shutdown"])
require("src/shell/list_screen_bridge_qt.cpp", [
    "pageRequested", "begin_refresh", "next_page", "select", "cancel"])
require("src/shell/qml_surface_qt.cpp", [
    'setContextProperty("navigationModel"',
    'setContextProperty("navigationBridge"',
    "make_navigation_manifest", "require_navigation_complete"])

ui_cmake = read("src/ui/CMakeLists.txt")
for path in (
    "navigation/NavigationRail.qml", "navigation/NavigationDrawer.qml",
    "navigation/NavigationHost.qml", "navigation/NoAccessibleModules.qml",
    "screens/ModuleListScreen.qml"):
    if path not in ui_cmake:
        errors.append(f"src/ui/CMakeLists.txt: {path} is not a compiled QML resource")

main = read("src/ui/Main.qml")
for token in ("compactNavigation", "NavigationRail", "NavigationDrawer",
              "NavigationHost", "NoAccessibleModules", "requestShutdown"):
    if token not in main:
        errors.append(f"src/ui/Main.qml: missing {token}")
if "Qt.quit" in main:
    errors.append("src/ui/Main.qml: direct Qt.quit is prohibited")

data_list = read("src/ui/common/DataList.qml")
for token in ("refreshRequested", "nextPageRequested", "rowActivated",
              "sortRequested", "filterRequested", "emptyMessage",
              "errorMessage", "loading", "hasMore", "reuseItems"):
    if token not in data_list:
        errors.append(f"src/ui/common/DataList.qml: missing {token}")
if re.search(r"\.(?:sort|filter)\s*\(", data_list):
    errors.append("src/ui/common/DataList.qml: QML must not sort/filter authoritative rows")

require("src/shell/list_bridge.cpp", [
    "StaleGeneration", "kMaximumFilterBytes", "kMaximumPageRows", "cancel"])

if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("Navigation/list policy: typed routes, queued Qt model and bounded list UI passed")
