# Fluent UI component sourcing strategy

Status: source vendored, wiring scaffolded, nothing built or visually
verified. This sandbox has no Qt install, so nothing below can be compiled
or checked here. Everything is written to be enabled and gated on a machine
that has a Qt 6.8+ toolchain, following the same discipline as the rest of
Phase 7/8's Qt-only work.

## Why a hybrid, not one library

Five candidate sources were evaluated (the user uploaded the actual source
for four of them as zip snapshots). Rather than pick one and wait on its
gaps, this plan layers four of them with an explicit precedence order, and
drops the fifth because it is not actually usable here:

| Source | What it is | License | Verdict |
| --- | --- | --- | --- |
| Qt's own `FluentWinUI3` style | Official Qt Quick Controls style, ships with Qt 6.8+ | N/A (Qt) | **Layer 1 (base)** |
| `stdware/qwindowkit` | C++ cross-platform frameless window + native Mica/Acrylic/Blur backdrop | Apache-2.0 | **Layer 0 (window chrome)** |
| `zhuzichu520/FluentUI` | C++ + QML Fluent component library, `Flu`-prefixed types | MIT | **Layer 2a (extras)** |
| FluentPySide's `FluentControls` module | Pure-QML Fluent component set + `Fluent` design-token singleton | MIT | **Layer 2b (extras)** |
| `RinLit-233-shiroko/Rin-UI` | Pure-QML Fluent-Design-like component set | MIT | **Layer 3 (gap-fill only)** |
| `zhuzichu520/FluentUI2` | Same author's second Fluent library | GPL-3.0 | **Dropped** -- the repo itself says commercial use or source access requires paying the author; there is nothing free to vendor, and it was never uploaded |

FluentPySide as a whole is a Python/PySide6 packaging tool and does not
apply to this C++ project, but one part of it -- the `FluentControls` QML
module -- is pure QML with no Python dependency, so it is vendored on its
own merits as Layer 2b. The rest of FluentPySide (its Python Mica/Acrylic
API, its repackaged copy of Qt's own style, its compiled Windows plugin) is
not vendored: Layer 0 (`qwindowkit`) already gives this project a real,
cross-platform, native C++ Mica/Acrylic/Blur backdrop, and Layer 1 already
gives it Qt's own style directly from the SDK.

## What is actually vendored now

| Path | Source | Kept | Dropped |
| --- | --- | --- | --- |
| `external/ui-fluent/qwindowkit` | `stdware/qwindowkit` | `src/core`, `src/quick`, top `CMakeLists.txt` | `src/widgets` (unused QWidgets module), examples, docs |
| `external/ui-fluent/fluentui` | `zhuzichu520/FluentUI` | `src/` in full (self-contained: bundles its own `qhotkey`/`qrcode`/`qmlcustomplot`) | `3rdparty/` prebuilt Windows DLLs, examples, docs |
| `external/ui-fluent/fluentcontrols` | FluentPySide's `fluentpyside/FluentControls` | the whole pure-QML module | everything else in FluentPySide (Python layer, bundled `FluentWinUI3` copies) |
| `external/ui-fluent/rin-ui` | `RinLit-233-shiroko/Rin-UI` | `RinUI/{components,windows,themes,utils,animations,assets,languages}` | `RinUI/core/*.py`, `RinUI/hooks/*.py` (Python loader) |

Each directory has a `README.squiflow.md` recording its exact source,
license, and what was pruned. None of these came with a resolvable commit
SHA (they were uploaded as zip snapshots, not clones), so that is recorded
honestly as unknown rather than guessed.

## Layer 0 -- QWindowKit (vendored, not yet wired)

Gives this project real capability 3 from the FluentPySide feature list
("Mica/Acrylic backdrop") natively in C++, cross-platform, instead of
PySide6-specific Python ctypes. **Known gap:** its `CMakeLists.txt` needs a
companion CMake helper, `qmsetup` (github.com/stdware/qmsetup), normally
pulled in as a submodule; the uploaded snapshot didn't include it and this
sandbox has no network to fetch it. Not wired into the build yet for that
reason -- see `external/ui-fluent/qwindowkit/README.squiflow.md`.

## Layer 1 -- Qt's native FluentWinUI3 style (done)

`src/ui/qtquickcontrols2.conf` sets `Style=FluentWinUI3`. Covers every
standard Qt Quick Controls type. No vendoring needed.

## Layer 2 -- extras (vendored, import path wired, not yet compiled)

- **2a, `FluentUI`:** `Flu`-prefixed types (`FluButton`, `FluWindow`, ...),
  plus unique extras this project may want later (QR codes, real-time
  plots, global hotkeys). Requires Qt Widgets + PrintSupport in addition to
  what this shell already links -- deliberately left commented out in
  `src/ui/CMakeLists.txt` until that extra dependency weight is confirmed
  as worth it, rather than silently taking it on.
- **2b, `FluentControls`:** plain-named types (`FluentWindow`,
  `NavigationView`, `TitleBar`, `InfoBar`, `ProgressRing`, `Expander`,
  `SettingExpander`, and more -- 49 components total) plus the `Fluent`
  singleton: 80+ design tokens (colors, typography, animation, spacing,
  radius) that read `Application.styleHints.colorScheme` directly, so
  light/dark auto-switching (capability 4) needs no polling thread and no
  Python. This satisfies capabilities 1, 2, and 4 from the user's request
  directly as portable QML with zero Python dependency.
- Both are additive, not competing: 2a's types are `Flu`-prefixed so they
  cannot collide with 2b's plain names.

## Layer 3 -- Rin-UI (vendored, import path wired, gap-fill only)

Used only when a component is missing from Layers 1-2. Rin-UI's own
`windows/FluentWindow.qml` and `windows/TitleBar.qml` use the *same* plain
names as Layer 2b -- this is fine only because of the precedence/collision
rule below, never by accident.

## Precedence rule for every new screen or component

1. Does Qt's `FluentWinUI3` style already provide it (standard control)?
   Use it as-is, no import needed.
2. Otherwise, does `FluentControls` (2b) provide it? Import it. This is the
   default choice for anything Fluent-specific (window chrome, navigation,
   info bars, flyouts) because it ships the richest token system.
3. Otherwise, does `FluentUI` (2a) provide it (once its extra Widgets/
   PrintSupport cost is accepted)? Import it under its `Flu` names.
4. Otherwise, does `Rin-UI` (3) provide it? Import it with an explicit
   `as` alias.
5. Otherwise, hand-roll it in `src/ui/common/`, matching Layer 1/2b's
   token language (`Theme.qml` today; migrating those tokens onto the
   `Fluent` singleton is a future, separate decision, not implied here).

**Hard rule to avoid collisions:** never import more than one of
{`FluentControls`, `FluentUI`, `RinUI`} into the same `.qml` file. A given
screen picks exactly one extras layer for its Fluent-specific chrome.
Record which layer a screen's non-trivial controls came from as a QML
comment at the import site.

## What is NOT done yet

- Nothing has been compiled or visually verified (no Qt install here).
- `qwindowkit` is vendored but not wired into any CMake target (missing
  `qmsetup`).
- `FluentUI` (2a) has commented-out CMake wiring, pending a deliberate
  decision to take on its extra Qt Widgets/PrintSupport dependency.
- `FluentControls` (2b) and `Rin-UI` (3) have their import paths wired via
  `SQUIFLOW_WITH_UI_FLUENT` (off by default) but no screen imports them
  yet.
- No exact commit SHA is recorded for any of the four vendored sources
  (uploaded as zip snapshots, not git clones).
- Mark any future work here `[~]` until it runs on a real Qt machine.
