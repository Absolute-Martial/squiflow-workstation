# Engineering code of conduct

Adopted 2026-08-03, before any Phase 6 code was written. These rules are
mandatory for every file in this repository unless the owner explicitly
overrides them in writing.

## 1. General

- Production code only. No prototypes, demos, tutorials, TODOs, placeholders,
  stubs, or partially implemented features.
- Maintainability beats cleverness. Every class, function, and file has one
  clear purpose.
- No unnecessary complexity and no abstraction without a stated reason.

## 2. Architecture and layering

Dependencies flow one way only:

```text
UI  ->  application (workflows)  ->  domain (engine and module domains)  ->  infrastructure
```

- The domain never depends on the UI, on Qt Widgets, or on Windows.
- Platform code is confined to `src/platform/`. No Windows header appears
  outside it. No Qt header appears in any header at all.
- Modules stay decoupled; the module graph check enforces the allowed edges.
- Composition over inheritance. Inheritance is for interfaces and for Qt.
- No global mutable state. Dependencies arrive through the constructor.

## 3. File organisation

- One primary class per header and source pair.
- No "Manager", "Helper", or "Util" classes. Names state a responsibility:
  `PathResolver`, `LocalDirectoryProbe`, `ReceivablesRepository`.
- Soft limits: a class under roughly 500 lines, a function under roughly 60.
  Exceeding either needs a comment saying why.

## 4. C++ rules

The language is C++23; the verification lane is GCC 11.5, which supports the
language but not every C++23 library header. Preferred: RAII, `const`
correctness, `constexpr`, `enum class`, `std::optional`, `std::variant`,
`std::span`, `std::string_view`, structured bindings, range-based loops,
`std::unique_ptr`.

Forbidden: owning raw pointers, manual `new`/`delete`, C-style casts, C-style
arrays where a container fits, macros where a language feature exists,
file-scope `using namespace` in a header, magic numbers, duplicated logic.

Unavailable on the verification toolchain and therefore banned repository wide:
`<expected>`, `<format>`, `<print>`, `<stacktrace>`, `<flat_map>`,
`<generator>`. The integrity pass enforces this.

## 5. Error propagation policy

One strategy, everywhere:

| Kind of failure | Mechanism |
| --- | --- |
| Expected, recoverable | a result value carrying a fault code, a message, and the offending input |
| Rule violation in the domain | `RuleViolation` |
| Programmer error, impossible state | assertion or a thrown `std::logic_error` at a seam that never crosses the UI |
| User-facing | short sentence, no internal detail |
| Diagnostics | the log, never the dialog |

`std::expected` is the natural spelling of the first row and is unavailable on
the verification compiler, so each layer uses a small explicit result struct
with the same shape. No error is ever discarded silently, and no function
returns a default-constructed object to signal failure.

## 6. Ownership and lifetime

Every object answers: who creates it, who owns it, who destroys it, when.
`std::unique_ptr` for exclusive ownership, `std::shared_ptr` only when
ownership is genuinely shared, Qt parent ownership inside the UI. Interfaces
are passed by reference and never owned by the receiver.

## 7. Paths, files, and configuration

- No machine-specific path, drive letter, user name, or install location is
  ever written into the source.
- Locations come from the platform path layer, which on Windows derives them
  from `QStandardPaths` and `QCoreApplication::applicationDirPath()`.
- Shop records live in the machine-wide program-data location, never a
  per-account folder. Cache is per-user and disposable.
- Every directory is created if missing, then proven writable, at startup.
- Every write that must not tear uses an atomic replace (`QSaveFile` in the Qt
  lane).
- Configuration comes from settings or the environment, never from a literal.

## 8. Security

All external input is untrusted: files, paths, configuration, network
responses, user text. Path traversal, control characters, oversized fields,
and integer overflow are checked at the boundary. Secrets never appear in a
log, a settings file, or an error message.

## 9. Qt rules (Phase 7 onward)

Qt is infrastructure and presentation, not the domain. Use `QString`, `QDir`,
`QFileInfo`, `QStandardPaths`, `QSettings`, `QSaveFile`,
`QNetworkAccessManager`, and Model/View directly where they are the right
tool; do not wrap stable Qt classes for the sake of wrapping. Inherit
`QObject` only when signals, slots, properties, or parent ownership are
actually needed. Never touch a widget from a non-UI thread; never block the UI
thread; never let an exception escape into the event loop.

## 10. Testing

Every feature is testable without a `QApplication`, without the network, and
without the real filesystem, because every external dependency sits behind an
interface with a fake. Tests cover normal cases, invalid input, boundaries,
empty states, and failure paths, and are written to be hostile rather than
reassuring.

## 11. Definition of done

A unit of work is done when it compiles with warnings as errors, passes the
full strict suite and the independent CMake lane, handles its failure paths,
contains no placeholder, matches the architecture, is documented where a
reader would otherwise ask why, and introduces no regression.

## 12. Deviations recorded honestly

Where this repository cannot yet satisfy a rule, the reason is written down
rather than hidden. Current deviations:

| Rule | Status | Reason |
| --- | --- | --- |
| `clang-tidy`, `clazy`, `cppcheck` in the loop | Configuration committed, execution deferred | None of the three is installed on the verification machine; they run in the Windows lane and in CI from Phase 9 |
| Sanitizers | Available, off by default | `SQUIFLOW_ENABLE_SANITIZERS` exists; the strict lane runs without them for speed and enables them in the nightly pass |
| `std::expected` | Not used | Absent from the verification toolchain; see section 5 |
| Qt in the platform layer | Only in `*_qt.cpp` sources | Those files cannot be compiled here, so a standard-library implementation of the same interface backs the verification lane |
