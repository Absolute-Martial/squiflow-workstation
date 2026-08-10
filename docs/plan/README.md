# The plan

This directory holds the plan for SquiFlow, and it lives in the repository on
purpose: it travels with the code, and it is reviewable in the same commit as
the changes it describes.

- **`phases.md`** -- all nine phases broken into sub-phases, each with its
  files and its done-condition. The intention.
- **`status.md`** -- what is actually finished, with the output that proves
  it. The record.
- **`build-performance.md`** -- the August 2026 build/startup/runtime
  performance pass: what was actually measured, what changed, and what could
  not be verified in the sandbox that pass ran in.
- **`language-and-verification.md`** -- the C++23 rules this project holds
  itself to, and the measured gap between what the compiler flag promises and
  what the standard library actually ships here.

`phases.md` and `status.md` are kept separate on purpose: a plan that edits
itself to match what happened is not a plan.

## The five gates

A sub-phase is not done until it clears all five. They run in this order
because each gate assumes the ones before it passed.

| Gate | What it catches |
| --- | --- |
| 1. Integrity | Encoding, BOMs, CRLF, tabs, trailing whitespace, non-ASCII, missing `#pragma once`, a `.cpp` not including its own header first, and orphaned sources named by no build file. |
| 2. Self-containment | Every header compiles alone, twice, with nothing included before it. A header that only works because something else came first is a trap for whoever includes it next. |
| 3. Compilation | `-Werror` with the full warning set. `-Wconversion` and `-Wsign-conversion` matter most: this software computes money, and a silent narrowing is a wrong number on an invoice, not a style problem. |
| 4. Execution | Compiling proves the shape of the code. Only running the tests proves the behaviour. |
| 5. Honesty | Any file that cannot be verified in this sandbox (no CMake, no Qt, no SQLite headers, no admin rights to install anything) is listed by name in `status.md`, never quietly counted as done. |

## Running the gates

One command runs all of it, and fails with a non-zero exit code the moment
anything is wrong:

```
make -f tools/sandbox/Makefile check
```

When CMake is available on a real machine, the second, independent lane is:

```
cmake -S tools/sandbox/cmake-verify -B build/cmake-verify -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Debug -DSQUIFLOW_WITH_SQLITE=OFF -DSQUIFLOW_WARNINGS_AS_ERRORS=ON
cmake --build build/cmake-verify
ctest --test-dir build/cmake-verify --output-on-failure
```

Both lanes must agree. If they diverge, that divergence is itself a bug to
fix before continuing, not a detail to note and move past.

## Rule for every sub-phase

1. State the objective, the files, and the done-condition before writing
   anything.
2. Write one file at a time, in the stated order. Complete and self-consistent,
   never a stub.
3. After each file, re-run the gates that apply to what changed. Fix failures
   immediately; never carry a known-broken file forward.
4. When every file in the sub-phase is written and every gate passes, update
   `status.md` with the exact command output that proves it, and only then
   move to the next sub-phase.
