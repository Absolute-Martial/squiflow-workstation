#!/usr/bin/env python3
"""Phase 7.8 supplied-resource reuse and first-party QML design policy."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
errors: list[str] = []


def read(path: str) -> str:
    candidate = ROOT / path
    if not candidate.exists():
        errors.append(f"{path}: missing")
        return ""
    return candidate.read_text(encoding="utf-8")


pin = read("docs/design/reference-assets/SQUIFLOW_SOURCE_PIN")
for value in (
    "8dd30e1099f7cf067f2e14279d6d5a4dd159d2f2a96a6a5534b6788b7e86fff0",
    "0f86f22d158ff22cea4d95459ef50d959fa1d31cf80837dff242e246638ed604",
):
    if value not in pin:
        errors.append("Figma reference source pin is incomplete")

policy = read("docs/design/phase-7.8-design-source-policy.md")
for token in ("FluentWinUI3", "FluentControls", "Material 3", "macOS 27",
              "No first-party QML file imports two extra"):
    if token not in policy:
        errors.append(f"design source policy: missing {token}")

qml_files = sorted((ROOT / "src/ui").rglob("*.qml"))
extra_import = re.compile(r"^import\s+(FluentControls|FluentUI|RinUI)", re.MULTILINE)
raw_color = re.compile(r'#[0-9a-fA-F]{6,8}')
for path in qml_files:
    relative = path.relative_to(ROOT)
    text = path.read_text(encoding="utf-8")
    if "font.pixelSize" in text:
        errors.append(f"{relative}: pixel typography is prohibited")
    extras = set(extra_import.findall(text))
    if len(extras) > 1:
        errors.append(f"{relative}: mixes extra UI libraries: {sorted(extras)}")
    if relative.as_posix() != "src/ui/Theme.qml" and raw_color.search(text):
        errors.append(f"{relative}: raw colors must come from Theme")

for path in (
    "src/ui/common/StatusBanner.qml",
    "src/ui/common/ConfirmDialog.qml",
    "src/ui/common/UnsavedChangesDialog.qml",
    "src/ui/common/CommandBar.qml",
    "src/ui/common/DataList.qml",
    "src/ui/common/Form.qml",
    "src/ui/navigation/NavigationRail.qml",
    "src/ui/navigation/NavigationDrawer.qml",
):
    if "import FluentControls" not in read(path):
        errors.append(f"{path}: does not reuse qualified FluentControls")

for path in ("src/ui/common/MetricCard.qml", "src/ui/common/LoadingSkeleton.qml",
             "src/ui/common/DataList.qml", "src/ui/common/Form.qml"):
    text = read(path)
    if "states:" not in text:
        errors.append(f"{path}: explicit state model missing")

for path in (
    "src/ui/parties/PartiesPage.qml",
    "src/ui/parties/PartyDetailPage.qml",
    "src/ui/parties/PartyFormPage.qml",
    "src/ui/parties/ContactEditor.qml",
    "src/ui/catalog/CatalogPage.qml",
    "src/ui/catalog/ProductDetailPage.qml",
    "src/ui/catalog/ProductFormPage.qml",
    "src/ui/pricing/PricingPage.qml",
    "src/ui/pricing/RateDetailPage.qml",
    "src/ui/pricing/RateFormPage.qml",
    "src/ui/pricing/PriceResolutionPanel.qml",
    "src/ui/orders/OrdersPage.qml",
    "src/ui/orders/OrderDetailPage.qml",
    "src/ui/orders/OrderEditorPage.qml",
    "src/ui/orders/OrderLineEditor.qml",
    "src/ui/orders/PriceEvidenceBadge.qml",
    "src/ui/counter/CounterSalePage.qml",
    "src/ui/counter/CartLine.qml",
    "src/ui/counter/PaymentPanel.qml",
    "src/ui/receivables/ReceivablesPage.qml",
    "src/ui/receivables/InvoiceDetailPage.qml",
    "src/ui/receivables/PaymentAllocationPage.qml",
    "src/ui/receivables/StatementPanel.qml",
    "src/ui/receivables/DocumentDeliveryPanel.qml",
):
    read(path)

manifest = read("src/shell/navigation_manifest.cpp")
if "qrc:/qt/qml/SquiFlow/parties/PartiesPage.qml" not in manifest:
    errors.append("navigation: party workspace is not routed")
if "qrc:/qt/qml/SquiFlow/catalog/CatalogPage.qml" not in manifest:
    errors.append("navigation: catalog workspace is not routed")
if "qrc:/qt/qml/SquiFlow/pricing/PricingPage.qml" not in manifest:
    errors.append("navigation: pricing workspace is not routed")
if "qrc:/qt/qml/SquiFlow/orders/OrdersPage.qml" not in manifest:
    errors.append("navigation: orders workspace is not routed")
if "qrc:/qt/qml/SquiFlow/counter/CounterSalePage.qml" not in manifest:
    errors.append("navigation: counter-sale workspace is not routed")
if "qrc:/qt/qml/SquiFlow/receivables/ReceivablesPage.qml" not in manifest:
    errors.append("navigation: receivables workspace is not routed")

pricing_qml = read("src/ui/pricing/RateDetailPage.qml")
if "amountText" not in pricing_qml or "amount_minor" in pricing_qml:
    errors.append("pricing UI must render C++-formatted exact money without calculating")
orders_qml = read("src/ui/orders/OrderDetailPage.qml") + read("src/ui/orders/OrderLineEditor.qml")
if "amountText" not in orders_qml or "quantity *" in orders_qml:
    errors.append("orders UI must consume authoritative amounts without calculating")
if "unknownOutcome" not in orders_qml:
    errors.append("orders UI must present unknown command outcomes safely")
counter_qml = read("src/ui/counter/CounterSalePage.qml") + read("src/ui/counter/PaymentPanel.qml")
if "amountText" in counter_qml or "totalText" not in counter_qml:
    errors.append("counter sale UI must consume authoritative totalText")
if "unknownOutcome" not in counter_qml or "Do not submit again" not in counter_qml:
    errors.append("counter sale UI must recover visibly from an unknown result")
receivables_qml = read("src/ui/receivables/InvoiceDetailPage.qml")
if "totalText" not in receivables_qml or "balanceText" not in receivables_qml:
    errors.append("receivables UI must consume authoritative exact-money text")
if "unknownOutcome" not in receivables_qml:
    errors.append("receivables UI must present unknown payment/delivery outcomes")

cmake = read("CMakeLists.txt")
if "SQUIFLOW_WITH_UI_FLUENT" not in cmake or "QML_IMPORT_PATH" not in cmake:
    errors.append("CMake: FluentControls compile import path missing")
staging = read("packaging/stage_windows.cmake")
if "external/ui-fluent/fluentcontrols" not in staging:
    errors.append("release staging: FluentControls module missing")

main_qml = read("src/ui/Main.qml")
for token in ("FC.Icon", "highContrast", "reducedMotion", "nativeWindowBridge"):
    if token not in main_qml:
        errors.append(f"native shell/accessibility closure: missing {token}")
native = read("src/shell/native_window_bridge_qt.cpp")
for token in ("QuickWindowAgent", 'QStringLiteral("mica")',
              'QStringLiteral("acrylic-material")', "high_contrast"):
    if token not in native:
        errors.append(f"native window adapter: missing {token}")
shell_state = read("src/shell/shell_state_qt.hpp")
for token in ("highContrast", "reducedMotion", "accessibilityChanged"):
    if token not in shell_state:
        errors.append(f"shell accessibility state: missing {token}")
dashboard_adapter = read("src/shell/dashboard_bridge_qt.cpp")
if "provider_unavailable" not in dashboard_adapter or "DashboardSnapshot empty" in dashboard_adapter:
    errors.append("dashboard adapter must fail closed without an authenticated provider")
list_adapter = read("src/shell/list_screen_bridge_qt.cpp")
if "list.error.provider_unavailable" not in list_adapter:
    errors.append("list adapter must fail closed without an authenticated provider")

if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"Phase 7.8 design policy: {len(qml_files)} QML files passed supplied-resource reuse and token rules")
