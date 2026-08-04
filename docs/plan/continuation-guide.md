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

**42 of 69 sub-phases done and verified**, per `todo.md`'s count at the
time of writing -- re-check that file's own count line, since it is the
authoritative live number, not this sentence.

| Phase | State |
| --- | --- |
| 1 Setup and the protocol spine | complete |
| 2 Engine's domain half | complete |
| 3 Engine's storage half | 6 of 7 -- SQLite-facing code written, unverifiable here |
| 4 The twelve modules | complete |
| 5 Workflows | 7 of 8 -- **5.8 remains, planned below** |
| 6 Platform and application shell | 7 of 8 -- **6.8 remains, planned below** |
| 7 The interface | 0 of 6 fully verified -- **7.2 and 7.3 implemented, portable/static gates green, Qt runtime lane pending** |
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
| 8.1-8.8 | `phase-8-server-plan.md` | Decision D1 (Oat++/Hical pin), Phase 2-5 engine libraries (done) |
| 9.1-9.6 | `phase-9-packaging-plan.md` | Phase 7 (something to build), Phase 8 (something to deploy), 3.5 outbox (done) |

Every one of those documents already contains, for its sub-phases: goal,
scope, non-goals, exact files, invariants, focused tests, gates, an
internal sequence, and acceptance criteria. This guide does not repeat
that content -- it only orders the documents against each other.

## The exact order to work in from here

```text
1.  5.8  Prepare a document for approval or email
2.  6.8  Startup order, fixed and tested
3.  7.1  Window and shell
4.  7.2  Navigation and module visibility from activation (implemented; run Qt gate)
5.  7.3  Lists (implemented; run Qt gate)
6.  7.4  Forms and validation
7.  7.5  Documents and print (spike required first: QPdfWriter without QtWidgets)
8.  7.6  Images and the AVIF plugin (spike required first: AVIF plugin against Qt 6.11.1)
9.  D1   Confirm the Oat++ pin (or the Hical alternative) -- a decision, not code
10. 8.1  Server skeleton and configuration
11. 8.2  PostgreSQL and migrations
12. 8.3  Identity and tokens
13. 8.4  Sync endpoints, idempotency, sequence assignment
14. 8.5  Media, including the AVIF conversion worker
15. 8.6  The update proxy
16. 8.7  Mail -- prepared by the system, sent only on confirmation
17. 8.8  Per-module endpoints
18. 9.1  GitHub Actions across the private repositories
19. 9.2  The ten-step Windows build
20. 9.3  Automated self-signed signing
21. 9.4  The updater
22. 9.5  Podman Quadlet units
23. 9.6  The release
```

**Why this order and not phase-number order:** 5.8 and 6.8 are cheap to
close now -- both only extend code that already exists (the workflow
framework and the startup-sequence scaffolding respectively) and neither
is blocked by anything. Closing them first means Phase 5 and Phase 6 both
reach 100% before any Phase 7 work starts, which keeps the `todo.md`
ledger from ever showing a phase stuck at "nearly done" indefinitely.
7.1-7.6 come next because the QML bridge they depend on is already built.
Phase 8 waits for D1 because starting a server against an undecided HTTP
framework pin means redoing the skeleton later. Phase 9 comes last because
it packages and ships what 7 and 8 produce -- there is nothing to build a
release pipeline around before then, though 9.1 (bare CI) can technically
start earlier if a maintainer wants build/test coverage on Phase 7 work as
it lands.

## Standing rules that apply to every one of the above (restated from `README.md`/`todo.md`)

- A sub-phase is done only when all five gates in `README.md` pass. Not
  four.
- Anything unverifiable in the current sandbox is named in `status.md`,
  never quietly counted as done.
- No invented numbers -- sizes, counts, and test results always come from
  an actual run.
- Two spikes are on the machine-only list in `todo.md` and are
  prerequisites for 7.5 and 7.6 specifically, not general project risks:
  `QPdfWriter` printing without QtWidgets, and the AVIF plugin against the
  pinned Qt 6.11.1 build.
- Decision D1 (Oat++ pin vs. a specific Hical-based alternative) is the
  only remaining open decision blocking a phase (Phase 8). D2-D5 are
  already resolved and recorded in `todo.md`.

## How to resume in one sentence

Open `todo.md` for the live done/not-done ledger, open the plan document
for whichever unchecked sub-phase is next in the order above, and follow
its own file list, invariants, and test list exactly -- do not re-plan a
sub-phase that already has a document here; extend or correct that
document if reality turns out to differ from it, rather than starting
over.
