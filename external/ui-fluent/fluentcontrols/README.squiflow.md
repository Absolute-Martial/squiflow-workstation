# FluentControls QML module (vendored from FluentPySide)

- Source: https://github.com/notlousybook/FluentPySide, subdirectory
  `fluentpyside/FluentControls` (`main`, uploaded as a zip snapshot by the
  user on 2026-08-06; no commit SHA available from the archive)
- License: MIT (see `LICENSE` in this directory, copied from the parent
  project's `LICENSE`)
- Scope: this is the one genuinely portable part of FluentPySide for a C++
  project. FluentPySide as a whole is a Python/PySide6 packaging tool and
  does not apply here, but `FluentControls` itself is a **pure QML module**
  (no Python) with its own `qmldir` -- 49 components including
  `FluentWindow`, `NavigationView`, `TitleBar`, `InfoBar`, `ProgressRing`,
  `Expander`, `SettingExpander`, plus the `Fluent` singleton (80+ design
  tokens: colors, typography, animation, spacing, radius) that reads
  `Application.styleHints.colorScheme` directly for automatic light/dark
  switching with no polling thread.

## What was kept
- The entire `FluentControls/` QML module as-is (all `.qml` files, the
  `FluentSystemIcons` icon font + JS index, `qmldir`).

## What was dropped
- Everything outside `fluentpyside/FluentControls/`: the Python packaging
  layer (`_frameless.py`, `_mica.py`, `_theme.py`, `_window.py`,
  `_installer.py`, `_loader.py`), the bundled copies of Qt's own
  `FluentWinUI3` style, and the compiled Windows `.dll` style plugin. None
  of that is usable from C++/Qt Quick, and this project already gets the
  native `FluentWinUI3` style for free from the Qt SDK itself (see
  `docs/plan/phase-7-fluent-ui-sourcing.md`, Layer 1).
- The Python-based Mica/Acrylic backdrop API in particular is replaced by
  vendored `qwindowkit` (`external/ui-fluent/qwindowkit`), which does the
  same job natively in C++ and cross-platform, not just on Windows.

## Known gap -- build is not yet wired
- No CMake/QML import path in this repository references this module yet.
  It needs to be registered as a QML import path (or copied into the build
  output) once the Qt Quick shell is being built and visually verified on a
  real Qt 6 machine.
