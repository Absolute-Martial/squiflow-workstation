# SquiFlow — Codebase Structure, Modules, Updates & Build Pipeline

Source page id: 2ac5a2eb71ad47f58ef2320c93f1a3c0

---

<callout icon="📌">
	**What this page is.** How the code is organised, how a new module gets added, how the shipped application updates itself, how memory safety is enforced rather than hoped for, where every megabyte of the memory budget goes, and how images and executables are built and published. Every environment fact used here was stated by the shopkeeper. Where something is still unknown it appears as an open item, not as an assumption.
</callout>
## Decisions recorded this round
<table header-row="true">
<tr>
<td>Question</td>
<td>Decision</td>
</tr>
<tr>
<td>Devices and people</td>
<td>**Two devices, two people** — one for the shop owner, one for a staff member</td>
</tr>
<tr>
<td>Staff capability</td>
<td>**Granular, owner-controlled access.** The owner decides action by action what the staff member may do. No preset "staff can do X" bundle</td>
</tr>
<tr>
<td>Permission model</td>
<td>**Rights granted per person. No roles.** A person holds a set of rights directly</td>
</tr>
<tr>
<td>Sign-in</td>
<td>**Username and password**</td>
</tr>
<tr>
<td>Staff device offline</td>
<td>**Read-only when offline.** The staff device may view but not create or change anything while disconnected</td>
</tr>
<tr>
<td>Update application</td>
<td>**Notify, then the person clicks update.** Never silent, never forced mid-work</td>
</tr>
<tr>
<td>Update source</td>
<td>**Private GitHub Releases**</td>
</tr>
<tr>
<td>Repository layout</td>
<td>**Three separate private repositories, a private umbrella repository that pulls them together, and a public shell.** Linked by submodules or subtrees</td>
</tr>
</table>
---
## Three consequences to accept deliberately
Each of these follows from an answer above. None is a problem — but each costs something, and it is cheaper to accept the cost now than to discover it in Wave 3.
### 1. "Rights per person, no roles" is simpler now and heavier later
With two people, roles are pure overhead — a role is a saved bundle, and there is nothing to save. So the model is: a person has a **right set**, and the owner edits it with a checklist.
The cost arrives at the third employee, when the owner starts hand-ticking the same twenty boxes again. The mitigation is cheap and goes in now: **a right set can be copied from another person**. That is 90% of what roles do, for none of the machinery. If roles are ever wanted, they become a named saved right set — additive, not a rewrite.
The rule that keeps this honest: **the owner's right set is not editable and cannot be reduced.** Exactly one person always holds every right, or the shop can lock itself out of its own data.
### 2. "Staff read-only when offline" removes a large amount of machinery
This is the most consequential answer of the round, and it simplifies the architecture significantly.
<table header-row="true">
<tr>
<td>What it removes</td>
<td>Why</td>
</tr>
<tr>
<td>Reserved number blocks on the staff device</td>
<td>The staff device never issues a document offline, so it never needs a pre-allocated number range. Only the owner device does</td>
</tr>
<tr>
<td>An outbox on the staff device</td>
<td>Nothing is written offline, so there is nothing to queue</td>
</tr>
<tr>
<td>Offline divergence between the two devices</td>
<td>Only one device can produce work while disconnected. Two histories cannot both move forward</td>
</tr>
<tr>
<td>Merge and conflict-resolution UI</td>
<td>What remains is the ordinary race — two people changing the same record. Resolved by a version check plus a fixed tiebreak: **the owner's version wins**, and the losing version is retained for inspection. No merge engine, no resolution screen</td>
</tr>
<tr>
<td>Offline re-validation of credit limits and quantity caps on the staff device</td>
<td>The staff device is either online, where the server answers authoritatively, or read-only, where it cannot commit anything to re-validate</td>
</tr>
</table>
**And here is what it costs — now decided.** The staff member is **an all-rounder, not a specialised helper**. That person mans the counter and does whatever the day requires, which means strict read-only offline would stop the shop taking money on the first bad network day.
So the narrow exception is **granted**: on the staff device, **a counter sale and a payment receipt can be created offline**, using a number range reserved for that device. Everything else — quotations, invoices, agreements, catalog prices, permissions, cancellations — remains read-only when disconnected.
This is the smallest exception that keeps the shop earning, and it survives the simplification almost intact, because a counter sale has no prior state to collide with: it is a new record with a new number. What comes back is a small outbox on the staff device holding only these two record types, and a reserved number range on both devices rather than only the owner's.
### The tiebreak: the owner's version wins
When the same record has been changed in two places, **the owner's version is taken.** One rule, no negotiation, no merge screen. It is the right rule for a two-person shop where one of the two is accountable for the outcome.
Two qualifications keep this from destroying data:
- **The losing version is retained, not discarded.** It is stored against the record and viewable, so "the owner overwrote my change" is answerable with evidence rather than argument. Silently deleting the staff member's work would make the rule feel arbitrary the first time it cost someone an hour.
- **A separate record is never a conflict.** Two counter sales entered on two devices are two sales. Two payments are two payments. The tiebreak applies only when both devices changed *the same record*, identified by its own identity — never to independently created records that merely look similar. Getting this wrong would delete money, so it is a rule about identity, not resemblance.
### 3. Private GitHub Releases cannot be downloaded by an anonymous client
This is a hard technical fact, not a preference. Release assets in a **private** repository require an authenticated request. So the shipped application cannot simply fetch a URL.
That leaves two shapes, and only one of them is safe:
<table header-row="true">
<tr>
<td>Shape</td>
<td>Verdict</td>
</tr>
<tr>
<td>Ship a GitHub token inside the executable so it can call the API directly</td>
<td>**Rejected.** A token in a client binary is a published token. Anyone with the exe has read access to the private repository, and rotating it means shipping a new build</td>
</tr>
<tr>
<td>**The shop's own server proxies updates.** The Ubuntu server holds a read-only fine-grained token as a Podman secret, checks GitHub for new releases, downloads and caches the asset, and serves it to the two workstations over the existing authenticated channel</td>
<td>**Adopted.** The secret stays on one machine the shopkeeper controls. Both devices download over the LAN instead of twice from the internet. And the update is available to the staff device even though that device has no GitHub credentials at all</td>
</tr>
</table>
The fallback for the case where the server is unreachable and an update is genuinely needed: the shopkeeper downloads the installer from GitHub in a browser, while signed in, and runs it. That path must keep working and must be documented, because it is the recovery path when the server itself is the thing that broke.
---
## Repository layout
Five repositories: three private components, one private umbrella, one public shell.
<table header-row="true">
<tr>
<td>Repository</td>
<td>Visibility</td>
<td>Contains</td>
<td>Depends on</td>
</tr>
<tr>
<td>`squiflow-protocol`</td>
<td>Private</td>
<td>The wire contract and nothing else: message and DTO definitions, permission identifiers, operation classes, error codes, version constants, and the contract tests that both sides must pass</td>
<td>Nothing. This is the root of the dependency graph and it must stay that way</td>
</tr>
<tr>
<td>`squiflow-server`</td>
<td>Private</td>
<td>The Oat++ backend, PostgreSQL migrations, the container definition, Quadlet units, the update-proxy service</td>
<td>`squiflow-protocol`</td>
</tr>
<tr>
<td>`squiflow-workstation`</td>
<td>Private</td>
<td>The Qt/QML application, the embedded local engine, SQLite migrations, the packaging and installer scripts, the updater helper</td>
<td>`squiflow-protocol`</td>
</tr>
<tr>
<td>`squiflow-craft-core`</td>
<td>Private</td>
<td>**The umbrella.** No product code of its own. Holds the three components as submodules, the release manifest, the cross-component integration tests, the release workflows, the developer bootstrap script, and the documents that describe the whole system</td>
<td>All three</td>
</tr>
<tr>
<td>`squiflow-craft`</td>
<td>Public</td>
<td>**The shell.** Public identity: README, licence, screenshots, issue templates, the published protocol specification as documentation, and release notes. No buildable product source</td>
<td>Nothing</td>
</tr>
</table>
### Submodules, not subtrees
Both were considered. Submodules win here, for one reason that matters more than every convenience argument: **a submodule records which exact commit of each component a release was built from, and it does it in one line.** For a system that ships an executable to a shop and must be able to answer "what precisely is running on that machine", that property is worth the friction.
Subtrees copy the component's files and history into the umbrella. That makes checkout simpler and everything else worse: the umbrella's history balloons, changes made in the umbrella have to be pushed back upstream through a separate command, and the clean answer to "which commit is this" disappears into merge commits.
The friction submodules bring, stated so nobody is surprised by it:
- A plain clone gets empty component directories. Cloning must be recursive, and the bootstrap script in the umbrella must do this so nobody has to remember it.
- A component change is **two commits**: one in the component, one in the umbrella to move the pointer. This is the price of the pinning, and it is the same price as pinning any other dependency.
- Changing a branch in the umbrella does not change the submodule contents until they are updated explicitly. Every workflow must update submodules as an explicit step.
- **GitHub Actions cannot check out a private submodule with the default workflow token.** That token is scoped to the one repository the workflow runs in. Cloning private submodules requires an explicitly provisioned credential — a GitHub App installation token, scoped to the four private repositories, is the right shape; a long-lived personal token is the shape to avoid. This is a setup step that must be done before the first umbrella build, and it will fail confusingly if it is skipped.
### The protocol version rule
`squiflow-protocol` is the only thing both sides share, which makes it the only place where a careless change breaks a running shop. Three rules:
1. **The wire major version is compared on every connection.** Mismatched majors refuse to talk and say so in plain language, rather than failing on a field that isn't there.
2. **Additive changes only within a major version.** New optional fields, new messages, new permission identifiers. Never a removed field, a renamed field, or a changed meaning.
3. **The contract tests live in the protocol repository, and both server and workstation run them in their own pipelines.** A protocol change that breaks a consumer fails in the consumer's build, not at the shop counter.
---
## Inside the workstation repository
```plain text
squiflow-workstation/
  cmake/                   toolchain, vcpkg overlay ports, packaging helpers
  external/protocol/       submodule
  src/
    app/                   entry point, single-instance lock, crash handler, logging
    platform/              Windows specifics: DPAPI, service control, paths, updater IPC
    engine/                the embedded local engine
      storage/             SQLite access, single-writer gate, migrations, integrity check
      sync/                outbox, cursor, orchestrator, transport, MessagePack codec
      identity/            session, right set evaluation, offline capability gate
    modules/               one directory per feature module
      parties/
      catalog/
      pricing/
      sales/
      jobs/
      approvals/
      agreements/
      receivables/
      sourcing/
      companion/
    shell/                 navigation, command palette, attention surface, shared widgets
    ui/                    QML, one subdirectory per module, mirrored names
  tests/
    unit/                  per module, no database
    engine/                storage and sync against a temporary database
    contract/              the protocol tests
    ui/                    QML tests for the flows that must never break
  packaging/               windeployqt manifest, installer script, file associations
```
Two structural rules that keep this from rotting:
- **`modules/x`**** may depend on ****`engine`****, ****`shell`**** and ****`protocol`****. It may never include a header from ****`modules/y`****.** Anything two modules both need moves down into the engine or the shell. This is enforced by build configuration, not by good intentions — each module is its own library target with an explicit dependency list, so a cross-module include fails to link.
- **`ui/x`**** talks only to ****`modules/x`****.** QML never reaches into storage.
### Inside the server repository
```plain text
squiflow-server/
  external/protocol/       submodule
  src/
    main/                  bootstrap, configuration, health endpoint
    infrastructure/        Postgres pool, migrations runner, object store, secrets
    endpoints/             Oat++ controllers, one per module, thin
    services/              one per module, mirrors the workstation module list
    sync/                  sequence assignment, idempotency ledger, batch apply
    identity/              authentication, right set enforcement, audit writer
    updates/               the GitHub release proxy and asset cache
  migrations/              numbered, forward-only SQL
  deploy/                  Containerfile, Quadlet units, backup scripts
  tests/
```
**The endpoint layer contains no business rules.** It decodes, authenticates, calls one service method, encodes. Every rule lives in a service, so every rule is testable without a network.
---
## What a module is, and how a new one is added
A module is not a folder convention. It is a unit with a fixed contract, so that adding the eleventh module is the same amount of work as adding the fourth.
**Every module provides exactly five things:**
<table header-row="true">
<tr>
<td>Provides</td>
<td>Meaning</td>
</tr>
<tr>
<td>Its data</td>
<td>Its own tables and its own migrations. No module writes another module's tables — it calls that module's service</td>
</tr>
<tr>
<td>Its operations</td>
<td>Each one declared with an **operation class** — local-only, synchronizable, or online-required — and the **right** it requires. Both are declarations, not code in the handler, so both can be listed, tested, and shown in the permission checklist automatically</td>
</tr>
<tr>
<td>Its screens</td>
<td>QML registered by name, loaded on demand, unloaded when hidden</td>
</tr>
<tr>
<td>Its navigation and search contributions</td>
<td>What it adds to the command palette, the sidebar, and global search — declared, so the shell needs no knowledge of the module</td>
</tr>
<tr>
<td>Its attention rules</td>
<td>Any deterministic rule it contributes to the companion surface</td>
</tr>
</table>
**Adding a module is then a checklist, in this order:**
1. Add its messages and DTOs to `squiflow-protocol`, with a contract test.
2. Add the server service and its Postgres migration. Service first, endpoint after.
3. Add the endpoint — decode, authenticate, delegate, encode.
4. Add the workstation module library, its SQLite migration, and its declared operations with their rights and operation classes.
5. Register its rights so they appear in the owner's permission checklist without any UI change.
6. Add its QML and register its screens, palette entries, and search contributions.
7. Add its sync behaviour: which records sync, in what dependency order, and what the server does with a replayed idempotency key.
8. Add tests at all three levels — unit, engine, contract — and only then wire it into the navigation.
If a step in that list is impossible for some module, the module boundary is wrong. That is the value of having the list.
---
## Memory safety
The standard is not "we are careful". The standard is: **an unsafe construct either does not compile, or fails a build.**
### Ownership rules, in force everywhere
- No raw owning pointers. Ownership is a unique pointer or a value; a raw pointer or reference is always a non-owning borrow whose lifetime is shorter than the owner's, and that must be obvious at the call site.
- **Shared ownership requires a written reason.** A shared pointer is a design decision, not a default.
- `string_view` and `span` never outlive what they view. They are parameters. They are never stored in a member, never returned from a function that owns the buffer, and never captured in a lambda that outlives the frame.
- Qt objects follow Qt's parent ownership; C++ objects follow C++ ownership. **Nothing is owned by both.** Anything handed to QML is either a value type or has a lifetime that provably exceeds the QML engine's — this is the single most common way a Qt application crashes, and it will be reviewed on every change that crosses that boundary.
- Expected failures return `std::expected`. Exceptions are reserved for the genuinely exceptional, and **no exception crosses a module boundary or a thread boundary**.
- Integer arithmetic on anything derived from input uses checked operations. Money is exact decimal, never floating point, on both sides of the wire.
### What the build enforces
<table header-row="true">
<tr>
<td>Where</td>
<td>Enforcement</td>
</tr>
<tr>
<td>Every compile</td>
<td>Warnings as errors, high warning level, on both compilers. A warning that is not worth fixing is not worth having enabled</td>
</tr>
<tr>
<td>Debug builds</td>
<td>Hardened standard library assertions, checked iterators, runtime checks for uninitialised locals and stack corruption</td>
</tr>
<tr>
<td>Release builds</td>
<td>Stack protection, buffer-overflow fortification, control-flow guard, ASLR and DEP enabled, additional security checks on MSVC. These are compiler flags, not effort</td>
</tr>
<tr>
<td>CI, dedicated job</td>
<td>Address and undefined-behaviour sanitizers on the full server and engine test suites. Already passing on the server core — this must remain a required check, not an occasional one</td>
</tr>
<tr>
<td>CI, dedicated job</td>
<td>Thread sanitizer on the sync and storage tests specifically. The orchestrator, the single-writer gate, and the outbox are where a data race would actually live</td>
</tr>
<tr>
<td>CI, on every protocol change</td>
<td>**Fuzzing the decoder.** The MessagePack and message decoders parse bytes that arrive over a network. That is the one place in the system where hostile or corrupt input is a realistic scenario, and a decoder crash on malformed input is the classic C++ failure. A short fuzz run per change, with a persistent corpus, catches this cheaply</td>
</tr>
<tr>
<td>CI, static analysis</td>
<td>Clang-tidy with lifetime, bug-prone and modernisation checks, plus MSVC analysis. New warnings block; the existing baseline is burned down, not ignored</td>
</tr>
<tr>
<td>Runtime, shipped build</td>
<td>A crash handler writes a minidump and a log locally and offers to attach it to a report. A crash the shopkeeper cannot report is a crash that gets fixed twice</td>
</tr>
</table>
**One deliberate omission.** C++26 contracts are in the parent plan as a language feature to adopt. They are not load-bearing for safety here, because compiler support is still moving. Assertions carry the weight; contracts replace them where they work, and nothing depends on them.
---
## Memory overhead
The ceiling is **150 MB idle on the workstation, with the local database open.** A ceiling that is not divided is a ceiling that is not enforced, so here is the division.
<table header-row="true">
<tr>
<td>Consumer</td>
<td>Budget</td>
<td>How it is held there</td>
</tr>
<tr>
<td>Qt and QML runtime, one loaded screen</td>
<td>\~60 MB</td>
<td>QML compiled ahead of time into the binary rather than parsed at startup. Only the current screen is instantiated; others are loaded on navigation and unloaded when hidden</td>
</tr>
<tr>
<td>SQLite page cache</td>
<td>\~16 MB, hard-capped</td>
<td>An explicit cache size, set once at open. Default SQLite settings will happily grow past this. Memory mapping is used conservatively because the disk is a spinning HDD</td>
</tr>
<tr>
<td>Loaded data models</td>
<td>\~20 MB</td>
<td>Virtualized lists fetching a window of rows, never a whole table. A model holds the visible window plus a small margin, and releases the rest. This is the same mechanism as the 100,000-row requirement, so it is not extra work</td>
</tr>
<tr>
<td>Local engine, sync, codecs</td>
<td>\~20 MB</td>
<td>Sync batches capped at 50–100 items. Decoding streams rather than buffering a whole payload. Request handling uses a reset-per-operation arena instead of thousands of small allocations</td>
</tr>
<tr>
<td>Images, thumbnails, previews</td>
<td>\~15 MB</td>
<td>A small bounded cache, evicted by least-recent use. Thumbnails are generated to disk once and read back at display size — never decoded at full resolution to be drawn small. Design files are large; this is the budget line most likely to be blown</td>
</tr>
<tr>
<td>Headroom</td>
<td>\~19 MB</td>
<td>Deliberately unallocated</td>
</tr>
</table>
### Rules that protect the budget
- **The budget is a test.** A startup-and-idle measurement runs in CI on a Windows runner and fails the build past the ceiling. A budget nobody measures is a wish.
- **No object-relational mapping and no runtime-defined fields.** Both trade a small fast binary for flexibility this shop does not need — already a locked decision, restated here because both are memory decisions as much as design decisions.
- **Nothing polls.** Idle means idle: no timers redrawing, no background queries on a schedule, no animation running behind a hidden screen. This protects CPU and battery, and it also protects memory, because polling paths keep caches warm that should have been dropped.
- **Freed means returned.** After a large operation — a bulk price edit, a long list, a file scan — buffers are released rather than retained "in case". A process that only ever grows is a leak with good manners.
### On the server side
The server shares an 8 GB machine with PostgreSQL, so the same discipline applies with different numbers:
- **Podman memory limits per container**, set deliberately. A limit turns a slow leak into a restart that the shopkeeper notices, instead of a machine that swaps to death on an HDD — which, on a spinning disk, is indistinguishable from a total outage.
- **Oat++ thread pool sized to the machine**, not to a default. Four cores and two real users do not need dozens of worker threads, and each one costs a stack.
- **PostgreSQL tuned for a small number of connections with generous shared buffers**, with a connection pool ceiling in the server so a bug cannot open connections without bound.
- **The update asset cache is bounded** — keep the current release and the previous one, delete older. It exists to avoid re-downloading, not to archive.
---
## Auto-update
The shape, end to end:
1. The server checks GitHub for a new release on a schedule, using its read-only token held as a Podman secret. It never checks on behalf of a device request, so the token is used on one predictable path.
2. A new release is downloaded once, its digest verified against the release manifest, and cached on the server.
3. Each workstation asks the server on launch and periodically: *is there something newer than what I am running?*
4. If yes, a **quiet, dismissible notice** appears — not a modal, never during a document being edited. It states the version, what changed, and the size. The person clicks update when they choose.
5. The application downloads the staged asset from the server over the LAN, verifies its digest **and a signature made with a key the shop controls**, and stages it into a versioned folder beside the current one.
6. The update applies **on next launch**, performed by a small separate updater process. This is not a preference: Windows will not let a running executable be replaced, so a helper must do the swap after the application exits.
7. The previous version's folder is kept. If the new version fails to start twice, the launcher reverts to the previous folder automatically and says so.
### The parts that are easy to get wrong
<table header-row="true">
<tr>
<td>Trap</td>
<td>Handling</td>
</tr>
<tr>
<td>An update that requires a database migration, applied while the other device runs the old version</td>
<td>The release manifest declares a **minimum compatible protocol major and schema version**. If the server has moved past what a device supports, that device refuses to sync and states that it must update — it does not sync badly. Migrations are forward-only and additive within a major version so the two devices can be one version apart for a day without harm</td>
</tr>
<tr>
<td>Updating while unsynced work sits in the owner device's outbox</td>
<td>The update is offered but **applying it requires the outbox to be empty**, or an explicit acknowledgement. The local store is load-bearing; an update is not worth risking queued work</td>
</tr>
<tr>
<td>The digest is verified but the source is not</td>
<td>A digest proves the file arrived intact, not that it is legitimate. The manifest is signed with a key the shop holds, and the public half is embedded in the application. Without this, the update channel is the easiest way into the shop</td>
</tr>
<tr>
<td>Windows warns about an unsigned executable on every install</td>
<td>**Decided: an automated self-signed certificate**, generated and used in the pipeline. See the section below for exactly what that does and does not buy — it is genuinely useful, but it does not silence the Windows warning by itself</td>
</tr>
<tr>
<td>A half-downloaded update</td>
<td>Download to a temporary name, verify, then rename into place. A staged folder is only valid once a marker file says it is complete</td>
</tr>
<tr>
<td>The server is down and an update is needed anyway</td>
<td>The documented manual path: sign in to GitHub in a browser, download the installer, run it. This must be tested, not just written down</td>
</tr>
</table>
---
## Build and publish pipeline
Everything builds in GitHub Actions. Nothing is compiled on shop hardware.
<table header-row="true">
<tr>
<td>Workflow</td>
<td>Runs on</td>
<td>Does</td>
</tr>
<tr>
<td>**Protocol**</td>
<td>Every push and pull request to `squiflow-protocol`</td>
<td>Builds the contract, runs contract tests, runs the decoder fuzzer against the stored corpus. Fast, because everything else waits on it</td>
</tr>
<tr>
<td>**Server — test**</td>
<td>Every pull request</td>
<td>Debug build, Release build, and a sanitizer build. Full test suite against a real PostgreSQL service container, because a mocked database proves nothing about SQL</td>
</tr>
<tr>
<td>**Server — image**</td>
<td>Merge to main, and tags</td>
<td>Builds the container image, pushes to the private GitHub Container Registry, **records the digest**. Main pushes a rolling tag; a release tag pushes an immutable one. The server pulls by digest, never by a floating tag</td>
</tr>
<tr>
<td>**Workstation — test**</td>
<td>Every pull request</td>
<td>Windows build, unit and engine tests, QML tests, static analysis, and the startup memory measurement against the 150 MB ceiling</td>
</tr>
<tr>
<td>**Workstation — package**</td>
<td>Tags</td>
<td>Release build, Qt deployment collection, installer construction, digest, manifest signing, upload to a **private GitHub Release**</td>
</tr>
<tr>
<td>**Umbrella — integration**</td>
<td>Nightly, and before any release</td>
<td>Checks out all three submodules at their pinned commits, starts the server in Podman against a real PostgreSQL, and drives the workstation engine against it. This is the only job that proves the pieces fit</td>
</tr>
<tr>
<td>**Release**</td>
<td>Manual, from the umbrella</td>
<td>Takes the three pinned commits, verifies each has a green build, produces the release manifest — versions, digests, minimum compatible schema, release notes — signs it, publishes the private Release, and tags the umbrella</td>
</tr>
</table>
### What makes it fast enough to use
A C++ and Qt build is slow by nature, and a pipeline nobody waits for is a pipeline nobody runs.
- **Prebuilt Qt**, installed by the runner rather than compiled. Compiling Qt in CI is an hour nobody needs to spend.
- **A vcpkg binary cache published to GitHub Packages.** Dependencies compile once per version bump, not once per push. This is the single largest saving available.
- **A compiler cache** keyed on content, shared across runs of the same workflow.
- **Pull-request builds do less than release builds.** One configuration plus tests on a pull request; the full matrix on a tag.
- **Sanitizer and fuzz jobs run in parallel with the ordinary build**, not after it.
### What makes it trustworthy
- **One source of truth for the version.** The umbrella's tag. Every artefact — installer, image, manifest, about screen — carries it, and it is injected at build time rather than typed into three files.
- **Pinned everything.** Compiler version, Qt version, vcpkg baseline, base container image by digest, runner image version. "It built last month" must remain true next month.
- **The Oat++ patch stays a vcpkg overlay port**, as already decided. A fork would put a dependency's history inside the release chain.
- **Every published artefact is reproducible from a commit that exists.** The manifest names the three submodule commits. Given a manifest, the build can be repeated.
- **A release is blocked** if any component build is red, if the nightly integration job has not passed on those commits, or if the manifest is unsigned.
---
## Self-signed signing — what it actually buys
A self-signed certificate is the right call for a two-machine shop, but only if it is chosen for what it really does. Being precise about this matters, because the obvious expectation is the one thing it does *not* deliver.
<table header-row="true">
<tr>
<td>What it does</td>
<td>What it does not do</td>
</tr>
<tr>
<td>**Proves the file was not tampered with** between the pipeline and the shop. Any modification breaks the signature</td>
<td>**It does not silence the Windows warning on its own.** Windows trusts a signature only if it chains to a certificate authority it already trusts. A self-signed certificate does not</td>
</tr>
<tr>
<td>**Signs the update manifest**, which is the actual security boundary. This is the same key, so one automated mechanism covers both</td>
<td>**It earns no SmartScreen reputation.** Reputation is built from install volume of a commercially signed binary. Two machines will never build it</td>
</tr>
<tr>
<td>**Becomes fully silent on your two machines** once the certificate is installed into the Windows Trusted Publishers store — a one-time administrator action per device. After that, installs and updates run without a prompt</td>
<td>**It gives anyone else nothing.** If the application is ever handed to another shop, that shop sees the warning until it also trusts the certificate manually — which is exactly the habit you would not want to teach a customer</td>
</tr>
</table>
**How it is automated, and where the key lives.** The signing key is generated once, kept as a repository secret used only by the release workflow, and **backed up offline outside GitHub** — on paper or on a drive in a drawer. That backup is not optional: if the key is lost, every future update fails signature verification against the public half already embedded in the installed application, and the only recovery is a manual reinstall on both machines.
The certificate carries a long validity — ten years or more — because nothing external validates it and an expiry would only create a self-inflicted outage. A trust-installation step for both devices belongs in the setup checklist, alongside the antivirus exclusion and the backup rehearsal.
**One consequence to accept:** because reputation will never accumulate, the day this is handed to a second shop, a real code-signing certificate becomes a purchase. That is a decision for that day, not this one.
---
## Server specification and PostgreSQL settings
**Stated: the server is the same specification as the workstation** — 8 GB DDR4, Intel Core i5-7500, spinning HDD, Ubuntu Server, everything in Podman. That closes the last blocker on tuning, so here are actual numbers instead of principles.
<table header-row="true">
<tr>
<td>Container</td>
<td>Memory limit</td>
<td>Reasoning</td>
</tr>
<tr>
<td>PostgreSQL</td>
<td>3 GB</td>
<td>Shared buffers around 2 GB, with the rest as working memory and overhead. Comfortable for a two-user shop with years of history</td>
</tr>
<tr>
<td>Oat++ backend</td>
<td>1 GB</td>
<td>Two users. A thread pool sized to the four cores, not to a library default</td>
</tr>
<tr>
<td>Object store</td>
<td>512 MB</td>
<td>It streams bytes; it does not need to hold them</td>
</tr>
<tr>
<td>Reverse proxy and update proxy</td>
<td>256 MB combined</td>
<td>Both are thin</td>
</tr>
<tr>
<td>Left to the operating system</td>
<td>\~3 GB</td>
<td>**Deliberate, not leftover.** On a spinning disk the kernel's file cache is what makes the database tolerable. Handing it all to PostgreSQL would make things slower, not faster</td>
</tr>
</table>
Settings that matter specifically because the disk is mechanical:
- **Connection ceiling low — around 20**, with a pool in the backend. Each connection costs memory, and two users cannot need more.
- **Checkpoints spread out and infrequent.** Bunched checkpoints on an HDD are a visible stall at the counter.
- **`fsync`**** stays on. Always.** This is the setting that corrupts databases when disabled, and it is the reason the browser history discussed earlier was recoverable as a lesson rather than as data.
- **Autovacuum left on**, scheduled to prefer quiet hours, never disabled.
- **Backups are the only redundancy.** One disk, no second disk. An offsite copy is not a nice-to-have; it is the entire disaster plan.
---
## Windows 7 — a direct conflict that has to be resolved
**This one cannot be recorded as decided, because it contradicts decisions already locked elsewhere in the plan.** Stating it plainly rather than quietly building around it:
<table header-row="true">
<tr>
<td>Constraint</td>
<td>Conflict with Windows 7</td>
</tr>
<tr>
<td>**Qt 6**</td>
<td>Qt 6 supports Windows 10 and later. Windows 7 support ended with Qt 5.15. There is no build configuration that makes Qt 6 run on Windows 7</td>
</tr>
<tr>
<td>**C++23 moving to C++26**</td>
<td>The toolchains and runtime that modern C++ needs target Windows 10 era APIs. Targeting Windows 7 means giving up APIs and fighting the toolchain on every upgrade</td>
</tr>
<tr>
<td>**Qt licensing**</td>
<td>Falling back to Qt 5.15 is worse than a version downgrade: current 5.15 patch releases are commercial-only. Given that the plan deliberately relies on Qt shipping under LGPL, this reopens a licensing question that was already closed</td>
</tr>
<tr>
<td>**Security and transport**</td>
<td>Windows 7 left support in January 2020. It has no TLS 1.3, needs manual configuration for TLS 1.2, and requires specific updates just to validate a modern signature — including the self-signed one above</td>
</tr>
</table>
**So the question back to you, and it is a factual one, not a preference:** is there an actual Windows 7 machine that must run this, or was Windows 7 named as "support as far back as possible"?
- If **no real Windows 7 machine exists**, the minimum becomes **Windows 10, 64-bit**, and nothing else in the plan changes.
- If **a real Windows 7 machine exists**, the cheapest fix by a wide margin is to upgrade or replace **that one machine**, not to downgrade the entire technology stack of the project to accommodate it. Rebuilding on Qt 5.15 would cost more than the machine.
---
## Image and document storage
AVIF for bill photos is a good choice — it is the best compression available for photographs of paper, and a phone photo of a bill drops dramatically in size. Two things to settle around it.
### Format per artefact type
<table header-row="true">
<tr>
<td>Artefact</td>
<td>Format</td>
<td>Why</td>
</tr>
<tr>
<td>**Supplier bill photo**</td>
<td>**AVIF**, lossy, longest edge capped around 2000 px</td>
<td>It only has to be readable, not archival-perfect. Capping the dimension saves more than tuning quality does</td>
</tr>
<tr>
<td>**Thumbnail for lists**</td>
<td>**WebP or JPEG**, around 256 px, generated once and cached on disk</td>
<td>Deliberately a format Qt handles without any extra plugin, so a list never depends on the AVIF path working</td>
</tr>
<tr>
<td>**Signature capture**</td>
<td>**The stroke data itself** — the captured points — plus a small **PNG** render</td>
<td>**Never a lossy format.** A signature is thin dark lines on white, which is precisely what lossy compression smears. It is also evidence, and evidence should not be approximated. The stroke data is tiny and reproduces at any size</td>
</tr>
<tr>
<td>**Scan or photo of a signed agreement**</td>
<td>AVIF if it is a photo; the original file untouched if it arrives as a PDF</td>
<td>Never re-encode a document that arrived in a document format</td>
</tr>
<tr>
<td>**Design files**</td>
<td>Untouched, on shop volumes only</td>
<td>Unchanged decision. Never converted, never uploaded</td>
</tr>
</table>
### Where the bytes live
**Metadata in PostgreSQL, bytes in the object store, addressed by content hash.** The database row holds the hash, size, format, dimensions, who captured it, when, and what it is attached to. The object store holds exactly one copy of each distinct image, so photographing the same bill twice costs one copy.
Images do not go into PostgreSQL as blobs. On one spinning disk, that inflates the database, slows every backup, and makes restoring a small text record wait behind megabytes of photographs. The only bytes stored directly in the database are the genuinely tiny ones — signature strokes, at a few kilobytes.
### Two practical notes on AVIF
- **Encode on the server, not at the counter.** AVIF encoding is expensive, and an i5-7500 has no hardware AV1 acceleration — that arrived in much later Intel generations. The workstation uploads the photo as captured; a background worker on the server encodes it, stores the AVIF, and discards the original **only after** the AVIF is verified readable. A few seconds of encoding must never sit between the shopkeeper and the next customer.
- **Verify Qt can display AVIF before committing the viewer to it.** Qt handles PNG and JPEG natively and WebP through its extra image formats. AVIF support depends on the Qt version and on an image-format plugin backed by an AVIF library, and it must be confirmed for the exact Qt version chosen rather than assumed. This is why thumbnails are deliberately WebP or JPEG: if the AVIF viewer path needs an extra plugin bundled into the installer, that is a packaging task, not a broken feature list.
---
## Open items
1. **Windows 7.** Is there a real Windows 7 machine, or was that "as far back as possible"? This one blocks the toolchain baseline, so it should be answered before any code is written. *(See the section above.)*
2. **The original bill photo.** Once the AVIF exists and is verified, is the original capture deleted, or kept? Deleting saves real space on one HDD; keeping preserves the untouched evidence. My recommendation is to delete it, because a readable AVIF of a bill is sufficient for the purpose it serves — remembering what was paid to whom.
3. Still outstanding from earlier: the **agreement quantity consumption event** — consume at job creation and release on cancellation, or consume at invoice — and **how an emailed approval reply is handled**, whether the shopkeeper marks it approved by hand or the system eventually reads replies.
<callout icon="🧭">
	**Reading rule.** This page governs code organisation, module boundaries, safety enforcement, memory budgets, updates, and the build pipeline. For what the features do, the feature specification wins. For runtime architecture and sync behaviour, the runtime architecture page wins. Where this page states a technical constraint — a running executable cannot be replaced on Windows, a private release asset requires authentication, a workflow token cannot reach another private repository — that is a fact about the tools, not a choice to revisit.
</callout>
