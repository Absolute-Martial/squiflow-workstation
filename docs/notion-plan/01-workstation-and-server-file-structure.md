# SquiFlow — Workstation & Server File Structure

Source page id: b5ea1a1c336447ef8c26dadef029eb7f

---

<callout icon="📌">
	**What this page is.** The actual directory and file layout to start building, with the reasoning for every level — not a tree to copy but a tree that can be defended. Four budgets govern every choice: **storage, memory, compute, and traces left on the machine.** Where a decision costs one budget to serve another, that trade is stated rather than hidden.
</callout>
## The four budgets, as enforceable rules
<table header-row="true">
<tr>
<td>Budget</td>
<td>Rule</td>
<td>What it forbids outright</td>
</tr>
<tr>
<td>**Storage**</td>
<td>The installed program folder stays small; the data folder grows only with real records. Caches and logs have hard caps that are enforced in code, not by hope</td>
<td>Unbounded logs, unbounded thumbnail caches, loose duplicated assets, shipping debug symbols to the shop</td>
</tr>
<tr>
<td>**Memory**</td>
<td>150 MB idle, already divided line by line and tested in CI</td>
<td>Loading every screen at startup, whole-table models, holding decoded images</td>
</tr>
<tr>
<td>**Compute**</td>
<td>Approximately 0% CPU at idle. Every cycle spent must be traceable to something the person asked for</td>
<td>Polling timers, background scans on a schedule, animation behind hidden screens, work on the UI thread that isn't drawing</td>
</tr>
<tr>
<td>**Traces**</td>
<td>The application touches exactly three places on the machine: its own program folder, one data folder, and one registry entry for uninstall. Nothing else</td>
<td>Scattering files across the user profile, writing to the program folder at run time, temp files that outlive their operation, any telemetry leaving the shop</td>
</tr>
</table>
Every structural decision below is checked against these four. At the end there is an accounting table showing which decision serves which budget.
---
## Step 1 — Decide what a module physically *is*, before drawing any folder
This has to come first, because a folder is not a boundary. A folder is a naming convention that a single `#include` can walk straight through. If "modular" is going to mean anything, the boundary has to be something the build system refuses to cross.
**A module is a static library target with an explicit dependency list.** Not a folder, not a namespace. The build declares that `sales` may link `engine`, `shell` and `protocol` — and nothing else. A stray include of `catalog/...` from inside `sales` then fails at link time, on the machine of the person who wrote it, on the same day.
### Why static libraries and not runtime-loadable plugins
Runtime plugins are the more obviously "modular" answer, and they are the wrong one here:
<table header-row="true">
<tr>
<td>Cost of a plugin architecture</td>
<td>Budget it violates</td>
</tr>
<tr>
<td>Every plugin is a separate library file to locate, map, relocate and resolve symbols for at startup</td>
<td>Compute and storage — and on a spinning disk, ten extra file opens at launch is measurable in the cold-start budget</td>
</tr>
<tr>
<td>Cross-library calls cannot be inlined, and whole-program optimisation stops at the boundary</td>
<td>Compute, and binary size grows because dead code cannot be eliminated across the gap</td>
</tr>
<tr>
<td>Every plugin is a loose file on disk that can be replaced, corrupted or missed by the installer</td>
<td>Traces, and it directly weakens the signing decision — a signed executable that loads unsigned files beside it is only partly signed</td>
</tr>
<tr>
<td>Interfaces must stay binary-compatible forever, which makes ordinary refactoring expensive</td>
<td>Not a budget, but a permanent tax on a codebase with one developer</td>
</tr>
</table>
And the thing plugins buy — third parties dropping in functionality without a rebuild — is **already a locked non-feature**. There is no extension marketplace and there never will be one.
So: **modularity here means source-replaceable, not runtime-loadable.** Adding a module means adding a directory and one line to a list, then rebuilding. Removing one means deleting a directory and that line, and the binary genuinely shrinks. That is a stronger form of modularity than plugins for this project, because the compiler proves the boundaries instead of a convention describing them.
---
## Step 2 — The top level
```plain text
squiflow-workstation/
  CMakeLists.txt              top-level build, nothing but structure
  CMakePresets.json           named configurations so nobody types flags by hand
  vcpkg.json                  dependencies, pinned
  vcpkg-configuration.json    the registry baseline, pinned
  .clang-format               formatting is automated, never discussed
  .clang-tidy                 the static analysis rule set
  .editorconfig
  cmake/                      build logic, kept out of every CMakeLists
  external/                   submodules only, never vendored source
  src/                        all product code
  tests/                      mirrors src/
  packaging/                  what turns a build into a deliverable
  docs/                       decisions that belong beside the code
```
**Reasoning for the pieces that are not obvious:**
- **`cmake/`**** exists so that individual ****`CMakeLists.txt`**** files stay three lines long.** Warning flags, hardening flags, sanitizer configuration, and the module-declaration helper each live in exactly one file. When a hardening flag is added, it is added once and every target gets it. The alternative — flags copied into thirty build files — guarantees that the thirty-first target is built without them, silently.
- **`external/`**** contains submodules and nothing else.** No copied third-party source. Copied source has no version, so it never gets updated, and eventually nobody knows what it was.
- **`tests/`**** mirrors ****`src/`**** exactly.** A mirrored tree means the question "where is the test for this file" has one answer, forever, and a missing test is visible as a missing file.
- **`docs/`**** beside the code**, holding the short decision notes that a future reader needs when the reasoning is not obvious from the code. Long-form planning stays in Notion; the code repository holds the terse version that cannot drift out of sync with the files it describes.
---
## Step 3 — `src/app`: the only place allowed to know everything
```plain text
src/app/
  main.cpp                  twenty lines: construct, run, return
  application.hpp/.cpp      lifetime, startup order, shutdown order
  composition_root.cpp      the ONE file that knows every module by name
  module_registry.hpp/.cpp  what modules register into
  single_instance.hpp/.cpp  the named lock
  crash_handler.hpp/.cpp    minidump, breadcrumb log, restart offer
  logging.hpp/.cpp          levels, rotation, size cap
  generated/version.hpp     written by the build, never edited
```
**The composition root is the important idea here.** Somewhere, something must know that ten modules exist. If that knowledge is scattered, the modules are not really independent. So it is deliberately concentrated into **one file** — the only file in the codebase that includes every module's header. Everything else depends downward.
The practical test: deleting a module should mean deleting its directory and **one line** in `composition_root.cpp`. If it means hunting through the shell, the navigation, the permission screen and the sync router, the boundary has leaked.
**Startup order is explicit and written down here**, not implied by construction order scattered across files. Paths, then logging, then the crash handler, then the database, then migrations, then the integrity check, then identity, then modules, then the shell, then the window. Nothing before the crash handler is protected, so the crash handler goes early. Nothing touching the database can be constructed before migrations have run, so modules come after.
**`logging.cpp`**** owns the trace budget.** Levels default to warnings and above; files rotate; total log size is capped in code at a few megabytes. A log that grows without limit is the most common way a small application quietly eats a disk.
---
## Step 4 — `src/platform`: a layer even though there is only one platform
```plain text
src/platform/
  paths.hpp             where data, cache, logs and secrets live
  paths_win.cpp
  secrets.hpp           store and retrieve a credential
  secrets_dpapi.cpp
  network_state.hpp     online, metered, offline
  network_state_win.cpp
  power.hpp             sleep and wake notification
  power_win.cpp
  updater.hpp           hand off to the updater process and exit
  updater_win.cpp
  testing/              fake implementations used by tests
```
A reasonable objection: the target is Windows only, so why abstract at all? Three concrete reasons, none of them about portability:
1. **Windows headers are contagious.** Included directly in business code, they drag in macros that collide with ordinary identifiers and make every file slower to compile. Confining them to a handful of `*_win.cpp` files keeps the rest of the codebase clean and the build fast — a compute budget served every single build.
2. **None of these can be tested without a fake.** Sync behaviour has to be tested against a pretend network state; the secret store has to be tested without touching the real credential store. Without the interface there is no seam, and those tests simply don't get written.
3. **It is where the honest platform limits get documented.** "A running executable cannot be replaced" belongs as a comment in `updater_win.cpp`, next to the code that works around it.
The cost is one extra indirection on a handful of calls that happen a few times per session. That is nothing.
---
## Step 5 — `src/engine`: the mechanisms every module borrows
```plain text
src/engine/
  storage/
    database.hpp/.cpp        owns THE connection; nobody else opens one
    writer.hpp/.cpp          the single-writer gate; all writes queue here
    statement.hpp/.cpp       prepared statement cache
    migration_runner.*       forward-only, transactional, versioned
    migrations/              0001_core.sql, then one per module
    integrity.*              startup check, and the honest failure path
  records/
    identity.hpp             record id, created/updated, soft delete
    audit.*                  who did what, on which device
    numbering.*              reserved blocks, gap tolerance, burned numbers
    lifecycle.*              draft → issued → cancelled → replaced
    snapshot.*               freeze values at the moment of agreement
  sync/
    outbox.*                 pending → in flight → acknowledged → applied
    cursor.*                 the server sequence position
    orchestrator.*           exactly one sync at a time
    transport.*              websocket with HTTP fallback and backoff
    codec_msgpack.*          the decoder that gets fuzzed
    conflict.*               version compare, owner wins, loser retained
  identity/
    session.*                who is signed in, on which device
    rights.hpp               generated — see step 6
    capability.*             may this operation run right now, offline?
  files/
    object_ref.*             hash, size, format, where it lives
    thumbnail_cache.*        bounded, on disk, evicted by least-recent use
  services/
    service.hpp              start, stop, and a reason it is allowed to run
    supervisor.*             owns every service and every thread
    triggers.*               event, OS notification, idle, coalesced tick
    idle_detector.*          is the person actually away from the machine
    health.*                 last run, last outcome, failure budget
```
**Why one connection and one writer, as a structural rule rather than a guideline.** `database.cpp` is the only file that opens the database. Everything else receives a handle. This is what makes the single-writer rule enforceable instead of aspirational, and it is what prevents the `SQLITE_BUSY` class of failure that shows up only under load, at the counter, in front of a customer.
**Why ****`records/`**** is separate from ****`storage/`****.** These are the four mechanisms every module reuses — identity, lifecycle, numbering, snapshots. Written once, they behave identically in quotations, invoices and agreements, which is exactly the promise the feature specification makes. Written per module, they drift, and by the third module they disagree about what "cancelled" means.
**`capability.*`**** is where the two-device rule lives.** One function answers: given this operation, this person's rights, this device, and the current connection state, may it run? The staff device's offline exception — counter sales and receipts only — is **data in one table, evaluated by one function**, not a condition scattered through ten screens. When the rule changes, one file changes.
---
## Step 5a — Background services, and what each one costs when nothing is happening
Background services are where a small desktop application usually becomes a heavy one. Not through one expensive thing, but through fifteen cheap things that each wake up independently, and together mean the machine never actually goes idle.
### The three rules that come before the list
**1. No service owns a thread.** The natural design gives each service its own thread, and it is the wrong one. Every Windows thread reserves a megabyte of stack address space, adds a scheduler entry, and — worse — wakes on its own timetable. Fifteen services with fifteen timers means the CPU is woken fifteen times a period regardless of how little each one does. Instead: **one supervisor, a two-thread worker pool, and one thread for network I/O.** Total thread count for the whole application stays around five on a four-core machine, and a service is a function that runs on a pool thread when its trigger fires.
**2. A timer is the trigger of last resort.** In order of preference:
<table header-row="true">
<tr>
<td>Trigger</td>
<td>Used by</td>
<td>Idle cost</td>
</tr>
<tr>
<td>**An event in the application** — a record was saved, a screen was opened</td>
<td>Outbox drain, attention evaluation, media upload</td>
<td>Zero. Nothing happens, nothing runs</td>
</tr>
<tr>
<td>**An event from the server** — a message arrives on the open connection</td>
<td>Applying changes made on the other device</td>
<td>Zero. The connection is already open; no polling</td>
</tr>
<tr>
<td>**An OS notification** — network state changed, machine is sleeping or waking</td>
<td>Transport supervisor, token renewal, resync on wake</td>
<td>Zero. Windows tells us; we never ask</td>
</tr>
<tr>
<td>**Idle detection** — no input for several minutes and no pending work</td>
<td>Database maintenance, cache eviction, log pruning, backup</td>
<td>Zero while the person is working, which is the point</td>
</tr>
<tr>
<td>**One coalesced tick**, the only recurring timer in the program</td>
<td>Heartbeat, number-block check, update check — all riding the same tick</td>
<td>One wake per minute, coarse, allowed to drift so it merges with other system wakes</td>
</tr>
</table>
The single most important line in that table is the last one: **there is exactly one recurring timer in the entire application**, it is coarse rather than precise, and every periodic service rides it instead of creating its own. A coarse timer lets the operating system nudge the wake-up to coincide with wake-ups it was already going to perform, which is the difference between a laptop-hostile application and one that lets the CPU stay in a low-power state.
And the timer resolution of the system is **never raised**. Requesting a finer system-wide timer is a well-known way to increase idle power draw across the whole machine, for no benefit to an application that has nothing to do sixteen times a second.
**3. Nothing heavy runs while the person is working — because the disk is spinning rust.** On an HDD, maintenance is not a background cost, it is a foreground cost paid by whoever is typing. Every maintenance service is gated on genuine idleness, and every one of them is interruptible the moment input resumes.
### The service registry
<table header-row="true">
<tr>
<td>Service</td>
<td>Trigger</td>
<td>What it must never do</td>
</tr>
<tr>
<td>**Transport supervisor** — keeps one connection open, reconnects with backoff, downgrades to plain requests on a weak link</td>
<td>Network-state notification; wake from sleep; connection loss</td>
<td>Never reconnect on a fixed timer. Backoff runs 5s, 10s, 20s, 40s to a five-minute ceiling, and resets on a real network-state change, not on hope</td>
</tr>
<tr>
<td>**Outbox drain** — sends pending operations in dependency order, 50–100 per batch</td>
<td>A record was saved; the connection came back; sync finished with more work waiting</td>
<td>Never run twice at once. The orchestrator holds a single permit — this is the rule that prevents duplicate invoices more than anything else in the design</td>
</tr>
<tr>
<td>**Change applier** — applies what the other device did</td>
<td>Server message arrives; a catch-up pull after reconnect</td>
<td>Never apply while the person has an unsaved edit to the same record open. It queues and the screen shows that something changed</td>
</tr>
<tr>
<td>**Heartbeat and presence**</td>
<td>The coalesced tick, only when connected</td>
<td>Never keep ticking while offline. An offline heartbeat is pure waste</td>
</tr>
<tr>
<td>**Number-block top-up** — keeps a reserve of document numbers so the counter never stalls</td>
<td>The tick, and only when the remaining reserve falls below a threshold</td>
<td>Never fetch a block it doesn't need. Unused blocks become permanent gaps in the numbering</td>
</tr>
<tr>
<td>**Media upload worker** — bill photos out to the server, where AVIF conversion happens</td>
<td>A photo was attached; connection returned</td>
<td>Never encode on this machine, never block the save. The record is saved with the photo pending; the upload catches up</td>
</tr>
<tr>
<td>**Attention evaluator** — overdue payments, expiring agreements, quantity caps nearly reached, sync stuck</td>
<td>Data changed; day rollover on the tick</td>
<td>Never re-scan everything. It evaluates only what changed, plus a once-daily sweep for date-driven items</td>
</tr>
<tr>
<td>**Token renewal**</td>
<td>Scheduled from the token's own expiry, plus a check on wake</td>
<td>Never let a token expire mid-operation. Renewal happens well before expiry, and always after sleep, where the clock jumped</td>
</tr>
<tr>
<td>**Update check**</td>
<td>The tick, at most once a few hours, and on launch</td>
<td>Never download or prompt while the outbox is non-empty, and never install anything on its own — notify, and wait for the click</td>
</tr>
<tr>
<td>**Database maintenance** — WAL checkpoint, incremental vacuum, statistics refresh</td>
<td>Idle for several minutes with an empty write queue</td>
<td>Never a full rebuild in one shot. Incremental, a bounded slice per run, abandoned instantly when input resumes</td>
</tr>
<tr>
<td>**Cache janitor** — thumbnails and temporary files back under their cap</td>
<td>Idle; also when the cap is exceeded</td>
<td>Never walk the whole cache directory. Sizes are tracked as entries are written, so eviction is a query, not a scan</td>
</tr>
<tr>
<td>**Log pruning**</td>
<td>Rotation, which is size-driven</td>
<td>Never a scheduled job. A log that is not being written does not need attention</td>
</tr>
<tr>
<td>**Local snapshot backup** — the owner's device only</td>
<td>Idle, once a day at most</td>
<td>Never on the staff device, and never while a sync is running. A copy taken mid-sync captures a half-applied state</td>
</tr>
<tr>
<td>**Sleep and wake listener**</td>
<td>The OS power notification</td>
<td>Never let every service fire at once on wake. They are staggered — connection first, then token, then catch-up, then everything else</td>
</tr>
</table>
Fourteen services, and **when the shop is quiet and nobody has touched the machine, the recurring cost is one coarse wake per minute that finds nothing to do.** Everything else is asleep until something real happens.
### What the supervisor enforces, so that each service doesn't have to
- **Cancellation is mandatory.** Every service receives a cancellation signal and must respond within a bounded time. Closing the application waits briefly, then abandons cleanly — never a hung window that has to be killed, and never a half-written transaction, because the database work is transactional regardless of where it stops.
- **A failure budget, not infinite retries.** Repeated failure stops the service and raises an **attention item** the person can see. A service silently retrying forever is how a shop discovers a week later that nothing has synced.
- **No service holds state between runs.** Buffers are taken from a shared pool and returned. This is what keeps the background half of the application inside a few megabytes rather than growing quietly all day.
- **Every service reports last run, last outcome, and next trigger** into one small table, shown on a diagnostics screen. If the machine is doing something, it must be possible to point at what.
- **Nothing touching the network starts before sign-in.** A locked screen means an idle machine.
- **The staff device runs a smaller set** — no backup, no maintenance-heavy work. Its job is the counter.
---
## Step 6 — One module's internal layout, identical for all twelve
This shape repeats exactly. Adding the thirteenth module is copying a directory.
```plain text
src/modules/sales/
  CMakeLists.txt            three lines, uses the helper from cmake/
  module.cpp                registration: screens, palette, rights, rules
  operations.def            the declaration list — see below
  domain/
    sale.hpp                the types; no database, no Qt, no network
    invariants.hpp          the rules that must always hold
  service/
    sales_service.hpp/.cpp  every business rule lives here
  data/
    sales_repository.hpp/.cpp   SQL, and nothing but SQL
    migrations/0006_sales.sql
  sync/
    sales_sync.cpp          what syncs, in what order, replay behaviour
  view/
    sale_list_model.hpp/.cpp    virtualized, windowed
    sale_form.hpp/.cpp          what QML binds to
  tests/
```
**The layering rule, top to bottom: ****`view`**** → ****`service`**** → ****`data`**** → ****`domain`****.** Never upward, never sideways. `domain/` includes nothing from the project except other domain headers, which means the rules can be tested in microseconds with no database and no Qt — and tests that fast actually get run.
### `operations.def` — one declaration, four consumers
The single highest-leverage file in a module. It lists every operation the module offers, once:
```plain text
SQF_OPERATION(sale_create,        right_sale_create,   Synchronizable, OfflineAllowed)
SQF_OPERATION(sale_void,          right_sale_void,     Synchronizable, OnlineRequired)
SQF_OPERATION(receipt_print,      right_receipt_print, LocalOnly,      OfflineAllowed)
```
Four things are generated from that one list:
1. **The rights registry**, so a new operation appears in the owner's permission checklist with no UI change.
2. **The offline capability table**, so the staff device's read-only rule needs no per-screen code.
3. **The sync router**, so nothing is forgotten or double-registered.
4. **A completeness test**, which fails the build if an operation exists in code but not in the list.
The reasoning: these four things must always agree. If they are written by hand in four places, they will disagree — and the failure mode is a permission that silently isn't enforced, which is the worst possible bug in a system that handles money.
---
## Step 6a — When something deserves to be a module
Module count is not a matter of taste and it is not a function of team size. A module either passes these tests or it is something else wearing a module's clothes.
### The five tests
1. **Ownership.** It owns at least one entity that no other module may write. If nothing is exclusively its own, it is not a module — it is a **mechanism**, and mechanisms live in `engine/`.
2. **One-way dependency.** Nothing it depends on depends back on it. A cycle is not a sign of two modules that are too small; it is a sign the cut is in the wrong place.
3. **Deletability.** Remove it and the build stays coherent. Features disappear; nothing breaks. If removing it breaks unrelated things, it is a layer, not a module.
4. **Its own surface.** Its own rights, its own screens, its own migrations. If it has none of these, it belongs to its neighbour.
5. **Independent change.** Changes to it do not systematically force simultaneous changes elsewhere. If two things always change together, they are one thing.
A candidate that fails test 1 is a mechanism. One that fails test 2 has been cut along the wrong line. One that fails test 5 should be merged. **Nothing here refers to how many developers or users exist**, and it should not.
### The thirteen, tested
<table header-row="true">
<tr>
<td>Candidate</td>
<td>Verdict</td>
<td>Reasoning</td>
</tr>
<tr>
<td>`parties`</td>
<td>**Module**</td>
<td>Owns the party. Almost everything reads it; it reads nothing. The cleanest boundary in the system</td>
</tr>
<tr>
<td>`catalog`</td>
<td>**Module**</td>
<td>Owns product identity by name. Knows nothing about customers, rates or documents — and must be kept that way</td>
</tr>
<tr>
<td>`pricing`</td>
<td>**Module, and separate from ****`catalog`**</td>
<td>A rate is not an attribute of a product. It is a relation between a product, a party, an agreement and a moment in time. Folding it into `catalog` would drag customer and contract knowledge into a module that must not have any. **Rate resolution has exactly one owner: this module**, even though agreements store agreed rates</td>
</tr>
<tr>
<td>`quotations`</td>
<td>**Module**</td>
<td>Own document, own revision rules, own lifecycle. Conversion to an order is a workflow, not a dependency</td>
</tr>
<tr>
<td>`orders`</td>
<td>**Module**</td>
<td>Owns the order document and its lifecycle. It may cause jobs to exist, but it does not own them</td>
</tr>
<tr>
<td>`jobs`</td>
<td>**Module** — resolved, stays separate</td>
<td>An order can produce several jobs, and a job can exist with no order at all. Two entities, two lifecycles, a link that is optional in one direction. Merging them would force a job that never had an order to carry an empty order around it</td>
</tr>
<tr>
<td>`approvals`</td>
<td>**Not a module — a mechanism**</td>
<td>**Fails test 1.** It owns nothing. An approval always belongs to the thing being approved — a quotation, a job, a delivery, an agreement. Left as a module, every other module would have to depend on it, breaking test 2 across the whole system. It moves to `engine/records/approval.*` and `engine/records/signature.*`, beside lifecycle and numbering, where it can be used by anything without creating a dependency</td>
</tr>
<tr>
<td>`agreements`</td>
<td>**Module**</td>
<td>Owns the contract document, its periods and its quantity caps. Depends on pricing and parties; neither depends back</td>
</tr>
<tr>
<td>`receivables`</td>
<td>**Module** — the strongest boundary of all</td>
<td>Owns immutable financial records. Its invariants must never be reachable from anywhere else. If any boundary in this system is worth enforcing at the build level, it is this one</td>
</tr>
<tr>
<td>`sourcing`</td>
<td>**Module**</td>
<td>Owns suppliers and purchase records. Nothing else writes them</td>
</tr>
<tr>
<td>`companion`</td>
<td>**Module**</td>
<td>Owns tasks. It points at other records through a generic reference type in `engine/records`, so it depends on no module in particular — which is what keeps it legal</td>
</tr>
<tr>
<td>`files`</td>
<td>**Module**</td>
<td>Owns design-file identity and the search index. The object-reference primitive stays in `engine/files`; the domain stays here</td>
</tr>
<tr>
<td>`administration`</td>
<td>**Module**</td>
<td>Owns people, rights, devices and settings</td>
</tr>
</table>
**Final: twelve modules, plus one mechanism correctly demoted into ****`engine/`****.** No merges. The only change to the original list is that `approvals` was never a module to begin with.
### `orders` and `jobs` — resolved, and what the answer forces
**Stated fact: an order can have several jobs, and a job can exist with no order.** Two entities, two lifecycles. They stay separate, and three consequences follow that would have been wrong under a merge:
1. **A job carries its own number**, from its own reserved block, independent of any order number. A job that was never ordered still has to be identifiable.
2. **The link is optional and one-way.** A job may reference the order that caused it; an order never has to have jobs. Anything that assumes a job has an order is a bug, and the type system should say so rather than a comment.
3. **Creating jobs from an order is a workflow**, not a method on either module. It lives in `src/workflows/`, alongside quote-to-order — which is precisely the layer that attack 1 introduced.
This also settles a feature question structurally: **a job taken straight at the counter, with no order document, is not a special case.** It is the ordinary shape, which is what the skippable-chain decision always implied.
### The cost that made merging tempting, and its actual fix
Every module costs a build file, a registration line, a migration lane and a test target. That is real, and it is **typing**, not architecture. The correct response is to make a module cheap to create, not to create fewer of them:
- A `templates/module/` directory in the repository, copied by one script that renames placeholders.
- `cmake/SquiflowModule.cmake` reducing the build file to three lines.
- The eight-step add checklist, which is short precisely because the template does the rest.
**And at run time, the module count costs nothing at all.** Static libraries with whole-program optimisation produce the same binary whether the code was organised into six directories or thirteen. Module count is purely a question of where source lives — so it should be answered purely by where responsibility lives.
---
## Step 6b — Core modules and extra modules
Every module is compiled into the one binary. What differs between tiers is **whether it can be switched off**, and that is runtime data, not a build variant.
<table header-row="true">
<tr>
<td>Tier</td>
<td>Definition</td>
<td>May depend on</td>
</tr>
<tr>
<td>**Core**</td>
<td>Permanent. Cannot be deactivated. Present in every installation, forever</td>
<td>Engine, and other core modules only</td>
</tr>
<tr>
<td>**Extra**</td>
<td>Optional. Activated per shop. Its screens, rights and operations appear only when it is on</td>
<td>Engine, core modules, **and other extra modules — under the three conditions below**</td>
</tr>
</table>
### The one rule that keeps the tiers honest
**A core module may never depend on an extra.** If it did, switching that extra off would break something unswitchable, and "core" would mean nothing. This is checked at build time, not by discipline — the module declaration names its tier and its dependencies, and the build fails on a violation.
Core must therefore be **closed under dependency**: everything core depends on is also core.
### Which modules are core
The criterion, so this is decided by rule rather than by opinion: **a module is core if the shop cannot complete a single counter sale without it, or if a core module depends on it.**
<table header-row="true">
<tr>
<td>Module</td>
<td>Tier</td>
<td>Why</td>
</tr>
<tr>
<td>`administration`</td>
<td>**Core**</td>
<td>People, rights and devices. Nothing runs without a signed-in person</td>
</tr>
<tr>
<td>`parties`</td>
<td>**Core**</td>
<td>Everything that is billed is billed to someone</td>
</tr>
<tr>
<td>`catalog`</td>
<td>**Core**</td>
<td>A sale needs something being sold, identified by name</td>
</tr>
<tr>
<td>`pricing`</td>
<td>**Core**</td>
<td>A sale needs a rate, and `receivables` depends on it</td>
</tr>
<tr>
<td>`orders`</td>
<td>**Core**</td>
<td>The counter sale itself</td>
</tr>
<tr>
<td>`receivables`</td>
<td>**Core**</td>
<td>Money in and money owed. A shop that cannot take payment is not open</td>
</tr>
<tr>
<td>`jobs`</td>
<td>Extra — **confirm**</td>
<td>By the criterion, a counter sale can complete without a job ticket, so it is extra. For a print shop this may well be wrong, and it is a business decision, not a structural one</td>
</tr>
<tr>
<td>`quotations`</td>
<td>Extra</td>
<td>A shop can operate on counter sales alone</td>
</tr>
<tr>
<td>`agreements`</td>
<td>Extra</td>
<td>Depends on `pricing` and `parties`, both core. Clean</td>
</tr>
<tr>
<td>`sourcing`</td>
<td>Extra</td>
<td>Suppliers and purchase records; independent of selling</td>
</tr>
<tr>
<td>`companion`</td>
<td>Extra</td>
<td>Tasks and reminders; nothing depends on it</td>
</tr>
<tr>
<td>`files`</td>
<td>Extra</td>
<td>Design-file identity and search. Substantial, and entirely optional to a shop that does not need it</td>
</tr>
</table>
Six core, six extra — and the core set is closed: nothing in it points at anything outside it.
### Extras depending on extras — allowed, under three conditions
At link time this costs nothing, because everything is compiled in regardless. The entire difficulty is **activation**: if `B` depends on `A` and someone switches `A` off while `B` is on, `B` is calling into a module that is supposed to be absent. Three conditions make that impossible rather than merely unlikely:
1. **The dependency is declared, not discovered.** A module names its required modules in its declaration. Nothing may reach into a module it did not declare, and the build enforces it — the same mechanism that already stops a module reaching sideways.
2. **The graph is acyclic, checked at build time.** `A` requiring `B` requiring `A` is a build failure, not a runtime surprise. Without this check, tiers become a package manager, and a package manager is exactly what was rejected in step 1.
3. **Activation is computed, never chosen item by item.** Switching a module off switches off everything that transitively requires it, and the confirmation says so in plain words before anything happens: *"Turning off agreements will also turn off contract invoicing."* Nothing is ever left half-active.
### What deactivation means — and what it must never mean
- **It hides features. It never deletes data.** Records belonging to an inactive module stay in the database, keep syncing, and reappear intact when it is switched back on.
- **A module in use cannot be switched off.** If an active record depends on it — an invoice priced from an agreement, a job attached to an order — the switch is refused with the reason and an example. One query per dependent module; cheap, and it prevents the only serious failure mode here.
- **Activation is shop-wide, not per device.** It is a synced setting. Two devices disagreeing about which modules exist would be a genuinely confusing bug.
- **Rights survive deactivation.** Grants for an inactive module's operations remain stored and simply do not appear. Reactivating restores the previous permission state — the same rule as retired right identifiers.
- **Screens for an inactive module are never constructed**, so an inactive module costs nothing but the disk space of code that is never touched and therefore never paged in.
### What this still does not become
A tier system is not an extension mechanism. **Third parties still cannot add modules**, there is no marketplace, and nothing is loaded at run time. Core and extra describe **which of our own modules a given shop has switched on** — nothing more. That boundary is what keeps this a two-line settings lookup rather than a dependency resolver.
---
## Step 7 — The UI tree, and why QML lives inside the binary
```plain text
src/ui/
  Main.qml                  window, nothing else
  shell/                    navigation, palette, attention, dialogs
  theme/                    colours, spacing, typography — one place
  common/                   list, form field, empty state, error banner
  sales/  parties/  catalog/  ...   one folder per module, same names
```
**QML is compiled and embedded into the executable**, not shipped as loose files. Reasoning across three budgets at once:
- **Compute:** compiled ahead of time, so startup does not parse text. This is one of the largest single contributors to the cold-start budget.
- **Traces:** no loose script files sitting in the program folder to be edited, corrupted, or flagged by antivirus.
- **Signing:** embedded content is covered by the executable's signature. Loose QML beside a signed exe is unsigned code that a signed program executes.
The cost: changing a screen requires a rebuild. For a one-developer project with a fast incremental build, that is not a real cost.
**Screens are loaded on navigation and unloaded when hidden.** Not an optimisation to add later — a structural rule from the first screen, because retrofitting it means rewriting every screen's assumptions about its own lifetime.
**`theme/`**** exists so the white-label requirement is one directory**, not a search-and-replace across the interface.
---
## Step 8 — Registration, and the trap that will otherwise cost a day
Modules register themselves into the registry so the shell needs no knowledge of them. The natural way to write this is a self-registering object in each `module.cpp`.
**In a static library, that silently does not work.** The linker only pulls in object files that something references. A module registering itself in a global constructor is referenced by nobody, so it is never pulled in, and the module vanishes from the build with **no error and no warning** — it simply isn't there at run time. This is one of the oldest traps in C++ and it presents as "my feature disappeared for no reason".
Two acceptable fixes, and the choice matters:
<table header-row="true">
<tr>
<td>Fix</td>
<td>Verdict</td>
</tr>
<tr>
<td>Force the linker to include the whole archive for every module library</td>
<td>**Rejected.** It works, but it also drags in genuinely unused code and defeats the dead-code elimination that keeps the binary small — a storage cost paid to avoid writing one list</td>
</tr>
<tr>
<td>**An explicit list in ****`composition_root.cpp`**** that names each module's register function**</td>
<td>**Adopted.** One visible line per module. It is the thing that makes the module list readable in one place, it keeps dead-code elimination working, and the "add a module" checklist already includes that line</td>
</tr>
</table>
The cost is one line of manual work per module. The benefit is that nothing about the system is invisible — and a completeness test can compare the registered list against the module directories and fail if someone forgets.
---
## Step 9 — Build files, and what each one prevents
<table header-row="true">
<tr>
<td>File</td>
<td>What it exists to prevent</td>
</tr>
<tr>
<td>`CMakePresets.json`</td>
<td>Configurations that exist only in one person's shell history. Debug, release, sanitizer and CI configurations are named and identical everywhere</td>
</tr>
<tr>
<td>`vcpkg.json` plus a pinned baseline</td>
<td>The build that worked last month failing this month because a dependency moved. Also what makes the binary cache effective — unchanged pins mean nothing recompiles</td>
</tr>
<tr>
<td>`cmake/SquiflowModule.cmake`</td>
<td>Modules drifting apart. One helper declares a module: sources, its allowed dependencies, its warnings, its tests. A module cannot quietly grant itself an extra dependency, because the helper is where dependencies are named</td>
</tr>
<tr>
<td>`cmake/Warnings.cmake`, `Hardening.cmake`</td>
<td>A target built without the safety flags. Applied centrally, so a new target cannot miss them</td>
</tr>
<tr>
<td>`cmake/Version.cmake.in`</td>
<td>A version typed into three files and wrong in two. The tag flows into a generated header, the installer, the manifest and the about screen</td>
</tr>
</table>
---
## Step 10 — The server, mirrored on purpose
```plain text
squiflow-server/
  external/protocol/
  src/
    main/            bootstrap, configuration, health, graceful shutdown
    infrastructure/  postgres pool, migrations, object store, secrets, jobs
    identity/        authentication, right enforcement, audit writer
    sync/            sequence assignment, idempotency ledger, batch apply
    updates/         github release poller, asset cache, manifest serving
    media/           the AVIF conversion worker
    modules/         parties/ catalog/ pricing/ sales/ ... same names, same order
      sales/
        endpoint.cpp      decode, authenticate, delegate, encode — no rules
        service.*         the rules, mirroring the workstation's service
        repository.*      SQL only
        migrations/
        tests/
  migrations/        numbered, forward-only
  deploy/            Containerfile, Quadlet units, backup scripts
  tests/
```
**The module names match on both sides, deliberately.** `sales` on the workstation talks to `sales` on the server through `sales` messages in the protocol. Changing one feature means opening three directories with the same name. This sounds trivial and it is the difference between a codebase one person can hold in their head and one they cannot.
**Rules are duplicated on both sides, and that is intentional, not an accident.** The workstation enforces a rule so the person gets an immediate answer; the server enforces it again because a client-side check is advice, not a rule. The protocol's contract tests are what keep the two from drifting — which is why the tests live in the protocol repository rather than in either consumer.
**`media/`**** is a background worker, not part of request handling.** AVIF conversion is the most expensive thing the server ever does, and it must never sit inside a request the counter is waiting on.
---
## Step 11 — What exists on the shop machine, and nothing more
The trace budget, made concrete:
```plain text
C:\Program Files\SquiFlow\
  current\                     a link to the active version folder
  versions\1.2.0\              executable, Qt libraries, plugins
  versions\1.1.0\              the previous version, kept for rollback
  squiflow-updater.exe         swaps the link, then exits

%LOCALAPPDATA%\SquiFlow\
  data\squiflow.db             plus its journal files
  cache\thumbnails\            capped; safe to delete at any time
  logs\                        rotating, a few megabytes total, capped
  secrets\                     DPAPI-protected, never plaintext
```
The rules that keep it this way:
- **The program folder is never written to at run time.** Program files and data files are separate, always. Writing into the program folder breaks with restricted permissions, confuses backups, and makes the update swap unsafe.
- **Exactly two version folders are kept** — current and previous. Older ones are deleted after a successful launch. Version folders accumulating is the classic way a self-updating application fills a disk.
- **`cache/`**** is disposable by definition.** Deleting it while the application is closed must be harmless, and if that ever stops being true, something load-bearing has been put in the wrong folder.
- **One registry entry**, for uninstall. No settings in the registry — settings live in the database, where the backup already covers them.
- **Uninstall removes the program folder and leaves the data folder**, with a plainly worded checkbox to remove data too. Deleting a shop's records because someone uninstalled an application would be unforgivable.
- **Nothing is sent anywhere.** No telemetry, no usage reporting, no crash upload without the person pressing a button. The crash dump is written locally and offered.
---
## Budget accounting — which decision serves which
<table header-row="true">
<tr>
<td>Decision</td>
<td>Storage</td>
<td>Memory</td>
<td>Compute</td>
<td>Traces</td>
</tr>
<tr>
<td>Static module libraries, no runtime plugins</td>
<td>Dead code eliminated across the whole program</td>
<td>No per-library load overhead</td>
<td>No startup symbol resolution; cross-module inlining</td>
<td>No loose libraries beside the executable</td>
</tr>
<tr>
<td>QML compiled into the binary</td>
<td>One file instead of hundreds</td>
<td>No parse tree held at startup</td>
<td>Largest single cold-start saving</td>
<td>Nothing loose to tamper with; covered by the signature</td>
</tr>
<tr>
<td>Screens loaded on demand, unloaded when hidden</td>
<td>—</td>
<td>The main reason 60 MB for the interface is achievable</td>
<td>Nothing runs behind a hidden screen</td>
<td>—</td>
</tr>
<tr>
<td>Windowed list models</td>
<td>—</td>
<td>100,000 rows cost the same as 100</td>
<td>Queries stay small and indexed</td>
<td>—</td>
</tr>
<tr>
<td>One database connection, one writer</td>
<td>—</td>
<td>One page cache, capped once</td>
<td>No lock contention, no retry storms</td>
<td>Predictable journal files, nothing stray</td>
</tr>
<tr>
<td>`operations.def` generating four things</td>
<td>—</td>
<td>—</td>
<td>—</td>
<td>Permissions cannot silently fail to be enforced</td>
</tr>
<tr>
<td>Capped logs, capped caches, two version folders</td>
<td>The disk cannot fill quietly</td>
<td>—</td>
<td>—</td>
<td>The footprint has a known maximum</td>
</tr>
<tr>
<td>Event-driven only, no polling</td>
<td>—</td>
<td>Caches go cold and are released</td>
<td>Idle means idle; no fan noise</td>
<td>No background disk activity</td>
</tr>
</table>
---
## Step 12 — The order to actually build it
Not the module order — the **file** order. The goal of the first week is one thin vertical slice that proves the whole shape works, before ten modules are written against a shape that turns out to be wrong.
1. **The skeleton that does nothing.** Top-level build, `cmake/` helpers, presets, vcpkg pins, an empty window. Prove the build works in GitHub Actions on day one, not week six — a pipeline discovered late is a pipeline that reshapes the code.
2. **`app/`**** and ****`platform/`****.** Paths, logging with its cap, single instance, crash handler. Everything after this is protected and observable.
3. **`engine/storage`****.** One connection, the writer gate, the migration runner, the integrity check, and a test that opens, migrates, writes, closes and reopens.
4. **`engine/records`****.** Identity, audit, lifecycle, numbering. Tested with no module in existence, because these are the mechanisms every module will assume.
5. **One module end to end — ****`parties`****.** Chosen because it is the simplest thing with a real screen: `operations.def`, domain, service, repository, migration, list model, QML, registration, tests. **This is the template.** Everything learned here is paid once instead of ten times.
6. **`shell/`****.** Navigation and the command palette, driven by whatever `parties` registered. If the shell needs to know the word "parties" anywhere, the registration mechanism is wrong and it gets fixed now, while there is one module and not ten.
7. **`engine/sync`**** plus the server's ****`sales`****/****`parties`**** counterpart.** The first real round trip: outbox, cursor, idempotency, the conflict tiebreak. Done with two modules rather than ten, so mistakes are cheap.
8. **Packaging, updater, self-signing.** Produce an installable, updatable, signed artefact **before** the feature waves begin — because delivery problems discovered at the end reshape the code, and delivery problems discovered early only reshape a script.
9. **Then the waves**, module by module, each one a copy of the `parties` shape.
The principle: **every structural risk is taken in the first two weeks, while the codebase is small enough to change its mind.**
---
## Appendix A — What `operations.def` actually does
The file contains no ordinary C++. It is a list of macro invocations and nothing else:
```c++
SQF_OPERATION(sale_create,   right_sale_create,   Synchronizable, OfflineAllowed)
SQF_OPERATION(sale_void,     right_sale_void,     Synchronizable, OnlineRequired)
SQF_OPERATION(receipt_print, right_receipt_print, LocalOnly,      OfflineAllowed)
```
It is never compiled on its own. Each consumer **defines the macro differently, includes the file, and undefines it**, so the same twenty lines produce a different artefact each time:
```c++
// Consumer 1 — the operation identifiers
enum class SalesOperation {
#define SQF_OPERATION(id, right, sync, offline) id,
#include "operations.def"
#undef SQF_OPERATION
};

// Consumer 2 — the table the permission screen and the sync router read
constexpr OperationInfo kSalesOperations[] = {
#define SQF_OPERATION(id, right, sync, offline) \
	{ SalesOperation::id, #id, Right::right, Class::sync, Offline::offline },
#include "operations.def"
#undef SQF_OPERATION
};
```
Five things are produced from the one list:
<table header-row="true">
<tr>
<td>Consumer</td>
<td>What it produces</td>
<td>What it prevents</td>
</tr>
<tr>
<td>Operation identifiers</td>
<td>A typed enumeration instead of strings</td>
<td>A typo in an operation name compiling successfully</td>
</tr>
<tr>
<td>The rights registry</td>
<td>Rows in the owner's permission screen</td>
<td>A new operation that nobody can grant or deny because the screen was never updated</td>
</tr>
<tr>
<td>The capability table</td>
<td>The offline rule for each operation</td>
<td>The staff device's read-only rule being written per screen and forgotten on the eleventh screen</td>
</tr>
<tr>
<td>The sync router</td>
<td>Dispatch from an outbox entry to its handler</td>
<td>An operation that is saved locally and silently never sent</td>
</tr>
<tr>
<td>The completeness test</td>
<td>A build failure</td>
<td>A handler that exists in code but appears in no list at all</td>
</tr>
</table>
And because the dispatch is a switch over an enumeration, **adding a line to the file makes the build fail until the handler is written.** The compiler becomes the checklist.
**The honest cost.** Macro-generated code is harder to step through in a debugger and confuses editor tooling. Mitigation: the build writes the expanded table to a generated header that can be opened and read like ordinary code. A plain `constexpr` array of structures would be friendlier to tooling, but it cannot generate the enumeration or force switch exhaustiveness, which is the entire point.
---
## Appendix B — Attacking this structure
Every item below is a real weakness in the layout above, with the smallest change that repairs it. Nothing here requires abandoning the shape.
<table header-row="true">
<tr>
<td>#</td>
<td>The attack</td>
<td>Severity</td>
<td>The small fix</td>
</tr>
<tr>
<td>1</td>
<td>**Modules may not depend on each other — but real work crosses them.** Turning a quotation into an order prices it, checks an agreement's quantity cap, and creates a receivable. Four modules. The rule forbids it and the composition root cannot hold business rules, so this logic has no legal home and will leak into whichever module is touched first</td>
<td>**Fatal as written**</td>
<td>Add one layer: `src/workflows/`. It is the **only** place allowed to depend on several modules; modules stay leaves and still cannot see each other. Quote-to-order, invoice issue, cancel-and-reissue, and delivery live there</td>
</tr>
<tr>
<td>2</td>
<td>~~Thirteen modules is over-partitioning for one developer~~ — **this attack is withdrawn.** It argued from how many people work in the shop, which is not an architectural argument. Module count follows from what owns what. The real finding, on merit, is different: one item in the list is not a module at all</td>
<td>**Withdrawn, and replaced**</td>
<td>The count stays. `approvals` is reclassified as a **mechanism** and moves into `engine/`, and one merge question about `orders` and `jobs` is decided by a fact rather than by preference. See step 6a</td>
</tr>
<tr>
<td>3</td>
<td>**Per-module migration numbering collides.** Two modules both write `0007_`, and a cross-module foreign key needs a guaranteed order the per-module scheme cannot express</td>
<td>High — breaks silently on a second machine</td>
<td>**One global sequence** with the module in the name: `0031_sales_add_void_reason.sql`. Files may live in the module folder; ordering is global and the runner refuses duplicate numbers</td>
</tr>
<tr>
<td>4</td>
<td>**Rights have two sources of truth.** `operations.def` is in the workstation module; the canonical right list was put in the protocol repository. The server cannot build from a file it does not have, and the two will drift</td>
<td>High — and the drift is a permission gap</td>
<td>Move the `.def` files **into the protocol repository**, one per module. Both sides include the same file from the same submodule commit. Zero drift by construction</td>
</tr>
<tr>
<td>5</td>
<td>**Right identifiers are compile-time, but grants are stored rows.** Rename or remove a right and every stored grant becomes an orphan that fails open or fails closed — both bad</td>
<td>Medium, but silent</td>
<td>Right identifiers are **permanent and never reused**. Removals go to a retired list, and a migration maps each retired identifier to its replacement or explicitly to nothing</td>
</tr>
<tr>
<td>6</td>
<td>**Installing into ****`Program Files`**** means every update needs an administrator prompt**, which collides with the notify-and-click update flow</td>
<td>Medium — friction on every release</td>
<td>Install **per user** under `%LOCALAPPDATA%\Programs\SquiFlow`. Updates then need no elevation at all. This is what most self-updating desktop applications do</td>
</tr>
<tr>
<td>7</td>
<td>**Data in ****`%LOCALAPPDATA%`**** is per Windows account.** Sign into a different Windows user — after a repair, a reinstall, a new profile — and the shop's database appears to be gone</td>
<td>**High — looks like data loss**</td>
<td>Program per user, **data in ****`%PROGRAMDATA%\SquiFlow`** with restricted permissions. Independent of which Windows account is signed in</td>
</tr>
<tr>
<td>8</td>
<td>**A ****`current`**** symbolic link needs a privilege ordinary accounts lack** on Windows</td>
<td>Medium — fails only on the real machine</td>
<td>Use a **directory junction**, which needs no special privilege, or skip the link entirely: a tiny launcher reads the active version from a one-line file</td>
</tr>
<tr>
<td>9</td>
<td>**Maintenance is gated on idleness — so on a machine that is busy all day and switched off at closing, it never runs.** Checkpoints and vacuuming quietly never happen</td>
<td>Medium, compounding</td>
<td>Two extra triggers: a bounded maintenance pass **at graceful shutdown** with a hard time cap, and a forced pass **at startup** if none has run in several days</td>
</tr>
<tr>
<td>10</td>
<td>**A two-thread pool can be starved.** One slow photo upload on a weak link occupies a slot; a second occupies the other; the outbox stops draining and nothing syncs</td>
<td>Medium — appears only on bad days</td>
<td>Media uploads get their **own single-slot lane**. The shared pool keeps a **reserved slot for sync** that nothing else may take</td>
</tr>
<tr>
<td>11</td>
<td>**Screens unload when hidden — including one with a half-typed form**, and reloading costs a disk seek on an HDD</td>
<td>Medium — a person notices immediately</td>
<td>Keep the **two most recent** screens loaded, and **never unload a screen with unsaved edits**. The memory budget survives two</td>
</tr>
<tr>
<td>12</td>
<td>**Everything compiled in means the module list is fixed at build time**, which contradicts white-labelling by configuration rather than by fork</td>
<td>Medium — a future contradiction</td>
<td>Keep static linking, but make **activation** runtime data. All modules register; a settings table decides which appear. One binary, several configurations — now formalised as the core and extra tiers in step 6b</td>
</tr>
<tr>
<td>13</td>
<td>**Document wording is inside compiled QML**, so changing a line on a receipt requires a full release cycle</td>
<td>Low, but constant</td>
<td>Screens stay compiled; **printed document templates become data** in the database. Wording changes need no build</td>
</tr>
<tr>
<td>14</td>
<td>**No telemetry means crashes are invisible** unless someone describes them over the phone</td>
<td>Low</td>
<td>The crash dump raises an **attention item**, and on the owner's device it may be attached to the next sync as a diagnostic record — visible, opt-in, never automatic</td>
</tr>
<tr>
<td>15</td>
<td>**The daily sweep for date-driven attention items assumes the machine is on at midnight.** It is not</td>
<td>Low</td>
<td>Sweep on the **first launch of each day**, not at a wall-clock hour</td>
</tr>
<tr>
<td>16</td>
<td>**The local snapshot backup offers false comfort.** It copies a store that is a cache; the real record is on the server, whose restore has never been rehearsed</td>
<td>Medium — already an open risk</td>
<td>Label the local snapshot as convenience only. **The server restore rehearsal stays the gating requirement** and is not satisfied by anything on the workstation</td>
</tr>
</table>
**What survived the attack unchanged:** static modules over plugins, the composition root, `operations.def`, one connection with one writer, compiled screens, event-driven services with a single coarse tick, and mirrored module names across the two sides. **What changed:** a workflows layer, `approvals` demoted from a module to a mechanism in `engine/`, global migration numbering, the `.def` files moving into the protocol repository, and four corrections to where things live on the Windows machine. **The module count itself stands** — attack 2 was withdrawn because it reasoned from team size rather than from ownership.
<callout icon="🧭">
	**Reading rule.** This page governs where files go and why. Module boundaries, safety enforcement and the pipeline live on the codebase and pipeline page; runtime and sync behaviour on the architecture page; what the features do on the feature specification. If a structure here ever makes a feature awkward to build, the structure is wrong — but it should be changed deliberately and recorded, not worked around with one exception that becomes ten.
</callout>
