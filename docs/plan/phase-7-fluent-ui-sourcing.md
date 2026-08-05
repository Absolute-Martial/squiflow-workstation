# Fluent UI component sourcing strategy

Status: planned, not vendored. This sandbox has no network access and no Qt
install, so nothing below can actually be fetched, compiled, or visually
verified here. Everything is written to be vendored and gated on a machine
that has both, following the same discipline as the rest of Phase 7/8's
Qt-only and network-only work.

## Why a hybrid, not one library

Four candidate sources were evaluated. Rather than pick one and wait on its
gaps, this plan layers three of them with an explicit precedence order, and
drops one entirely because it is not actually usable here:

| Source | What it is | Verdict |
| --- | --- | --- |
| Qt's own `FluentWinUI3` style | Official Qt Quick Controls style, ships with Qt 6.8+, zero third-party code | **Layer 1 (base)** |
| `zhuzichu520/FluentUI` | C++ + QML Fluent component library, Qt 6 native, MIT | **Layer 2 (extras)** |
| `RinLit-233-shiroko/Rin-UI` | QML Fluent-Design-like library for Qt Quick, MIT, younger/WIP | **Layer 3 (gap-fill only)** |
| `zhuzichu520/FluentUI2` | GPL-3.0; the repo itself says commercial use or source access requires paying the author | **Dropped** -- this is not a licensing detail to defer, it is a source-access blocker: there is nothing free to vendor |
| `notlousybook/FluentPySide` | A Python/PySide6 packaging tool that repackages Qt's own built-in `FluentWinUI3` QML assets for PySide6 apps | **Dropped as a dependency** -- this project is C++, not Python/PySide6, so the tool itself doesn't apply. Its only real contribution is confirming Layer 1 exists and is the right base |

## Layer 1 -- Qt's native FluentWinUI3 style (done, no vendoring needed)

`src/ui/qtquickcontrols2.conf` now sets `Style=FluentWinUI3` with
`FallbackStyle=Basic` (was `Fusion`/`Basic`). This covers every standard
Qt Quick Controls type (`Button`, `TextField`, `ComboBox`, `CheckBox`,
`Slider`, `SpinBox`, etc.) with Microsoft's own Fluent/WinUI3 look, shipped
by Qt itself. No external code, no license question, no submodule.
Requires Qt >= 6.8; on 6.8 it is a Windows/WinUI3-oriented style but is
Qt's forward direction for Fluent design across platforms.

## Layer 2 -- `zhuzichu520/FluentUI` (planned submodule, extras only)

Used only for controls Layer 1 does not provide: `NavigationView`,
Acrylic/Mica surface effects, `InfoBar`, flyouts/teaching tips, and similar
Fluent-specific compositions beyond stock QQC2. Vendor as a git submodule
at `external/ui-fluent/fluentui`, matching the `external/protocol`
convention:

```
git submodule add --branch main https://github.com/zhuzichu520/FluentUI.git external/ui-fluent/fluentui
git -C external/ui-fluent/fluentui submodule update --init --recursive  # it vendors its own deps (e.g. zxing-cpp)
```

Then pin to an exact commit, never the floating branch head (the same
rule D1 already enforced for the HTTP framework choice):

```
git -C external/ui-fluent/fluentui checkout <chosen-commit-sha>
git add external/ui-fluent/fluentui
git commit -m "chore(ui): pin zhuzichu520/FluentUI to <short-sha>"
```

CMake wiring point (once vendored): add
`add_subdirectory(external/ui-fluent/fluentui)` and link its QML module
from `src/ui/CMakeLists.txt`, gated behind a new `SQUIFLOW_WITH_UI_FLUENT`
option (default off), the same pattern `SQUIFLOW_WITH_QT` already uses --
so the portable sandbox lane keeps building without it.

## Layer 3 -- `Rin-UI` (planned submodule, gap-fill only)

Consulted only when a component is missing from both Layer 1 and Layer 2.
Vendor the same way, under `external/ui-fluent/rin-ui`:

```
git submodule add --branch master https://github.com/RinLit-233-shiroko/Rin-UI.git external/ui-fluent/rin-ui
```

Rin-UI components are imported under their own QML namespace and are never
used to restyle a control already covered by Layer 1 or 2 -- mixing two
styled versions of the same control (e.g. two different `Button` looks) on
one screen is the exact incompleteness-by-inconsistency this hybrid is
meant to avoid, not introduce.

## Precedence rule for every new component

1. Does Qt's `FluentWinUI3` style already provide it? Use it as-is.
2. Otherwise, does `FluentUI` provide it? Use it, imported under its own
   namespace.
3. Otherwise, does `Rin-UI` provide it? Use it, imported under its own
   namespace.
4. Otherwise, hand-roll it in `src/ui/common/`, matching Layer 1's visual
   language (spacing, color tokens in `Theme.qml`).

Record which layer a screen's non-trivial controls came from as a QML
comment at the import site, so a later audit (or a library dropping out)
has a clear list of what needs replacing.

## What is NOT done yet

- Neither submodule is vendored (no network in this sandbox).
- No CMake option/wiring for Layer 2/3 exists yet -- only Layer 1's config
  change is real and committed.
- No exact commit pin has been chosen for either library.
- Visual verification is impossible without a Qt 6.8+ install; mark any
  future work here `[~]` until it runs on a real Qt machine, same as the
  rest of Phase 7's Qt-dependent items.
