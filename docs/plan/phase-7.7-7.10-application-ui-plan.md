# Phase 7.7-7.10 -- native application UI implementation plan

Status: Phase 7.7 implemented in the portable/static lane with Qt runtime
verification delegated to the configured Linux Qt and Windows MSVC jobs;
Phases 7.8-7.10 remain planned. Phases 7.1-7.6 provide the window, navigation,
lists, forms, PDF, image-preview, and Fluent foundations.

## Outcome

At the end of 7.10, a signed-in user can operate every enabled workstation
module through native Qt Quick pages without encountering placeholder routes.
Every visible action is permission-aware, invokes an application operation or
query through a presentation bridge, reports pending/success/error state, and
refreshes from authoritative data.

## Rules for all four phases

- QML renders state and emits intent. It never queries storage, calls a module
  service, calculates business totals, evaluates permissions, or constructs a
  `RequestContext`.
- Domain entities never cross into QML. Thin adapters in
  `src/shell/modules/<module>/` publish owned presentation snapshots.
- Stable record ids are the only navigation and selection identity. No QML
  object keeps a domain pointer or a raw filesystem path.
- Money remains formatted text plus exact minor-unit integers at the bridge;
  QML never uses floating point for money.
- A command is disabled while its request is pending. Repeated activation is
  also rejected by the bridge, not only by the button.
- Success never means “the button was clicked.” The screen refreshes from the
  operation result or a new authoritative query.
- Offline, unauthorized, validation, conflict, missing, loading, empty, and
  stale states are first-class page states.
- All worker completions cross to the GUI thread and carry a generation. A
  completion from a previous route, account, or refresh is discarded.
- Layouts support compact (640-899 px), standard (900-1279 px), and wide
  (1280+ px) windows. Compact mode never removes an action; it moves it into an
  overflow menu or drawer.
- Every interactive control has an accessible name, predictable tab order,
  visible focus, keyboard activation, and a non-color-only status cue.
- User-visible text uses `qsTr()` or a translation key. Dates, numbers, and
  currency use the already-selected account locale.
- Use Qt's `FluentWinUI3` controls first, FluentControls only for missing Fluent
  patterns, FluentUI only for a unique justified capability, and Rin-UI only as
  a final gap-fill. Never import two extras libraries in one QML file.
- Theme detection comes from Qt's `colorScheme` signal; no polling thread and
  no per-page theme detector.
- New generic abstractions require at least two real consumers. Otherwise, add
  a small module-specific mapper instead of a speculative framework.

---

## 7.7 -- design system, application shell, and dashboard

### 7.7 implementation status

Portable implementation and focused tests are complete. Qt 6.11.1/Linux and
MSVC workflows, QWindowKit Core+Quick build, and staged offscreen release smoke
tests are configured. Full closure waits only for those external CI jobs to
produce runtime evidence; visual/accessibility/performance evidence remains in
7.10. See `docs/qa/phase-7.7-dashboard-gate.md`.

### Goal

Turn the existing root window into the real workstation home: consistent page
chrome, a useful dashboard, global feedback, quick actions, and responsive
navigation.

### 7.7.1 -- central visual tokens and reusable patterns

Extend the existing theme rather than creating a second theme system.

**QML components**

- `src/ui/common/PageScaffold.qml`: page title, optional subtitle,
  breadcrumbs, command area, body, and responsive margins.
- `src/ui/common/CommandBar.qml`: primary action, secondary actions, overflow,
  and pending state.
- `src/ui/common/StatusBanner.qml`: offline, warning, error, success, and
  permission states; use FluentControls `InfoBar` only in the opt-in Fluent
  build.
- `src/ui/common/MetricCard.qml`: value, label, comparison/status, activation,
  skeleton state, and accessible summary.
- `src/ui/common/SectionCard.qml`, `EmptyState.qml`, `ErrorState.qml`,
  `LoadingSkeleton.qml`, `ConfirmDialog.qml`, and `UnsavedChangesDialog.qml`.
- Extend `Theme.qml` with semantic tokens only: surfaces, borders, focus,
  positive/warning/error, compact/standard spacing, radius, and motion timing.

**Do not add:** a second component framework, a CSS-like QML abstraction, or a
custom layout engine.

### 7.7.2 -- shell interactions

Refine `Main.qml` and navigation components with:

- tenant/user identity, current route title, back/forward, global search entry,
  connectivity/sync status, notifications, theme choice, and settings menu;
- command palette entry point with keyboard shortcut;
- unsaved-change interception before route change, account change, or shutdown;
- global toast/InfoBar queue with a hard maximum and deduplication key;
- compact drawer, standard rail, and wide expanded navigation behavior;
- route restoration only when the restored route is still enabled and allowed.

Portable ownership belongs in `src/shell/shell_state.hpp/.cpp` and
`notification_queue.hpp/.cpp`; Qt adapters remain in `src/shell/*_qt.*`.

### 7.7.3 -- dashboard read model and bridge

Add an application-owned read contract rather than letting the dashboard call
12 modules independently from QML.

**Files**

- `src/app/dashboard/dashboard_query.hpp`: `DashboardQueryPort` and immutable
  `DashboardSnapshot`.
- `src/app/dashboard/dashboard_service.hpp/.cpp`: composes existing module
  query operations under one received `RequestContext`; no new business rules.
- `src/shell/dashboard_bridge.hpp/.cpp`: refresh generation, pending/error
  state, permission-filtered quick actions, and presentation mapping.
- `src/shell/dashboard_model_qt.hpp/.cpp`: Qt properties/list models only.
- `src/ui/dashboard/DashboardPage.qml` plus focused dashboard sections.
- `tests/app/dashboard_service_test.cpp` and
  `tests/shell/dashboard_bridge_test.cpp`.

**Snapshot content**

- receivables due/overdue totals already calculated by the domain;
- open orders and jobs counts;
- quotations awaiting action and agreements nearing a meaningful date;
- unresolved tasks/reminders;
- recent authoritative activity and recent files;
- permission-filtered quick actions for customer, order, quotation, payment,
  purchase, task, and file workflows.

No metric is invented in QML. Missing permission removes a card/action rather
than showing a control that will always fail.

### 7.7 invariants

1. One refresh produces one immutable dashboard snapshot and generation.
2. A slow prior refresh cannot replace a newer refresh or another account's
   dashboard.
3. Dashboard cards navigate with stable route/record ids only.
4. A quick action is visible only when the current activation and rights allow
   its operation; the operation still checks rights authoritatively.
5. Dashboard failure does not prevent navigation to independently available
   modules.

### 7.7 tests and gate

Test empty/new account, representative populated account, partial module
activation, no rights, offline cache, refresh race, account switch during
refresh, card activation, quick-action success/failure, compact/standard/wide
layout, keyboard traversal, light/dark/high-contrast behavior, and bounded
notification overflow.

Close with focused portable tests, QML static checks, full strict gate,
independent CMake/CTest, Qt offscreen component construction, screenshots at
three breakpoints and two themes, and `docs/qa/phase-7.7-dashboard-gate.md`.

---

## 7.8 -- master-data and primary commercial pages

### Goal

Deliver complete list/detail/create/edit interactions for the foundational
records and the highest-frequency sales/receivables work.

### Shared screen shape

Each module gets thin bridge/mapping files under
`src/shell/modules/<module>/`, and QML under `src/ui/modules/<module>/`:

- `<Module>ListPage.qml`: authoritative paging, sort, filter, refresh, selection;
- `<Record>DetailPage.qml`: owned snapshot, actions, related-record links,
  history/evidence region;
- `<Record>FormPage.qml`: shared `Form.qml`, create/edit mode, dirty guard,
  domain errors;
- a module bridge that validates route parameters and actions before invoking
  application ports.

Do not introduce one universal “record page.” Fields and commands are
module-specific; only list/form/page-state mechanics are shared.

### 7.8.1 -- parties

Pages for customers/suppliers, contacts, addresses, notes, status, and related
commercial activity. Interactions: create/edit party, add/edit contact,
activate/deactivate where the domain permits, open invoices/orders/quotations,
and start a new document with the party preselected.

### 7.8.2 -- catalog

Product/service list, detail, create/edit, aliases, active state, and related
price history. Preserve product identity; changing a label never changes the
stable record id.

### 7.8.3 -- pricing

Price lists/rules, effective periods, exact minor-unit amounts, conflicts, and
read-only explanation of the authoritative effective price. QML never chooses
which price wins.

### 7.8.4 -- orders and counter sales

Order list/detail/editor, line selection, quantity changes, authoritative
repricing, issue/confirm/cancel actions, and evidence. Counter sale gets a
keyboard-first entry surface with barcode/search entry, bounded cart lines,
exact totals from the domain, payment confirmation, and idempotent submit.

### 7.8.5 -- receivables

Invoice list/detail, payment recording, customer balance, statement preparation,
PDF save/print, and delivery preparation. Draft/issued/cancelled state controls
which actions are offered, but domain lifecycle checks remain authoritative.

### 7.8 invariants

1. Detail pages reload or reconcile after every mutation; they do not patch
   domain truth optimistically.
2. Unsaved forms cannot be discarded by navigation without an explicit choice.
3. Document line totals, price selection, numbering, lifecycle, and balance are
   never calculated in QML.
4. Destructive/final actions require a reason or confirmation when the domain
   operation requires one, and cannot be double-submitted.
5. Related links are permission-filtered and carry stable ids.

### 7.8 tests and gate

For every module: empty/list paging, create, edit, validation, stale record,
concurrent conflict, rights revoked while open, offline-allowed and
online-required actions, cancel/dirty navigation, keyboard-only completion,
and compact layout. Orders/receivables additionally test exact totals,
numbering transitions, idempotent final actions, PDF failure, and no partial
state after rejection.

Implement and commit one module at a time in this order: parties, catalog,
pricing, orders/counter sales, receivables. Run that module's focused tests and
the full strict gate before starting the next. Close with
`docs/qa/phase-7.8-primary-pages-gate.md`.

---

## 7.9 -- remaining operational and supporting module pages

### Goal

Remove every remaining placeholder route and make all twelve compiled modules
operable through the native application.

### 7.9.1 -- quotations

Quotation list, draft editor, revision history, issue, revise, accept, expire,
PDF, and related party/agreement links. Issued revisions are immutable in the
UI because they are immutable in the domain.

### 7.9.2 -- agreements

Agreement list/detail/editor, rates/caps/periods, activate, close, reopen, amend,
and supersession chain. The UI displays the effective state and consumption
returned by the domain; it does not recalculate them.

### 7.9.3 -- jobs

Job list, detail, assignments, milestones/status, related order/party/files,
and allowed state actions. Long-running work exposes progress without creating
an unbounded polling loop.

### 7.9.4 -- sourcing

Supplier profile, purchase history, purchase recording, outstanding debt, and
settlement. Settlement confirmation shows exact authoritative amount and
cannot be applied twice.

### 7.9.5 -- companion

Task list/board, task editor, due/snooze/complete interactions, recurrence
summary, attention view, and calendar view. Recurrence calculation stays in the
domain; the UI displays the next occurrence returned to it.

### 7.9.6 -- files

File search/list, thumbnail grid, preview/detail, links to domain records,
version lineage, missing/offline-volume status, and forget action with reason.
QML uses `stableFileId`; trusted paths stay behind `FilePreviewSource`.

### 7.9.7 -- administration and settings

Activation/module visibility, users/roles/rights, account settings, numbering,
connection/sync diagnostics, appearance/language, cache management, logs/crash
report location, and about/version. Secrets are never displayed or copied into
QML. Rights editing must prevent removal of the last viable administrator if
that domain rule exists.

### 7.9 invariants

1. Every route in the navigation manifest resolves to a real page or a named
   permission/activation state—never a generic placeholder.
2. Immutable evidence remains read-only regardless of QML state.
3. File pages never receive a raw path from route parameters.
4. Administrative pages expose capability, status, and safe commands—not
   secret material or internal stack traces.
5. Background progress is event/coarse-timer driven and bounded.

### 7.9 tests and gate

Apply the same list/detail/form/action matrix as 7.8 plus revision-chain,
agreement lifecycle, job progress, sourcing settlement, recurring task, file
plugin-missing, missing volume, lineage, last-admin, activation change, locale
switch, and cache-clear cases. Implement and commit one module at a time in the
order above; run focused and full gates after each. Close with
`docs/qa/phase-7.9-remaining-pages-gate.md`.

---

## 7.10 -- integration, accessibility, performance, and Qt runtime closure

### Goal

Turn the complete page set into a releasable native interface and collect the
real Qt evidence that cannot be produced in the portable sandbox.

### 7.10.1 -- complete navigation and workflow journeys

Exercise cross-module journeys rather than isolated pages:

- customer → quotation → accepted revision → agreement/order;
- counter sale → payment → printable receipt;
- order → invoice → payment → statement;
- supplier → purchase → outstanding debt → settlement;
- task/file links → originating record and back;
- permission/activation change while a page is open;
- offline work → queued state → reconnect/reconcile/conflict.

Back/forward and deep links preserve stable route state but never stale domain
objects.

### 7.10.2 -- accessibility and localization

- keyboard-only completion of every primary journey;
- screen-reader names, roles, descriptions, live error announcements, and focus
  restoration after dialogs/navigation;
- 200% text scaling, high contrast, reduced motion, and non-color status cues;
- translation extraction for all first-party QML/C++ strings;
- layout tests with deliberately long translations and localized dates/money.

### 7.10.3 -- performance and resource budgets

Measure, do not guess:

- cold window-to-dashboard time and route-to-first-content time;
- dashboard query and list paging latency;
- QML object count, binding loops, image memory, thumbnail cache, and PDF peak;
- scrolling with the documented list capacity;
- repeated navigation for leaks and stale bridge/model retention.

Keep a maximum of one live detail/editor stack per active navigation branch;
release module bridges when their route is removed. No hidden page may retain
an unbounded result set.

### 7.10.4 -- real dependency/runtime gates

On the Qt 6.11.1 Windows/Linux runners:

- configure/build all Qt targets and run CTest/offscreen QML tests;
- verify `FluentWinUI3`, theme switching, FluentControls imports, fonts/icons,
  i18n resources, PDF pagination, PNG/JPEG/AVIF decoding, and deployed plugins;
- visually validate three breakpoints, light/dark/high contrast, and text scale;
- vendor `SineStriker/syscmdline`, then build QWindowKit Core+Quick and verify
  frameless resize, title-bar hit testing, maximize/snap, Mica/Acrylic fallback,
  multi-monitor/DPI, and Windows 10 fallback;
- keep FluentUI's Widgets/PrintSupport-dependent C++ plugin disabled unless a
  specifically approved feature justifies that packaging cost.

### 7.10 acceptance gate

Phase 7 closes only when:

- all twelve module routes have real pages and their permitted core actions;
- no production route loads a placeholder page;
- every page has loading, empty, error, offline, and permission behavior;
- all portable, CMake/CTest, Qt offscreen, visual, accessibility, AVIF, PDF,
  and QWindowKit gates pass on supported platforms;
- no critical/high accessibility, data-integrity, security, or lifecycle defect
  remains;
- `docs/qa/phase-7.10-application-ui-release-gate.md` records measurements,
  screenshots, plugin list, and known non-blocking limitations.

---

## Ordered implementation and commit plan

1. `feat(ui): add semantic tokens and shared page states`
2. `feat(shell): add bounded shell notifications and unsaved guards`
3. `feat(app): compose the dashboard read snapshot`
4. `feat(ui): implement the responsive dashboard`
5. `test(ui): gate Phase 7.7 dashboard and shell`
6. Implement 7.8 one module per logical commit/gate: parties, catalog, pricing,
   orders/counter sales, receivables.
7. Implement 7.9 one module per logical commit/gate: quotations, agreements,
   jobs, sourcing, companion, files, administration/settings.
8. `test(ui): add cross-module journey and accessibility gates`
9. `perf(ui): record startup navigation and resource budgets`
10. `build(ui): close Qt Fluent PDF AVIF and QWindowKit runtime gates`
11. `docs(ui): close the Phase 7 application UI release gate`

Never combine unrelated module pages in one commit. After each logical change:
run its focused portable tests, QML/static checks, full strict sandbox gate,
independent CMake/CTest when available, then commit before moving forward.

## Dependency order

- 7.7 depends on 7.1-7.4 and may begin immediately in the portable/static lane.
- 7.8 depends on 7.7 page patterns and existing module operations.
- 7.9 depends on 7.7; individual modules may proceed after their own service
  surfaces are confirmed, but should follow 7.8 to avoid changing shared
  patterns repeatedly.
- 7.10 depends on 7.7-7.9 and requires a real Qt 6.11.1 toolchain.
- The supplied `syscmdline` source resolves QWindowKit's nested host-build
  dependency. Linux Qt 6.11.1 and Windows MSVC now compile Core+Quick; native
  backdrop/chrome visual evidence remains a Phase 7.10 machine gate.
