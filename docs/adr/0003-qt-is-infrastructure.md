# 0003 - Qt is infrastructure, never domain

Status: Accepted, 2026-08-03

## Context

Qt 6 is the framework for the shipping Windows application. It is also easy to
let it spread: a `QString` in a domain model, a `QObject` base class on a
calculator, a `QSqlQuery` returned from a repository. Once that happens the
business rules can only be tested by starting an application.

## Decision

Qt appears in three places only: the presentation layer, the infrastructure
implementations of declared interfaces, and platform sources named `*_qt.cpp`
or `*_win.cpp`. No header in the repository includes a Qt header. Domain and
application layers exchange plain C++ types. Qt classes are used directly
where they are the right tool; they are not wrapped for the sake of symmetry.

## Alternatives rejected

- Wrapping `QString`, `QFile`, and friends behind house interfaces: pure cost,
  no testability gain, and it hides idiomatic Qt from Qt developers.
- Allowing Qt in the domain for convenience: it would make the entire strict
  verification lane, which runs without Qt installed, impossible.

## Consequences

The engine, every module, and every workflow compile and run on a machine with
no Qt at all, which is exactly how the strict suite is executed today. A Qt
backed implementation of a platform interface cannot be exercised on that
machine, so each such interface also has a standard-library implementation and
a fake, and the quality gate says plainly which lane proved what.
