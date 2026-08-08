# Phase 7.8 primary pages gate

Status: the UI/resource layer and the authenticated primary-query policy are
portable-verified. Final Qt attachment plus module-specific command providers
remain before Phase 7.8 can be marked production complete. Linux Qt 6.11.1,
QWindowKit, and Windows MSVC execution also remain mandatory CI evidence because
those runtimes are not installed in the local sandbox.

## Delivered

- Qt Quick Controls `FluentWinUI3` as the centralized native base style.
- Qualified supplied FluentControls packaging and runtime import path.
- SquiFlow-owned semantic color, point-typography, spacing, radius, motion,
  high-contrast, and reduced-motion facade.
- Material 3 and macOS reference-kit provenance, hashes, timestamps, and
  reviewed preview assets without committing the opaque source archives.
- FluentControls InfoBar, dialog, progress, focus, scrollbar, and icon reuse
  behind SquiFlow-owned application components.
- Dedicated party, catalog, pricing, order, counter-sale, and receivables
  workspaces with initial, loading, empty, populated, validation, error,
  offline/permission, pending, confirmation, and unknown-outcome states.
- C++-formatted exact-money presentation; QML does not choose prices or compute
  order, cart, invoice, allocation, or balance totals.
- Native QWindowKit shell adapter with Mica, Acrylic fallback, dark-mode, and
  high-contrast fallback behavior isolated from module pages.
- Missing authenticated dashboard/list providers now fail visibly rather than
  inventing an empty snapshot or leaving an endless loading state.
- Specialized primary routes plus a separate write-authorized counter-sale
  route, with updated portable navigation coverage.
- A bounded application query policy that validates activation, typed read
  rights, paging, approved sort/filter fields, provider output, stable IDs, and
  duplicate records before presentation data reaches Qt.
- A real local-database query adapter for parties, catalog, pricing, orders,
  and receivables. It reads through `Database::read(...)`, applies bounded
  storage-side paging/filtering, preserves integer minor-unit prices, and fails
  visibly when storage is unavailable.

## Supplied-resource evidence

`tools/sandbox/check_phase78_design_policy.py` verifies:

- both supplied Figma source pins;
- FluentControls reuse and release staging;
- no first-party `font.pixelSize` or page-local raw colors;
- no page imports more than one extra component library;
- explicit state models on first-party interactive wrappers;
- all Phase 7.8 module pages and routes;
- authoritative exact-money and unknown-outcome presentation;
- native-window, high-contrast, reduced-motion, and fail-closed provider policy.

## Portable evidence

Recorded on 2026-08-06:

- `make -f tools/sandbox/Makefile check`: exit **0**;
- integrity: **444 files**, all passed;
- header self-containment: **196 headers**, zero failures;
- architecture: application, composition, module, server-provider, extension,
  QML, UI-resource, Qt-bridge, navigation/list, Phase 7.7, and Phase 7.8 policy
  gates passed;
- Phase 7.8 policy: **46 QML files** passed;
- navigation manifest: **96 checks, 0 failed**;
- shell accessibility/state: **14 checks, 0 failed**;
- primary-page authorization and snapshot policy: **9 checks, 0 failed**;
- local primary-database query provider: **8 checks, 0 failed**;
- all portable platform, application, shell, workflow, and twelve module test
  programs passed; the largest focused suites included **1,691**, **214**, and
  **189** checks with zero failures.

Evidence log: `/data/phase78-local-query-check.log` in the implementation environment.

## Runtime/release evidence required from CI

The configured Linux Qt and Windows MSVC workflows must still prove:

- Qt 6.11.1 QML compilation and offscreen construction;
- supplied FluentControls font/assets in the staged bundle;
- QWindowKit Core+Quick compilation and Mica/Acrylic fallback behavior;
- keyboard/focus behavior, long translations, 200% text, high contrast, and
  reduced motion;
- authenticated query/command provider attachment after identity startup;
- Windows staged launch, archive creation, and checksum generation.

Absence of an authenticated provider is a deliberate visible error state, not
sample data and not an implicit grant. A release must not waive that CI gate.
