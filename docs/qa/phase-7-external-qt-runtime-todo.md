# Phase 7 external Qt/MSVC release-closure TODO

**Owner:** agent with Qt 6.11.1, Windows/MSVC, and Linux Qt runner access  
**Scope:** build, runtime verification, evidence capture, and compiler-discovered corrections only  
**Do not weaken or delete a gate to make it pass.**

The portable implementation and source wiring are prepared in the repository. This checklist contains the evidence that cannot be produced in the portable sandbox. Phase 7 must remain open until every required item below has a reproducible result.

## 1. Toolchain and dependency lock

- [ ] Use Qt **6.11.1** on both Windows/MSVC and Linux.
- [ ] Record exact CMake, Ninja, MSVC, Windows SDK, and compiler versions.
- [ ] Configure with `SQUIFLOW_WITH_QT=ON`, `SQUIFLOW_BUILD_TESTS=ON`, `SQUIFLOW_WITH_QWINDOWKIT=ON`, and the documented Fluent option.
- [ ] Verify vendored dependency revisions, including `qwindowkit`, `qmsetup`, and nested `syscmdline`; do not fetch an unpinned branch.
- [ ] Confirm no unapproved Qt Widgets or PrintSupport dependency enters the deployment.

## 2. Clean configure, build, and tests

- [ ] Configure from an empty build directory on Windows/MSVC.
- [ ] Configure from an empty build directory on Linux Qt.
- [ ] Build every target with warnings treated as errors.
- [ ] Run the full CTest suite in both lanes.
- [ ] Run Qt bridge tests and offscreen QML tests with `QT_QPA_PLATFORM=offscreen` where applicable.
- [ ] Run `qmllint` over every first-party QML file and fail on errors, unresolved types, required-property omissions, and unsafe signal handlers.
- [ ] Run the portable sandbox `make -f tools/sandbox/Makefile check` and preserve the zero exit status.
- [ ] If compilation exposes a source defect, fix the defect and add a regression test; never bypass the source or remove it from CMake.

## 3. Startup and authenticated workspace journey

- [ ] Launch against a clean data root and verify the first-run/identity behavior defined by the startup ADR.
- [ ] Verify paths, logging, crash handler, single-instance lock, SQLite open/migrations/integrity, identity, activation, module registration, shell, and window occur in the fixed startup order.
- [ ] Verify rollback after an injected failure at every startup step.
- [ ] Sign in with valid, invalid, disabled, and malformed credentials; confirm identical unknown-user/wrong-password disclosure.
- [ ] Confirm no plaintext password or `password_hash` reaches QML, logs, crash breadcrumbs, or diagnostics.
- [ ] Verify sign-out, sign-in generation changes, rights changes, activation changes, and stale-page command refusal.
- [ ] Verify list, record, and command calls flow only through `AuthenticatedWorkspace` and the current `RequestContext`.

## 4. Complete route and workflow matrix

For every route, test loading, populated, empty, failure, offline, permission-loss, activation-loss, refresh, paging, selection, back/forward, and deep-link states.

- [ ] Dashboard.
- [ ] Parties.
- [ ] Catalog.
- [ ] Pricing.
- [ ] Orders and counter sale.
- [ ] Receivables.
- [ ] Quotations: draft, issue, immutable revision, revise, accept, expire, PDF.
- [ ] Agreements: create/edit, activate, close, reopen, amend, supersession and consumption.
- [ ] Jobs: assignments, milestones, allowed status actions, bounded progress.
- [ ] Sourcing: supplier profile, purchase history, purchase recording, debt, exactly-once settlement.
- [ ] Companion: list/board/calendar/attention, due, snooze, complete, recurrence display.
- [ ] Files: search/grid, preview, lineage, missing/offline volume, plugin-missing, forget-with-reason; prove QML never receives a raw path.
- [ ] Administration/settings: activation, users/roles/rights, last-admin protection, numbering, sync diagnostics, appearance/language, cache, logs/crash location, version.

Cross-module journeys:

- [ ] Customer → quotation → accepted revision → agreement/order.
- [ ] Counter sale → payment → printable receipt.
- [ ] Order → invoice → payment → statement.
- [ ] Supplier → purchase → debt → settlement.
- [ ] Task/file link → originating record → back.
- [ ] Offline write → queued state → reconnect/reconcile/conflict.

## 5. Accessibility and localization

- [ ] Complete every primary workflow keyboard-only.
- [ ] Verify logical tab order, visible focus, Escape/Enter behavior, dialog focus trapping, and focus restoration.
- [ ] Inspect accessible names, roles, descriptions, value text, and live error announcements with Narrator and a Linux screen reader.
- [ ] Verify status is never color-only.
- [ ] Verify high contrast and reduced motion.
- [ ] Verify 100%, 150%, and 200% text scaling and DPI scaling.
- [ ] Extract translations from first-party QML/C++ and verify there are no untranslated user-facing source strings.
- [ ] Run pseudo-locale/long-translation layouts and localized date, time, quantity, and money rendering.

## 6. Performance and bounded-resource evidence

Record hardware, build type, dataset size, warm/cold state, median, p95, maximum, and threshold for every measurement.

- [ ] Cold process-to-dashboard time.
- [ ] Route-to-first-content time for every module.
- [ ] Dashboard query and list-page latency at documented capacities.
- [ ] Smooth scrolling with maximum page/cache capacity.
- [ ] QML object count and binding-loop warnings.
- [ ] Image memory and thumbnail-cache bounds.
- [ ] PDF peak memory and pagination time.
- [ ] Repeated navigation/detail/editor cycles with no stale bridges, unbounded models, or meaningful retained growth.
- [ ] Confirm a maximum of one live detail/editor stack per active navigation branch.
- [ ] Confirm job/progress updates use events or coarse bounded timers, never unbounded polling.

## 7. Native dependency and visual gates

- [ ] Verify FluentWinUI3 and supplied Fluent resources in light, dark, and high-contrast modes.
- [ ] Verify fonts, icons, QML imports, translations, and deployed plugins on a clean machine.
- [ ] Verify PNG, JPEG, and AVIF decoding plus the explicit AVIF-plugin-missing state.
- [ ] Verify PDF content, pagination, print preview/export, atomic replacement, and long documents.
- [ ] Verify three responsive breakpoints and 200% text scale.
- [ ] Verify QWindowKit title-bar hit testing, drag, resize, maximize/restore, Windows snap, multi-monitor movement, DPI changes, Mica/Acrylic fallback, and Windows 10 fallback.

## 8. Release evidence and closure

- [ ] Save configure logs, build logs, test logs, `qmllint` output, screenshots, accessibility notes, and performance results under `docs/qa/evidence/phase-7/`.
- [ ] Update each Phase 7 gate document with commit hash, runner identity, date, command, and result.
- [ ] Update `docs/plan/todo.md` from `[~]`/`[ ]` to `[x]` only after its mandatory evidence exists.
- [ ] Run `git diff --check` and the repository integrity/architecture policies.
- [ ] Create one final conventional commit for evidence-only closure.
- [ ] Produce a Git-inclusive checkpoint archive and SHA-256 digest.

## Stop conditions

Do **not** declare Phase 7 complete if any of these remain:

- a production route resolves to `ModuleListScreen.qml` or another placeholder;
- the executable grants synthetic rights or bypasses authenticated startup;
- a QML-visible object contains a password hash, secret, raw file path, device ID, volume ID, or stack trace;
- a command can be selected by arbitrary QML operation ID rather than an authorized current snapshot;
- issued evidence is editable;
- a settlement can be applied twice;
- a stale session context can read or write;
- any required Qt/MSVC/Linux runtime gate lacks retained evidence.
