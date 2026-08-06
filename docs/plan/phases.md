# Phases

Nine phases. Each is cut into sub-phases small enough to fit in one sitting,
end in something that runs, and gate the next sub-phase behind passing tests.
A phase like "build the twelve modules" is not a goal, it is a mood -- there
is no moment where you can call it finished by looking at it, so it is cut
until each piece has a name, a file list, and a done-condition that a machine
can check.

## What this sandbox can actually verify

| Tool | Present? | Consequence |
| --- | --- | --- |
| C++ compiler | Yes -- GCC 11.5.0 | Pure C++ compiles and runs. Both lanes build at `-std=c++23`, but see `language-and-verification.md` for the measured library gap, enforced by the integrity gate. |
| CMake | No | CMake files are written to the plan but unproven here. A plain Makefile (`tools/sandbox/Makefile`) drives the verification lane instead. |
| Qt | No | Nothing in the interface layer compiles here. QML and view models are written and honestly marked unverified. |
| SQLite headers | No (runtime library only, no headers, no install rights) | The data layer cannot be compiled here without a substitute; addressed at Phase 3. |
| Package installation | No -- no administrator rights | None of the above can be fixed by installing something. |

Roughly: the domain layer, protocol spine, module graph, rules engine, and
workflow logic can be compiled and run here. Storage's SQLite-facing code,
sync transport, platform code, and the entire interface cannot -- they are
written, and marked unverified rather than counted as done.

---

## Phase 1 -- Setup and the protocol spine

Repository skeleton, all CMake files, and the thing everything else depends
on: the module list with tiers, the dependency graph, the rights list, the
operation table.

**Verification:** compiled and executed. Tests prove the graph is acyclic,
core is closed under dependency, operation identifiers are unique, and
deactivation closure behaves.

| # | Sub-phase | Files | Done when |
| --- | --- | --- | --- |
| 1.1 | Repository skeleton and build logic | `CMakeLists.txt` (root), `cmake/SquiflowModule.cmake` and `cmake/ModuleGraph.cmake` (module declaration helper + graph checks), `tools/sandbox/Makefile`, `docs/plan/*` | Release, debug, sanitizer, and headless CMake configurations exist and are internally consistent; the sandbox Makefile builds and runs a trivial smoke test. CMake itself is unproven here -- none installed. |
| 1.2 | The module graph | `src/protocol/module_id.hpp`, `src/protocol/module_id.def`, `src/protocol/module_graph.hpp/.cpp`, `tests/protocol/module_graph_test.cpp` | Twelve modules declared with tiers; cycle detection named; core proven closed under dependency; activation closure computed and tested. |
| 1.3 | Rights | `src/protocol/right.hpp`, `src/protocol/right.def`, `tests/protocol/right_test.cpp` | 43 rights declared, each owned by exactly one module; a test enforces uniqueness of ownership. |
| 1.4 | The operation table | `src/protocol/operation_id.hpp`, `src/protocol/operation.hpp/.cpp`, `src/protocol/operation.def`, `tests/protocol/operation_test.cpp` | 67 operations declared, each carrying its owning right, sync class, and offline rule; lookup by name rejects unknown operations rather than guessing. |
| 1.5 | Offline rules and staff exceptions | `src/protocol/offline_rule.hpp`, `src/protocol/staff_offline.def`, `src/protocol/staff_offline_exception.hpp/.cpp`, `tests/protocol/offline_rule_test.cpp` | Counter-sale exceptions are data, not code; a test refuses any exception entry that contradicts its operation's own offline rule. |
| 1.6 | The verification harness | `tools/sandbox/language_probe.cpp`, `tools/sandbox/integrity_check.py` (or equivalent), `tools/sandbox/Makefile` (check target) | One command (`make -f tools/sandbox/Makefile check`) runs the language probe, integrity check, self-containment check, and both test programs, and fails with non-zero exit on any problem. |

**Phase 1 is complete when** the operation table, the rights list, and the
module graph cannot disagree with each other without something failing, and
that failure can be produced by one command.

---

## Phase 2 -- Engine, domain half

Record identity, lifecycle states, numbering, money, quantity, snapshots,
approval and signature types, the rights check. Touches no database and no
Qt by design, so it is fully compiled and executed here.

| # | Sub-phase | Concern |
| --- | --- | --- |
| 2.1 | Identity and time | Record ids, timestamps, clock abstraction. |
| 2.2 | Quantity | Unit-aware quantities, no silent unit mixing. |
| 2.3 | Money | Minor-unit integer money type; no floating point currency anywhere. |
| 2.4 | Lifecycle | The state-machine base every record's lifecycle is built from. |
| 2.5 | Numbering | Sequential, gapless-per-series document numbering. |
| 2.6 | Snapshots, signatures, approvals | Point-in-time snapshots of mutable records; approval and signature value types. |
| 2.7 | Rights, session, capability | The session type and the rights check every write operation goes through. |

File lists and exact done-conditions for 2.1-2.7 are written out in full when
Phase 2 is reached, in this same document -- following the same rule Phase 1
follows, so the plan never claims more precision than has actually been
thought through yet.

---

## Phase 3 -- Engine, storage half

Database gate, single writer, migration runner, outbox, cursor, conflict
rules.

**Verification:** partial. Logic behind an interface (the writer queue, the
migration runner's ordering rules, the outbox's retry logic) is compiled and
tested against an in-memory fake. The SQLite-facing implementation is
written but unverified here (no SQLite headers in this sandbox).

Sub-phases 3.1-3.7 are detailed in this document when Phase 3 is reached.

---

## Phase 4 -- The modules

Each module gets its own domain layer, service layer, data layer, and table
set, one sub-phase per module. Every operation a module exposes is declared
in the Phase 1 operation table first; the module's job is to implement the
handler behind that declaration.

**Verification:** domain and service layers compiled and tested against an
in-memory store. Data layer (SQLite-facing) written, unverified here.

| # | Sub-phase | Migration # | Notes |
| --- | --- | --- | --- |
| 4.1 | Module framework | -- | The shared `Module`, `Registry`, `Call`, `Transaction` seam every module plugs into. |
| 4.2 | administration | 10 | |
| 4.3 | parties | 11 | |
| 4.4 | catalog | 12 | |
| 4.5 | pricing | 13 | Four operations: `rate_set`, `rate_remove`, `rate_override`, `rate_default_set`. |
| 4.6 | orders | 14 | `order_create`, `order_update`, `order_line_add` (sync/offline), `order_cancel` (sync/online-only). |
| 4.7 | receivables | 15 | Eight operations including a local-only statement/print path. |
| 4.8 | jobs | 16 | State machine: Draft -> InProgress -> Done/Cancelled. |
| 4.9 | quotations | 17 | Five operations, all sync/offline. |
| 4.10 | agreements | 18 | Create/update sync/online-only; close/reopen online-required. |
| 4.11 | sourcing | 19 | `purchase_lookup` is local-only. |
| 4.12 | companion | 20 | Four operations, all sync/offline. |
| 4.13 | files | 21 | `index_scan`/`search` local-only; `link`/`forget` sync/offline. |

---

## Phase 5 -- Workflows

The cross-module sequences that never belong to a single module: counter
sale, quote to order, issue invoice, cancel and reissue, apply agreement,
take payment, record purchase.

**Verification:** compiled and executed against in-memory fakes -- the most
valuable tests in the project, because they prove the modules actually
cooperate, not just that each compiles alone.

Sub-phases 5.1-5.8 (one per workflow, plus a shared workflow-harness
sub-phase) are detailed in this document when Phase 5 is reached.

---

## Phase 6 -- Platform and application shell

Windows interfaces (filesystem, printer, clock, single-instance lock) plus
their fakes, startup order, composition root, activation.

**Verification:** fakes compiled and tested. Windows implementations written,
unverified here (no Windows SDK in this sandbox).

Sub-phases 6.1-6.8 detailed when Phase 6 is reached.

---

## Phase 7 -- Interface

Theme, controls, patterns, module screens, view models.

**Verification:** portable shell contracts compile and run in the strict lane.
Qt adapters and QML resources are statically gated and conditionally registered,
but remain marked unverified until a Qt-capable Linux/MSVC lane executes them.

Sub-phases 7.1-7.6 build the interface foundations. Sub-phases 7.7-7.10
build the real dashboard, all module pages and interactions, and the final Qt
runtime/accessibility/performance gate. They are detailed in the Phase 7 plan
documents.

---

## Phase 8 -- Server

Sync endpoints, identity, PostgreSQL schema, media worker, update proxy,
container files.

**Verification:** not verifiable here -- no Oat++, no PostgreSQL.

Sub-phases 8.1-8.8 detailed when Phase 8 is reached.

---

## Phase 9 -- Pipeline and packaging

CI workflows, staging, manifest, signing, installer, updater.

**Verification:** scripts written; the manifest and hashing tools can
actually be run here since they need only the C++ compiler / a scripting
runtime, not the full toolchain.

Sub-phases 9.1-9.6 detailed when Phase 9 is reached.

---

## Rules for every phase

- The architecture rules are enforced by code, not by discipline. Cross-
  references between modules, rights, and operations go through enumerations,
  so a mistake becomes a compile error rather than a runtime surprise.
- A phase ends with its tests passing, or with an explicit statement in
  `status.md` of what could not be run here and why.
- No invented numbers. Any budget or count gets filled in from what the real
  machine actually reports, never guessed.
- Nothing is called done because it looks done -- it is called done because
  `make -f tools/sandbox/Makefile check` exits zero and its output is pasted
  into `status.md`.
