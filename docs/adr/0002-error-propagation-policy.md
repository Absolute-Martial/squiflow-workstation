# 0002 - One error propagation policy without std::expected

Status: Accepted, 2026-08-03

## Context

Mixed error styles are the usual way a C++ codebase becomes unpredictable:
some functions throw, some return a bool, some return an empty object that the
caller forgets to check. The natural modern spelling for a recoverable failure
is `std::expected`, but the verification compiler on the build and test machine
is GCC 11.5, which does not ship it. The integrity pass already refuses the
C++23 library headers this toolchain lacks, so using it would split the two
lanes and make the strict suite unrunnable.

## Decision

Recoverable failures return a small explicit result struct with the same shape
as `std::expected` would have: a success flag, a fault enumeration, a human
sentence, and the offending input. Domain rule breaches keep using
`RuleViolation`. Impossible states assert. Nothing returns a default
constructed value to mean failure, and no failure is discarded.

## Alternatives rejected

- Exceptions for recoverable failures: they cross the Qt event loop unsafely
  and turn ordinary control flow into an unwind.
- A third-party expected implementation: a dependency for a language feature
  that arrives with the shipping compiler anyway.

## Consequences

A small amount of boilerplate per result type. One reading habit everywhere.
When the verification toolchain gains `<expected>`, the structs can be
replaced mechanically because their shape already matches.
