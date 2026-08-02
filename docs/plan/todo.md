# The list

Every phase and every sub-phase in the project, in one place, with a mark
against each. `phases.md` says what each one is for; this file says only
whether it is done.

It exists so that the state of the work does not live in anyone's memory,
including mine.

**Legend:** `[x]` done and verified · `[~]` written but not verifiable on this
toolchain · `[ ]` not started

**Count: 39 of 69 sub-phases done.**

| Phase | Sub-phases | Done |
|---|---|---|
| 1 - Setup and the protocol spine | 6 | 6 |
| 2 - The engine's domain half | 7 | 7 |
| 3 - The engine's storage half | 7 | 6 |
| 4 - The twelve modules | 13 | 13 |
| 5 - Workflows | 8 | 7 |
| 6 - Platform and the application shell | 8 | 0 |
| 7 - The interface | 6 | 0 |
| 8 - The server | 8 | 0 |
| 9 - Build, packaging and release | 6 | 0 |

---

## Phase 1 - Setup and the protocol spine — **complete**

- [x] 1.1 Repository skeleton and build logic
- [x] 1.2 The module graph - twelve modules, tiers, cycle detection, core closed
- [x] 1.3 Rights - 44, each owned by exactly one module
- [x] 1.4 The operation table - 67 operations
- [x] 1.5 Offline rules and staff exceptions
- [x] 1.6 The verification harness

Carried, unverifiable here: the eight `cmake/` files, `CMakePresets.json`,
`vcpkg.json`, `vcpkg-configuration.json`. Written, never configured. Listed in
`status.md`.

## Phase 2 - The engine's domain half — **complete**

- [x] 2.1 Record identity and time
- [x] 2.2 Quantity
- [x] 2.3 Money
- [x] 2.4 Document lifecycle
- [x] 2.5 Numbering
- [x] 2.6 Snapshots, signatures, approvals, audit
- [x] 2.7 Rights, session, capability

## Phase 3 - The engine's storage half — **4 of 7**

- [x] 3.1 Storage interfaces - `store.hpp/cpp`
- [x] 3.2 In-memory implementation - `memory_store.hpp/cpp`
- [x] 3.3 Migration runner - `migration_runner.hpp/cpp`
- [x] 3.4 The database gate and single writer - `database.*`, `writer.*`
- [x] 3.5 The outbox - pending, in flight, acknowledged, applied, conflicted,
      failed; batches of 50 to 100; a client-generated idempotency key on
      every entry so a retry cannot charge a customer twice
- [x] 3.6 Sync cursor and conflict rules - delta pull from a server-assigned
      sequence; the owner's version wins; the losing version is retained
- [~] 3.7 SQLite backing - `sqlite_store.cpp`, `sqlite_statement.*`.
      **Cannot be compiled here**, no `sqlite3.h`. Will be marked `[~]`

Still owed from 3.4: the named single-instance lock and the WAL and busy
timeout settings are Windows and SQLite work. They moved to 6.4 and 3.7 rather
than being counted here.

## Phase 4 - The twelve modules

- [x] 4.1 Module framework and explicit registration
- [x] 4.2 `administration` - people, rights grants, devices, activation
- [x] 4.3 `parties` - customers, suppliers, billing terms per customer
- [x] 4.4 `catalog` - products identified by name
- [x] 4.5 `pricing` - remembered rates, overridable; sole owner of rate resolution
- [x] 4.6 `orders` - an order may hold several jobs or none
- [x] 4.7 `receivables` - invoices, payments, manual allocation, customer credit, statements and aging; migration 15; QA approved
- [x] 4.8 `jobs` - a job may exist with no order; migration 16; QA approved
- [x] 4.9 `quotations` - quotations and revisions; migration 17; 91 checks, 0 failed
- [x] 4.10 `agreements` - agreed rates for a period, with quantity caps; migration 18; 212 checks, 0 failed
- [x] 4.11 `sourcing` - supplier directory, purchase logbook, paid or still
       owed with a settle date, and one screen listing what is owed; migration 19; 160 checks, 0 failed
- [x] 4.12 `companion` - tasks hanging off other records; migration 20; 91 checks, 0 failed
- [x] 4.13 `files` - design files by device, volume, file id and content hash; migration 21; 81 checks, 0 failed

## Phase 5 - Workflows

- [x] 5.1 Workflow framework: one transaction, one audit entry, one refusal path; migration 22; 54 checks, 0 failed
  - [x] 5.1A protocol workflow classification and engine audit persistence; direct probes, full strict gate, and independent CMake build passed
  - [x] 5.1B workflow definition and registry integration; one execution door, canonical requirements, replay, one transaction, one audit row, and one outbox row
  - [x] 5.1C migration 22, permanent harsh tests, rollback and concurrency coverage, and final framework gate
- [x] 5.2 Quotation to order: exact accepted revision snapshot, immutable provenance, one order per revision, transactional audit/outbox, replay and rollback; 21 workflow checks, 0 failed
- [x] 5.3 Order to jobs: one order line to one draft job by default, exact source-line provenance, selected subsets, frozen commercial snapshot, replay and rollback; 54 checks, 0 failed; direct jobs still need no order
- [x] 5.4 Issue an invoice: issue an existing confirmed draft without repricing; persist and atomically consume a final number from the current device's server-reserved block; migration 23; 41 checks, 0 failed
- [x] 5.5 Cancel and reissue: original retained and linked, values carried forward; active allocations released to unallocated money; no automatic reallocation; 44 checks, 0 failed
- [x] 5.6 Take a payment and allocate it, always by hand; tracking/receipt evidence optional; 42 checks, 0 failed
- [x] 5.7 Apply an agreement explicitly to a draft; consume its quantity cap on invoice issue and release it on cancellation; migration 24; 31 workflow checks, 0 failed
- [ ] 5.8 Prepare a document for approval or email; nothing sends without a
      human pressing send

## Phase 6 - Platform and the application shell

- [x] 6.1 Paths - machine-wide program data for records, per-account for cache;
      validated, created and proven writable at startup; probe seam with a fake;
      129 checks, 0 failed
- [x] 6.2 Logging with levels, rotation and a hard total size cap; one line per
      record, credentials redacted, nothing thrown when the disk misbehaves;
      716 checks, 0 failed
- [ ] 6.3 Crash handler and minidumps *(Windows, unverifiable here)*
- [ ] 6.4 Single-instance lock, named *(Windows, unverifiable here)*
- [ ] 6.5 Secrets via DPAPI, never plaintext *(Windows, unverifiable here)*
- [ ] 6.6 Connection state from `QNetworkInformation`, not server pings *(Qt)*
- [ ] 6.7 Background services - one coarse timer, about five threads, no
      service owning a thread
- [ ] 6.8 Startup order, fixed and tested

## Phase 7 - The interface *(none of this compiles here)*

- [ ] 7.1 Window and shell
- [ ] 7.2 Navigation and module visibility from activation
- [ ] 7.3 Lists
- [ ] 7.4 Forms and validation
- [ ] 7.5 Documents and print via `QPdfWriter`, avoiding QtWidgets
- [ ] 7.6 Images and the AVIF plugin

## Phase 8 - The server *(none of this compiles here)*

- [ ] 8.1 Skeleton and configuration
- [ ] 8.2 PostgreSQL and migrations
- [ ] 8.3 Identity and tokens
- [ ] 8.4 Sync endpoints, idempotency, sequence assignment
- [ ] 8.5 Media, including the AVIF conversion worker
- [ ] 8.6 The update proxy
- [ ] 8.7 Mail - prepared by the system, sent only on confirmation
- [ ] 8.8 Per-module endpoints

## Phase 9 - Build, packaging and release

- [ ] 9.1 GitHub Actions across the private repositories, App installation token
- [ ] 9.2 The ten-step Windows build
- [ ] 9.3 Automated self-signed signing
- [ ] 9.4 The updater - side-by-side, directory junction, auto-revert after two
      failed starts, blocked while the outbox is not empty
- [ ] 9.5 Podman Quadlet units
- [ ] 9.6 The release, size recorded by CI rather than estimated

---

## Decisions still owed

These are not work items. They are questions only the shopkeeper can answer,
and each one blocks something.

| # | Question | Blocks |
|---|---|---|
| D1 | **Oat++ has no current release.** 1.3.0 is the last one; 1.4.0 has been in development for years. Recommendation: pin a specific 1.4.0 commit through a vcpkg overlay port, never a branch | Phase 8 |
| D2 | The storage seam is a **typed record store**, not prepare-bind-step SQL. A SQL-shaped seam cannot be faked, and nothing above it would be testable here | already built; reversible only now |
| D3 | Is `jobs` really an Extra module? It currently requires nothing, which is unusual for something this central | 4.8 |
| D4 | **Decided:** consume an agreement quantity cap when the invoice is issued and release it when that invoice is cancelled; jobs do not consume caps | Done in 5.7 |
| D5 | Emailed approvals: the shopkeeper **marks them approved by hand** (proposed), or the system reads replies | 5.8, 8.7 |

## Work that can only happen on your machine

- [ ] Spike: printing through `QPdfWriter` without dragging in QtWidgets — before 7.5
- [ ] Spike: an AVIF image plugin verified against the pinned Qt — before 7.6
- [ ] Confirm `cmake --preset` configures at all — nothing in `cmake/` has ever run

## Standing rules that are easy to forget

- A sub-phase is done only when **all five gates** in `README.md` pass. Not four.
- Anything that cannot be verified here is named in `status.md`. Never quietly
  counted as done.
- Module count is decided on architectural merit, never on how many people
  work in the shop.
- **Verify a zip's contents before describing it.** This was got wrong once
  already: an archive was described as containing two phases and held one.
- Dependency pins are re-checked on the date in `dependencies.md`, not when
  something breaks.
