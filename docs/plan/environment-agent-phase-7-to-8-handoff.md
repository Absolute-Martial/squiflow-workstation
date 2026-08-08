# Environment agent handoff: finish Phase 7, then begin Phase 8

Status: executable environment handoff. This file is the entry point for the
agent that has Qt 6.11.1, MSVC 2022, Linux Qt, Podman, PostgreSQL, and network
runtime access. It does not replace the detailed phase plans.

## Read these first, in this order

1. `docs/plan/todo.md` — authoritative completion state; never turn `[ ]` or
   `[~]` into `[x]` without its referenced gate evidence.
2. `docs/qa/phase-7-external-qt-runtime-todo.md` — exact Phase 7 runtime work.
3. `docs/plan/phase-7.7-7.10-application-ui-plan.md` — UI scope and journeys.
4. `docs/plan/phase-8-workstation-server-protocol-plan.md` — workstation/server
   boundary and wire contract.
5. `docs/plan/phase-8-server-plan.md` — 8.1 through 8.13 implementation order.
6. `docs/plan/phase-8-framework-and-provider-isolation.md` — adapter and
   dependency boundaries.
7. `docs/plan/phase-8-backend-capability-map.md` and
   `docs/research/open-source-business-backend-review.md` — completeness and
   open-source reference register.
8. ADRs 0013-0015 — decisions override older prose when they conflict.

## Restore and establish the baseline

- [ ] Restore the Git-inclusive checkpoint and run `git fsck --full`.
- [ ] Confirm branch `master`, clean status, and expected checkpoint commit.
- [ ] Run `make -f tools/sandbox/Makefile check`; retain its complete log.
- [ ] Configure Qt 6.11.1 with the repository's pinned CMake/vcpkg inputs.
- [ ] Never start Phase 8 by weakening or deleting a Phase 7 gate.

## Finish Phase 7 before server work

- [ ] Replace the smoke-only `workstation_main_qt.cpp` path with the concrete
      `RealStartupRuntime` composition.
- [ ] Implement production `StartupServices`: paths, logging, crash handling,
      single-instance activation, secrets, SQLite, migrations, integrity,
      device/shop identity, authentication, module registry, connection state,
      `AuthenticatedWorkspace`, QML surface, and reverse shutdown.
- [ ] Show routes only after a real authenticated session exists; remove all
      synthetic rights and tenant values.
- [ ] Compile/moc/qmlcachegen/qmllint all new record/list bridges and QML pages.
- [ ] Finish module-specific create/edit/action payload forms. A shared visual
      component is allowed; a generic payload guesser is not.
- [ ] Move blocking list/record I/O off the GUI thread if the measured p95
      interaction budget is exceeded.
- [ ] Run all journeys and evidence tasks in
      `docs/qa/phase-7-external-qt-runtime-todo.md`.
- [ ] Commit each correction with a focused Conventional Commit.
- [ ] Close 7.9 and 7.10 only after Windows and Linux Qt evidence passes.
- [ ] Produce and clean-machine test the signed Windows runtime archive.

## Phase 7 stop rule

Do not call Phase 7 complete merely because the portable suite passes. Closure
requires a real Qt build, authenticated startup, executable journey evidence,
accessibility/performance evidence, signing, staged smoke test, and clean-machine
installation/run evidence.

## Begin Phase 8 only from a closed protocol baseline

- [ ] Run the protocol suite and record wire version, module count, rights count,
      operation count, and offline operation count.
- [ ] Treat `external/protocol` as the single cross-process authority. Do not
      duplicate module/right/operation numbers in the server.
- [ ] Implement 8.1 through 8.13 in order unless the server plan explicitly
      identifies a safe parallel stream.
- [ ] Add endpoint-core tests before wiring Hical.
- [ ] Add fake provider contract tests before adding libpqxx, libcurl, libavif,
      object storage, scanning, queue, or telemetry adapters.
- [ ] Keep every third-party type inside its adapter target.
- [ ] Run real PostgreSQL and loopback HTTP integration tests in CI; portable
      fakes alone cannot close Phase 8.

## Required commit sequence

Use focused commits such as:

- `feat(8.1): add transport-neutral server endpoint core`
- `feat(8.1): adapt pinned Hical transport`
- `feat(8.2): add PostgreSQL migration store`
- `feat(8.3): persist opaque device tokens`
- `feat(8.4): implement idempotent sync push`
- `feat(8.4): implement cursor delta pull`
- `test(8.4): add workstation server contract vectors`

Never combine a provider adoption, protocol change, database migration, and
business behavior change in one commit.

## Completion report required from the environment agent

The final handoff must state:

- exact commit and clean status;
- commands and exit codes;
- Qt/CMake/MSVC/PostgreSQL/Hical/provider versions;
- test counts and evidence paths;
- executable/archive hashes and signature verification;
- remaining unchecked items without euphemism;
- no claim of completion when any mandatory gate remains open.
