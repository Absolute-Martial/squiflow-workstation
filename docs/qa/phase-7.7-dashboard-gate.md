# Phase 7.7 dashboard and shell gate

Status: portable implementation complete; Linux Qt 6.11.1 and Windows MSVC
runtime evidence is required from CI before the phase is marked fully closed.

## Delivered

- semantic light/dark/system theme tokens;
- reusable page scaffold, command bar, banners, cards, empty/error/loading
  states, and confirmation/unsaved dialogs;
- responsive shell with compact drawer, standard rail, global search, command
  palette, identity, connectivity, notifications, and appearance controls;
- bounded and deduplicated portable notification queue;
- portable unsaved-change route guard;
- immutable dashboard contract and permission/activation-filtering service;
- generation/session-safe dashboard bridge that rejects stale account results;
- dashboard Qt presentation model and a real `dashboard.home` route;
- offscreen `--smoke-test` startup/shutdown mode;
- pinned syscmdline and qtimageformats source;
- Linux portable, Linux Qt, Windows MSVC, and Windows release workflow gates.

## Portable evidence

Run:

```bash
python3 tools/sandbox/check_dashboard_and_shell.py
make -f tools/sandbox/Makefile check
```

Required focused tests:

- `app_dashboard_service_test`: empty/populated, permissions, activation,
  exact money, malformed and bounded source snapshots;
- `shell_dashboard_bridge_test`: refresh race, account switch, offline cache,
  failure and cancellation;
- `shell_state_test`: identity/theme/connectivity, dirty navigation,
  confirmation, notification deduplication and hard capacity.

Recorded on 2026-08-06:

- dashboard service: **9 checks, 0 failed**;
- dashboard bridge: **10 checks, 0 failed**;
- shell state/notifications: **14 checks, 0 failed**;
- full strict gate: **435 integrity files**, **192 self-contained headers**, all
  architecture policies and all portable test programs passed;
- terminal marker: `DONE_EXIT:0` in `/tmp/phase77-final.log`.

CMake was not installed in this sandbox, so no local CMake, Qt, QWindowKit, or
MSVC result is claimed. Those results remain mandatory CI evidence.

## CI evidence still required

The `linux-qt` and `windows-msvc` jobs must prove:

- Qt 6.11.1 configure and compile;
- QWindowKit Core+Quick plus host qmsetup/syscmdline build;
- CTest success;
- QML root and dashboard construction with `QT_QPA_PLATFORM=offscreen`;
- clean normal shutdown without a stale callback or leaked route bridge.

The release job must prove the staged `windeployqt` bundle launches before its
archive and checksum are published.

## Visual evidence deferred to Phase 7.10

CI/offscreen construction is not a substitute for screenshots and interaction.
Record compact/standard/wide layouts, light/dark/high contrast, keyboard order,
screen-reader output, 200% text, QWindowKit native chrome, AVIF plugin loading,
PDF output, DPI/multi-monitor behavior, startup latency, and object/memory
budgets in the Phase 7.10 release gate.
