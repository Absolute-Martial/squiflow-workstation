# Phase 7 -- The interface: 7.1, 7.3-7.6 implementation plan

Status: 7.3 implemented with portable/static gates passing and the Qt runtime
gate pending; 7.1 and 7.4-7.6 remain planned. 7.2 has its own
document: `phase-7.2-navigation-and-activation.md`. Read that document
first -- 7.3 onward depend on its typed contribution contract and
immutable navigation snapshot, and this document does not repeat those
definitions.

## Shared ground rules for all of 7.1-7.6

These come from the already-committed QML presentation-bridge decision and
apply to every sub-phase below without restatement:

- Qt types terminate in `src/shell`. No `#include <Q...>`, `QObject`,
  `Q_OBJECT`, `Q_PROPERTY`, or any Qt type appears in domain, engine,
  workflows, protocol, or module code.
- Domain entities (`Quotation`, `Order`, `Party`, ...) are never exposed to
  QML directly. A Qt adapter under `src/shell` maps presentation-safe
  values only.
- Money is minor-unit integers or formatted strings, never floating point,
  at any point past the domain boundary.
- All model/property mutation happens on the GUI thread; worker-originated
  updates cross via `Qt::QueuedConnection` or `QMetaObject::invokeMethod`,
  guarded by the generation/revision pattern already used in
  `CompletionGate` and the paged list model.
- QML never imports `SquiFlow.Domain`, `SquiFlow.Engine`, or
  `SquiFlow.Modules`.
- Theme, style, and `qtquickcontrols2.conf` are centralized in `src/ui`;
  no screen picks its own style.
- Verification follows the same two-lane split as every other phase:
  portable behavior compiled and tested on this sandbox's GCC 11.5 lane;
  Qt-capable behavior written and registered for the Linux/MSVC Qt lanes,
  honestly marked `[~]` here until that lane actually runs.

## 7.1 -- Window and shell

**Goal:** the one root window, its lifecycle wiring to Startup steps 11/12
(from 6.8), and the outermost QML shell that 7.2's navigation rail mounts
into.

**Scope:**

- `src/shell/qml_surface_qt.hpp/.cpp` (already exists from the bridge work)
  gains the actual root `ApplicationWindow` QML component load, replacing
  any placeholder root used during the bridge sub-phases.
- `src/ui/Main.qml`: top-level `ApplicationWindow`, hosting a `StackView`
  or `SwipeView` region for the currently selected destination (populated
  by 7.2/7.3), window title bound to the active session/tenant, and a
  status region for offline/online state (from 6.6).
- Window close intent (`onClosing`) is intercepted and routed to
  `StartupSequence::shutdown(ShutdownReason::WindowClosed)` -- never
  `Qt.quit()`, matching the already-committed rule.
- Minimum window size, default size, and state restoration (maximized /
  normal / position) persisted through the 6.1 paths cache location, not a
  Qt-native settings file directly (so it obeys the machine-wide vs
  per-account split already decided in ADR 0004).

**Files:** `src/ui/Main.qml`, `src/shell/qml_surface_qt.{hpp,cpp}`
(extended), `src/shell/window_state.hpp/.cpp` (persisted geometry, behind a
fake for the portable lane), `tests/shell/window_state_test.cpp`.

**Invariants:**

1. Root window construction failure maps to a `PresentationError` and
   triggers reverse rollback exactly as already specified for step 12.
2. Window close always goes through lifecycle shutdown; there is no code
   path that calls `Qt.quit()` or `QGuiApplication::quit()` directly.
3. Persisted window geometry is validated before use -- an off-screen or
   zero-sized restored geometry falls back to the default, never crashes.

**Tests:** normal open/close, root creation failure and rollback, closing
intent routes to shutdown (fake lifecycle sink asserts the call), malformed
persisted geometry falls back safely, geometry persists across a
simulated restart in the fake.

## 7.3 -- Lists

**Goal:** the one reusable bounded list pattern every module screen uses,
built on the paging and Qt model work already done in the bridge
sub-phases (`paged_list_cache.*`, `paged_list_model_qt.*`).

**Scope:**

- Confirm `PagedListCache`/`PagedListModelQt` are generic enough for
  arbitrary module row shapes, or add a thin per-module row-mapping layer
  under `src/shell/modules/<module>/list_rows.hpp` if not -- do not
  duplicate the paging mechanism per module.
- `src/ui/common/DataList.qml` (already exists) gains: column/field
  templates driven by roles, empty-state, loading-state, error-state, and
  pull-to-refresh-equivalent (a refresh action) presentation.
- Row selection emits a stable-id-based command through the presentation
  bridge, never a domain pointer.
- Sorting and filtering are requested through the bridge as intents
  ("sort by X", "filter text Y") and satisfied by re-querying the module's
  service layer; QML never sorts or filters loaded rows itself, so results
  stay authoritative and paginated correctly.

**Files:** `src/shell/list_bridge.hpp/.cpp` (per-list bridge base type),
one instantiation per module list screen, `src/ui/common/DataList.qml`
(extended), `tests/shell/list_bridge_test.cpp`.

**Invariants:**

1. A list bridge never holds a live domain pointer past the call that
   produced the current page; each page is an owned, independent snapshot
   (this is the PMR page-arena rule already committed).
2. Selection, sort, and filter intents are validated against the
   originating module's actual columns/fields before being sent to the
   service layer -- an unknown field name is refused, not silently
   ignored.
3. List row count and page boundaries always match what the underlying
   query actually returned; the model never fabricates rows to fill a page.

**Tests:** empty list, single page, multiple pages, boundary page size,
malformed sort/filter field, selection of a stale/removed row id, refresh
during an in-flight page load, concurrent refresh requests, large row
count (the stated capacity ceiling), rollback of an in-flight load on
window close.

## 7.4 -- Forms and validation

**Goal:** the one shared create/edit form pattern, presenting typed field
errors from domain validation without ever letting QML re-derive business
rules.

**Scope:**

- `src/shell/form_bridge.hpp/.cpp`: a presentation bridge type that holds
  field values as presentation-safe types (`QString`/int/bool), submits
  them as a request to the owning module's create/update operation, and
  surfaces the resulting `Result<T, DomainError>` as a per-field or
  form-level error list.
- Field-level validation shown before submission (required, format, range)
  mirrors -- never replaces -- the domain's own validation; the domain
  check on submit is always authoritative. QML-side pre-checks exist only
  to reduce round trips, and every one has a server-truth equivalent.
- Money fields enter and display as formatted strings backed by minor-unit
  integers; no QML `TextField` ever binds directly to a floating-point
  money value.
- Optimistic-vs-confirmed state: a submitted form is disabled/pending until
  the domain call resolves; a rejected submission restores the form to its
  prior editable state with the returned field errors attached.

**Files:** `src/shell/form_bridge.hpp/.cpp`, `src/shell/domain_error_
presentation.hpp/.cpp` (maps `DomainError` codes from the app contracts
layer to presentation-safe field/form messages), `src/ui/common/Form.qml`,
`tests/shell/form_bridge_test.cpp`.

**Invariants:**

1. A form bridge never submits with a `RequestContext` it did not receive
   from the current session snapshot -- no reconstructing permissions
   client-side.
2. Every `DomainError` code from `app/contracts/domain_error.hpp` has an
   explicit presentation mapping; an unmapped code is a build-time or
   test-time failure, never a silent generic message.
3. A form cannot be submitted twice concurrently; a second submit while
   one is pending is refused at the bridge, not just visually disabled.

**Tests:** normal create, normal edit, validation failure per field,
server-side rejection after client-side validation passed (race with
another user), double-submit attempt, cancel during a pending submit,
unauthorized submission (right revoked between form open and submit),
malformed numeric/date input, every `DomainError` code maps to a message.

## 7.5 -- Documents and print via `QPdfWriter`, avoiding QtWidgets

**Goal:** render a Phase 5.8 `PreparedDocument` to a print-ready PDF using
only `QPdfWriter`/`QPainter`/`QTextDocument`'s device-agnostic drawing path
-- never linking QtWidgets, since this is a QML-only application and
pulling in QtWidgets would be a real, avoidable dependency-weight and
packaging cost.

**Spike required first (recorded in `todo.md`):** confirm on the actual
Windows/Qt 6.11.1 build that printing through `QPdfWriter` without
QtWidgets actually renders acceptable text/table layout for an invoice or
quotation document. This cannot be verified on this sandbox (no Qt). If
the spike finds `QPdfWriter` insufficient for layout fidelity, the fallback
is `QTextDocument::print()` targeting a `QPdfWriter` as the paint device,
which also avoids QtWidgets; that fallback should be tried before
reconsidering the "avoid QtWidgets" constraint itself.

**Scope:**

- `src/shell/document_render_qt.hpp/.cpp`: takes a `PreparedDocument`
  snapshot (presentation-mapped, not the domain type) and a template
  identifier, and produces PDF bytes via `QPdfWriter`.
- One template per document kind to start: quotation, invoice, statement --
  driven by the actual fields Phase 4/5 already produce, not invented
  fields.
- Output path: either a save-to-file flow (native file dialog via
  `Qt.labs.platform` or `QML FileDialog`) or a direct-to-printer flow via
  `QPrinterInfo`-free enumeration if available in a QtWidgets-free form;
  if Qt 6.11.1 requires QtWidgets for printer enumeration, the spike
  above must confirm this and the plan revised rather than silently
  linking QtWidgets.
- No sending. Printing/saving a PDF is unrelated to the 5.8/8.7
  approval-and-send path; a prepared, approved document may be printed any
  number of times without affecting its send-intent state.

**Files:** `src/shell/document_render_qt.hpp/.cpp`, one template resource
per document kind under `src/ui/documents/`, `tests/shell/document_
render_qt_test.cpp` (Qt-capable lane only, since it needs `QPdfWriter`).

**Invariants:**

1. Rendering only ever reads a `PreparedDocument` snapshot; it never
   queries the live mutable record, so a printed document always matches
   what was actually approved.
2. Money and quantity fields render exactly as formatted by the domain's
   own formatting (from Phase 2.2/2.3), never re-formatted independently
   by the rendering code.
3. A render failure (bad template, missing field) produces a named
   `PresentationError`, never a partially-written PDF file left on disk.

**Tests (Qt-capable lane):** normal render per document kind, missing
template, malformed/missing required field, very long content (multi-page
overflow), zero-line document (e.g. an invoice with no lines -- must be
refused upstream by 5.8, but the renderer must also refuse gracefully if it
ever receives one), concurrent render requests, save-path failure
(permission denied / disk full) leaves no partial file.

## 7.6 -- Images and the AVIF plugin

**Goal:** display design-file thumbnails and previews (Phase 4.13 `files`
module) including AVIF, without requiring a server round trip for every
thumbnail.

**Spike required first (recorded in `todo.md`):** verify an AVIF Qt image
plugin actually loads against the pinned Qt 6.11.1 build. The supplied
`qtimageformats` archive is the expected source of this plugin; confirm it
builds and registers before writing any code that assumes AVIF "just
works".

**Scope:**

- `src/shell/image_provider_qt.hpp/.cpp`: a `QQuickImageProvider`
  registered on the QML engine at step 11, serving thumbnails by stable
  file-record id, never a raw filesystem path from QML.
- Thumbnail cache under the 6.1 per-account cache root (disposable,
  rebuildable), keyed by file id + content hash from Phase 4.13, so a
  changed file (new hash) never serves a stale thumbnail.
- Graceful fallback for an unsupported or corrupt image: a named
  placeholder, never a crash or a blank `Image` with no error signal.
- AVIF is one format among the formats `files` already indexes; this
  sub-phase does not change what `files` indexes, only how the shell
  previews it.

**Files:** `src/shell/image_provider_qt.hpp/.cpp`,
`src/shell/thumbnail_cache.hpp/.cpp` (portable logic, behind a fake for the
non-Qt lane), `tests/shell/thumbnail_cache_test.cpp`,
`tests/shell/image_provider_qt_test.cpp` (Qt-capable lane).

**Invariants:**

1. The image provider only ever resolves a request by the file record's
   stable id + content hash; it never accepts or trusts a raw path from
   QML.
2. A cache entry is invalidated by content-hash change, never by mtime
   alone -- matching the 4.13 module's own identity rule (device, volume,
   file id, content hash).
3. Cache size is bounded with the same kind of hard cap discipline as the
   6.2 logging budget; the oldest entries are evicted first, never an
   unbounded cache filling the account's disk.

**Tests:** normal AVIF load, normal non-AVIF (e.g. PNG/JPEG) load,
corrupt/truncated image file, unsupported format, cache hit vs miss,
cache invalidation on content-hash change, cache eviction at the size
cap, concurrent thumbnail requests for the same file id, missing file
record id, plugin-not-loaded fallback (portable lane simulates this via
the fake).

## Cross-cutting sequence for 7.1-7.6

| Order | Sub-phase | Depends on |
| --- | --- | --- |
| 1 | 7.1 Window and shell | 6.8 (startup steps 11/12), bridge work already done |
| 2 | 7.2 Navigation (separate document) | 7.1 |
| 3 | 7.3 Lists | 7.2's navigation snapshot and stable ids |
| 4 | 7.4 Forms | 7.3's selection pattern, `app/contracts` result/error types |
| 5 | 7.5 Documents/print | 5.8 prepared documents, 7.4's presentation-mapping conventions |
| 6 | 7.6 Images/AVIF | 4.13 files module; independent of 7.3-7.5 otherwise, can run in parallel with 7.5 |

Each sub-phase above closes with: focused tests passing, full strict gate,
independent CMake gate, Qt-capable lane registered (even if it cannot run
here), a gate document under `docs/qa/`, and its own commit before the
next sub-phase starts -- following the same discipline as every prior
phase.

## Acceptance criteria for closing Phase 7

Phase 7 is complete only when every one of 7.1-7.6 (including the
already-planned 7.2) has its own gate document, and `docs/plan/todo.md`'s
Phase 7 row moves from `0` to `6` of `6` sub-phases done -- with the two
required spikes (7.5 print, 7.6 AVIF) resolved on the real machine, not
assumed.
