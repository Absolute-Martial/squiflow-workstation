# Continuation guide -- where this project stands and how to pick it up

This document exists so that anyone -- a future session, a different
person entirely -- can continue this project without needing anyone's
memory. It links every remaining phase to its detailed implementation plan
and states, in one place, the exact order to work in.

Read `README.md`, `phases.md`, and `todo.md` in this same directory first
for the project's overall method and the done/not-done ledger. This
document only adds: exact plan documents for everything not yet started,
and the dependency order across all of them.

## Current state, as of this document's commit

The authoritative completion ledger is `todo.md`. This guide deliberately does
not copy its numeric total because adding 7.7-7.10 changes the denominator and
a duplicated count becomes stale.

| Phase | State |
| --- | --- |
| 1 Setup and the protocol spine | complete |
| 2 Engine's domain half | complete |
| 3 Engine's storage half | 6 of 7 -- SQLite-facing code written, unverifiable here |
| 4 The twelve modules | complete |
| 5 Workflows | 7 of 8 -- **5.8 remains, planned below** |
| 6 Platform and application shell | 7 of 8 -- **6.8 remains, planned below** |
| 7 The interface | 0 of 10 fully verified -- **7.1-7.6 implemented in portable/static lanes; 7.7-7.10 product UI planned; Qt runtime lane pending** |
| 8 The server | 0 of 8 -- **all of 8.1-8.8 planned below** |
| 9 Build, packaging, release | 0 of 6 -- **all of 9.1-9.6 planned below** |

The QML presentation-bridge architecture (the seam between Qt/QML and the
pure-C++23 domain/engine/workflow code) is already fully implemented and
gated, ahead of Phase 7's own sub-phases actually using it. Read
`docs/plan/qml-presentation-bridge.md` before touching any `src/shell`
file -- it explains the seam every Phase 7 sub-phase below builds on.

## Every remaining plan document, in one place

| Sub-phase(s) | Plan document | Depends on |
| --- | --- | --- |
| 5.8 | `phase-5.8-approval-and-send.md` | 5.1, 2.6, 3.5 (all done) |
| 6.8 | `phase-6.8-startup-order.md` | 6.1-6.7 (all done), extends existing `src/app/startup*.hpp/.cpp` |
| 7.1, 7.3, 7.4, 7.5, 7.6 | `phase-7-interface-plan.md` | 7.2 (`phase-7.2-navigation-and-activation.md`), 6.8, the QML bridge (done) |
| 7.2 | `phase-7.2-navigation-and-activation.md` | 6.8, the QML bridge (done) |
| 7.7-7.10 | `phase-7.7-7.10-application-ui-plan.md` | 7.1-7.6 foundations and existing module operations |
| 8.1-8.8 | `phase-8-server-plan.md` | Hical decision recorded; Phase 2-5 engine libraries done |
| 9.1-9.6 | `phase-9-packaging-plan.md` | Phase 7 (something to build), Phase 8 (something to deploy), 3.5 outbox (done) |

Every one of those documents already contains, for its sub-phases: goal,
scope, non-goals, exact files, invariants, focused tests, gates, an
internal sequence, and acceptance criteria. This guide does not repeat
that content -- it only orders the documents against each other.

## The exact order to work in from here

```text
1.  7.7  Design system, application shell, and real dashboard
2.  7.8  Master-data and primary commercial pages
3.  7.9  Remaining operational and supporting module pages
4.  7.10 UI integration, accessibility, performance, and Qt runtime closure
5.  8.1  Server skeleton and configuration
6.  8.2  PostgreSQL and migrations
7.  8.3  Complete identity-token storage and HTTP wiring
8.  8.4  Sync endpoints, idempotency, sequence assignment
9.  8.5  Media, including the AVIF conversion worker
10. 8.6  The update proxy
11. 8.7  Mail -- prepared by the system, sent only on confirmation
12. 8.8  Per-module endpoints
13. 9.1  GitHub Actions across the private repositories
14. 9.2  The ten-step Windows build
15. 9.3  Automated self-signed signing
16. 9.4  The updater
17. 9.5  Podman Quadlet units
18. 9.6  The release
```

**Why this order:** 7.7-7.10 come next because the 7.1-7.6 UI
foundations and all twelve module operations they depend on are implemented
in the portable/static lanes. This produces the real native workstation before
more server scope is added. The Hical server decision is already recorded, so
Phase 8 can follow without another framework-selection pause. Phase 9 comes
last because it packages and ships the completed workstation and server,
although 9.1 may be pulled forward solely to add CI coverage for UI work.

## Standing rules that apply to every one of the above (restated from `README.md`/`todo.md`)

- A sub-phase is done only when all five gates in `README.md` pass. Not
  four.
- Anything unverifiable in the current sandbox is named in `status.md`,
  never quietly counted as done.
- No invented numbers -- sizes, counts, and test results always come from
  an actual run.
- The 7.5 PDF and 7.6 AVIF implementations exist in the portable/static lane;
  their real Qt 6.11.1 runtime and visual gates remain prerequisites for 7.10.
- Decision D1 is resolved in favor of Hical. Remaining decisions and their
  owners are recorded in `todo.md`; none blocks starting 7.7.

## How to resume in one sentence

Open `todo.md` for the live done/not-done ledger, open the plan document
for whichever unchecked sub-phase is next in the order above, and follow
its own file list, invariants, and test list exactly -- do not re-plan a
sub-phase that already has a document here; extend or correct that
document if reality turns out to differ from it, rather than starting
over.
