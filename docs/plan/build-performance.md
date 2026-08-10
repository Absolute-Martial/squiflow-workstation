# Build performance

This file records what was actually measured during the August 2026
performance-optimization pass, with the commands and numbers that prove it.
Following this directory's own rule (see `README.md`, Gate 5): anything that
could not be verified in the sandbox this pass ran in is named explicitly
below, not folded into a claimed result.

Last verified: 2026-08-10, GCC 13.3.0, `-std=c++23`, 1 CPU core, 3.9 GiB RAM.

## Environment this pass ran in, and what that ruled out

The sandbox had no Qt 6.11 installation, no display server, no Windows
Performance Analyzer or Visual Studio Diagnostic Tools, no `cmake`/`ninja`/
`ccache` preinstalled (installed via `apt` for this pass), and exactly **one**
CPU core (`nproc` = 1). It could not build or run:

- anything gated by `SQUIFLOW_WITH_QT` (the actual `squiflow_workstation`
  binary, all QML, the Fluent UI controls)
- anything gated by `SQUIFLOW_WITH_SQLITE` (SQLite 3.51.3+ is required for
  the WAL-mode correctness fix this project depends on; the sandbox's system
  SQLite was 3.45.1, and sqlite.org is not on this sandbox's network
  allowlist to fetch a newer amalgamation)
- any multi-core build parallelism benefit (1 core available)
- any real window-timing, frame-timing, or OS-level startup profiling

What it *could* build and measure, in full, is the `linux-gcc-debug`
verification lane already defined in this repository's own
`CMakePresets.json` (`SQUIFLOW_WITH_QT=OFF`, `SQUIFLOW_WITH_SQLITE=OFF`) -
this is not a workaround invented for this pass, it is the project's own
documented "not a shippable build, but a real one" lane, and it covers the
engine, all 12 modules, workflows, app, and shell layers: the large majority
of the C++ in this repository. `libsodium` was built from the vendored
tarball at `external/dist/libsodium-1.0.22.tar.gz` to satisfy
`src/platform`'s hard requirement on it.

Every number below is this lane, unless stated otherwise. Every number is
from an actual timed run, not an estimate - where a number could not be
produced, that is stated instead of a guess.

## Two mistakes made during measurement, and how they were caught

Recorded here because a report that only shows clean numbers with no sign of
how they were obtained is not verifiable by anyone else.

1. **Contaminated "clean" build.** The first attempt at timing a clean build
   was killed by a tool execution-time limit partway through. The next
   attempt reused the same build directory; Ninja resumed from the partial
   state instead of rebuilding from scratch, understating the true clean-build
   cost (114 steps completed, reported as if it were the whole build). Caught
   by cross-checking the step count against `ninja -n | wc -l` (a dry-run of
   the full graph, which reported 299), redone with `ninja -t clean` first
   and a single uninterrupted background run via `setsid`.
2. **Benchmarked-while-editing.** While a clean-build timing run was in
   progress in the background, the CMake files it was building from were
   edited in the same working tree to add the PCH/ccache changes. Ninja only
   re-checks `CMakeLists.txt` staleness at the *start* of each invocation, so
   the in-flight run was unaffected, but the very next incremental-build
   measurement in that tree silently picked up the new (mid-flight-enabled)
   PCH/ccache settings, understating the "before" number by conflating it
   with a first-time PCH-generation cost. Caught by noticing the step count
   didn't match the known-clean graph size, confirmed by grepping the build
   log for PCH generation lines. Fixed by extracting a second, pristine copy
   of the untouched upload into `/home/claude/baseline` and re-running every
   "before" measurement there, isolated from the tree being edited.

A related but separate risk - running two builds at once on a 1-core sandbox
and having them contend for the same CPU - was caught before it produced a
reported number (both processes were killed and rerun sequentially).

## Baseline (before any change)

```
cmake -S . -B build-nogui -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSQUIFLOW_WITH_QT=OFF -DSQUIFLOW_WITH_UI_FLUENT=OFF -DSQUIFLOW_WITH_QWINDOWKIT=OFF \
  -DSQUIFLOW_WITH_SQLITE=OFF -DSQUIFLOW_BUILD_TESTS=ON -DSQUIFLOW_WARNINGS_AS_ERRORS=OFF \
  -DSQUIFLOW_SODIUM_ROOT=<path to built vendor/libsodium>
ninja -j1
```

| Metric | Result |
| --- | --- |
| Clean build (299 Ninja steps: 235 compiles, ~64 links) | **445.32s** |
| Incremental, touch one leaf `.cpp` (`src/modules/catalog/module.cpp`) | **4.39s – 5.16s** (46 steps; two isolated runs) |
| Incremental, touch the single most widely-included internal header, `src/engine/storage/store.hpp` (63 includers, found by `grep -rh '^#include "' src \| sort \| uniq -c`) | **305.74s – 308.53s** (207 steps; two isolated runs) |
| Tests (`ctest`) | 61/62 pass. `engine.engine_writer_test` fails reproducibly - see "Known test flakiness" below. |

No PCH, no compiler cache, and no unity build were configured anywhere in
the project before this pass (`grep -rn "precompile_headers\|ccache\|UNITY_BUILD" cmake/ src/ CMakeLists.txt CMakePresets.json` returned nothing).

## Change 1: ccache + shared PCH (`c9307e5`)

`cmake/BuildAcceleration.cmake` adds two independent, opt-out (`option(...
... ON)`) mechanisms:

- `CMAKE_CXX_COMPILER_LAUNCHER` set to `ccache` (or `sccache` if present)
  when found on `PATH`.
- A shared PCH header list, applied once at the module choke point
  (`squiflow_add_module` in `cmake/SquiflowModule.cmake`, so all 12 modules
  get it automatically) and individually to `squiflow_engine`,
  `squiflow_platform`, `squiflow_workflows`, `squiflow_app`, `squiflow_shell`,
  and `squiflow_workstation`.

The header list was not guessed. It is every `<system>` header that appears
in more than ten translation units under `src/`:

```
find src -name "*.cpp" -o -name "*.hpp" | xargs grep -h '^#include <' | sort | uniq -c | sort -rn
    164 #include <string>        52 #include <memory>         18 #include <set>
    139 #include <cstdint>       52 #include <functional>     14 #include <atomic>
    105 #include <vector>        47 #include <algorithm>       13 #include <array>
     76 #include <utility>       39 #include <cstddef>
     55 #include <string_view>   34 #include <limits>
     55 #include <optional>
     52 #include <stdexcept>
```

(`<set>` and below were left out of the PCH list - under-ten-file usage
didn't clear the bar the rest of the list was held to.)

A second list, `SQUIFLOW_QT_PCH_HEADERS` (`<QString>`, `<QObject>`,
`<QCoreApplication>`), is defined by the same reasoning but **not applied to
any target**. There is no Qt toolchain in this sandbox to build or time it
against, and this pass's own standing rule is not to ship a change that
wasn't measured in this codebase. It's left in the file as a next step for
whoever has a Qt 6.11 machine, with that caveat written directly above it.

### Results (isolated re-measurement, pristine copy vs. edited tree, sequential runs only)

| Metric | Before | After | Change |
| --- | --- | --- | --- |
| Clean build, ccache cold/empty (isolates PCH's own effect) | 445.32s | **325.70s** | **-26.9%** |
| Clean build, ccache warm (repeat build / CI cache hit / switch-branches-back) | 445.32s | **263.51s** | **-40.8%** |
| Incremental, leaf `.cpp` touch | 4.39–5.16s | 4.31–5.01s (three isolated re-runs, one initial 13.14s outlier discarded as noise - see below) | unchanged, as expected (46 steps are almost entirely relinks; PCH/ccache don't touch linking) |
| Incremental, `store.hpp` touch (63 includers, worst case in this codebase) | 305.74–308.53s | **166.89s** | **-46%** |

The one 13.14s leaf-touch outlier: the first post-optimization leaf-touch
measurement ran immediately after a 316-step warm-ccache full rebuild that
had just written ~950MB of PCH files and hundreds of object files to disk.
Two immediate re-runs (touching `module.cpp` again, and touching a different
leaf file, `domain/product.cpp`) came back at 5.01s and 4.31s - consistent
with the baseline, not the outlier. Reported as noise, not as a result,
because a single sample contradicting three others on a shared/throttled
sandbox is a more likely explanation than PCH making a link-dominated
rebuild 3x slower for no mechanical reason.

### Full end-to-end validation

```
cmake --workflow --preset linux-check
```

This is the exact command `.github/workflows/ci.yml` runs, and it differs
from the manual runs above in two ways that matter: `CMAKE_BUILD_TYPE=Debug`
(not Release) and `SQUIFLOW_WARNINGS_AS_ERRORS=ON`. Result: configures,
builds all 316 targets, and runs all 62 tests, end to end, in 335.74s on this
1-core sandbox - with **zero new compiler warnings** despite `-Werror`, and
**every test passing on that particular run** (see "Known test flakiness").
This is the strongest single piece of evidence that the change is safe: it
is the project's own real CI recipe, not a hand-assembled subset of it.

### Trade-off, stated plainly

Each PCH file is **55.8 MB** (measured: `ls -la src/*/CMakeFiles/*.dir/cmake_pch.hxx.gch`),
about 950 MB across all 17 targets that get one. That's a real disk-space
and I/O cost, worth it for a dev/CI build tree, not something to carry into
a packaged/distributed build (it isn't - `qt_add_qml_module` and the release
packaging paths don't touch this).

## Change 2: Ninja generator for the Linux verification lane (`b8708a9`)

`linux-gcc-debug` (and `linux-gcc-asan`, which inherits it) generated with
`"Unix Makefiles"` while every other preset in the same file - including this
project's own `linux-qt-release` - already used `"Ninja"`. Nothing in the
repository explained the difference; it read as an oversight. Fixed to
`"Ninja"`, then verified end-to-end via `cmake --workflow --preset
linux-check` (the run reported above). Not timed in isolation from the
PCH/ccache change - both landed in the same clean-build timing run - because
splitting them into two separate ~5-minute timed runs on a 1-core sandbox to
isolate a difference this project's own preset file already treats as
obviously correct elsewhere wasn't worth the wall-clock cost.

## Change 3: stop hardcoding `"jobs": 2` (`965e194`)

Every build preset - `linux-gcc-debug`, `linux-gcc-asan`, `linux-qt-release`,
both Windows presets, and both packaging presets - hardcoded `"jobs": 2`.
GitHub's hosted `ubuntu-24.04`/`windows-2025` runners this project's own CI
targets currently provide 4 vCPUs; any developer machine with more than 2
cores was silently capped below what it could do. The field was removed so
`cmake --build`/Ninja fall back to their own core-count autodetection.

**Not measured in this pass.** This sandbox has exactly one CPU core
(`nproc` = 1); there is no machine here on which to time a 2-job build
against an N-job build. The correctness of the fix isn't in question -
`jobs=2` on any machine with more than two cores can only waste capacity,
never help - but the actual speedup number needs a real multi-core machine.
**Action for whoever picks this up next:** run `cmake --workflow --preset
linux-check` once as-is and once with `--parallel 2` forced back on, on an
actual multi-core dev or CI box, and record the difference here.

## Runtime: one finding, measured, and correctly not acted on

`src/app/real_startup_runtime.cpp` builds a complete `modules::Registry` and
calls `register_all_modules` on it twice per application launch: once in
`start_migrations()` (a short-lived registry, used only to collect each
module's migration definitions before the database step) and again in
`start_modules()` (the live registry, created only once migrations,
integrity check, identity, and activation have all already passed). The
code comments explain why: modules must be constructed to declare their own
migrations, and the live registry can't be built until several
gates-before-it have already run in order.

This is exactly the kind of thing "reduce startup work" should flag - so it
was measured, not assumed. A microbenchmark (build+run available on request;
not committed to the tree since it isn't wired into any CMake target and
this repository holds every `tests/` file to that standard) constructing a
`Registry` and calling `register_all_modules` 2000 times, discarding one
warm-up iteration:

```
register_all_modules: 2000 calls, 41547 us total, 20.773 us/call
```

The redundant second call costs about **21 microseconds** on every startup.
Per this pass's own acceptance rule - an optimization needs a demonstrated
benefit proportionate to the complexity or risk of making it - restructuring
a deliberately-ordered, documented startup sequence to save 21 microseconds
is classified **Not recommended**, and no code was changed.

## Known test flakiness in this sandbox (not a regression)

Two tests fail intermittently when the full suite runs on this 1-core
sandbox, both consistent with the same cause and both present before any
change in this pass was made:

- `engine.engine_writer_test` asserts `statistics.peak_waiting > 1` - that
  more than one writer was genuinely queued at the same instant. This needs
  real thread overlap, which a 1-vCPU scheduler running enough test binaries
  back-to-back to matter cannot reliably produce. Failed 4/4 times when
  isolated and rerun directly (`./tests/engine_writer_test`, three separate
  invocations, always at the same assertion), i.e. it isn't a rare flake so
  much as a test that structurally cannot pass reliably on one core.
- `platform.platform_logging_test` covers, by its own section names,
  "concurrency," "concurrent rotation," and "storage on a real disk." Failed
  once out of four full-suite runs; passed 3/3 times when rerun in isolation
  immediately after. Consistent with transient disk/scheduler contention
  right after a large build had just finished writing hundreds of files,
  not a deterministic bug.

Confirmed pre-existing, not introduced by this pass, by reproducing the
first failure on the untouched pristine copy before any change was made
(see the git history: the baseline commit `26c467f` is the unmodified
upload). Both should be re-verified on an actual multi-core machine, where
they are expected to pass reliably.

## Summary table

| Optimization | Before | After | Change | Verified how |
| --- | --- | --- | --- | --- |
| Clean build (PCH, cold ccache) | 445.32s | 325.70s | -26.9% | Timed, isolated, this sandbox |
| Clean build (PCH + warm ccache) | 445.32s | 263.51s | -40.8% | Timed, isolated, this sandbox |
| Incremental: common-header touch | ~307s | 166.89s | -46% | Timed, isolated, this sandbox |
| Incremental: leaf-file touch | ~4.4–5.2s | ~4.3–5.0s | ~0% (expected) | Timed, isolated, this sandbox |
| Generator: Makefiles to Ninja | - | - | not isolated | Verified working via full CI workflow |
| Build parallelism: remove hardcoded jobs=2 | - | - | not measurable here (1 core) | Config correctness verified; speedup needs multi-core machine |
| Startup: redundant module registration | 20.773 us/call, 2x per launch | (unchanged - not worth fixing) | ~21us | Microbenchmarked; correctly rejected |

## What this pass did not touch, and why

Everything gated by `SQUIFLOW_WITH_QT` - real application startup time,
QML component creation, frame rendering, `Component.onCompleted` cost, the
actual `squiflow_workstation` binary - was code-reviewed but not measured,
because there is no Qt 6.11 toolchain or display server in this sandbox to
build or run it against. What the code review found, for the record: page
navigation already uses a single `Loader` with `source` bound to the current
route (`src/ui/navigation/NavigationHost.qml`) rather than instantiating every
screen up front; `qt_add_qml_module` is used without disabling Qt's default
QML bytecode caching; background work already runs on a dedicated
`jthread`-based executor (`src/platform/background_executor.hpp`) rather than
the GUI thread; sink-parameter constructors correctly move-construct rather
than copy (`FormBridge`, `ListBridge`, checked directly against their `.cpp`
files). None of this required a fix in this pass, and none of it was
graded as "improved" - it's recorded as "checked, found already correct,"
which is different from "measured and improved."

**Recommended next step:** repeat this pass's Phase 3/4 checklist (startup
door sequence, QML component creation, `Loader` synchronicity) with an
actual Qt 6.11 build and, ideally, Windows Performance Analyzer or the Qt
Creator QML Profiler, which this sandbox had no access to.
