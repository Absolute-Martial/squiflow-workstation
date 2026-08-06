# Rin-UI QML components (vendored)

- Source: https://github.com/RinLit-233-shiroko/Rin-UI (`master`, uploaded
  as a zip snapshot by the user on 2026-08-06; no commit SHA available from
  the archive)
- License: MIT (see `LICENSE` in this directory)
- Scope: gap-fill only, per `docs/plan/phase-7-fluent-ui-sourcing.md`.
  Rin-UI ships a Python/PySide6 convenience loader (`RinUI/core`,
  `RinUI/hooks`, `RinUI/__init__.py`) around what is otherwise a pure QML
  component set. This project vendors only the QML side and will register
  the QML module directly from C++ (`QQmlApplicationEngine::addImportPath`)
  instead of using Rin-UI's Python loader.

## What was kept
- `RinUI/components`, `RinUI/windows`, `RinUI/themes`, `RinUI/utils`,
  `RinUI/animations`, `RinUI/assets`, `RinUI/languages` -- the QML types,
  themes, fonts, and translation files.

## What was dropped
- `RinUI/core/*.py`, `RinUI/hooks/*.py`, `RinUI/__init__.py` (the Python
  loader/launcher/theme-manager/translator, and the PyInstaller hook --
  none of it applies to a C++ Qt Quick application).

## Naming collision note
- Rin-UI's `windows/` types (`FluentWindow.qml`, `TitleBar.qml`) use the
  same plain names as the vendored `fluentcontrols` module. Per the
  precedence rule in `docs/plan/phase-7-fluent-ui-sourcing.md`, Rin-UI is
  gap-fill only: a screen imports either `FluentControls` or `RinUI` (with
  an explicit `as` alias), never both into the same QML file, so this never
  becomes a real ambiguity.

## Known gap -- build is not yet wired
- No CMake/QML import path in this repository references this module yet;
  pending a real Qt 6 build/verification lane.
