# 0001 - Record architecture decisions

Status: Accepted, 2026-08-03

## Context

The project is long lived, built in phases, and will be maintained by someone
who was not present for the arguments. Decisions such as SQLite over a server
database, an explicit module registry over self-registration, or a synchronous
writer gate over a lock-free queue were each made for a concrete reason that is
invisible in the resulting code.

## Decision

Every decision that would be expensive to reverse gets a numbered record here
before the code that depends on it is written. A record states the context, the
decision, the alternatives that were rejected, and the consequences.

## Consequences

Planning slows slightly. Archaeology later becomes unnecessary. Records are
append-only; superseding is explicit.
