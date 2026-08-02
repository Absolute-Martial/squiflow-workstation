# SquiFlow — Runtime Architecture, Sync & Delivery

Source page id: c4f8998f6911462c819be5946ed68894

---

<callout icon="📌">
	**What this page is.** The confirmed runtime architecture, synchronization contract, background service registry, and build/delivery pipeline. Companion to the feature specification. **Rule for this page: every fact in the "Confirmed environment" table was stated by the shopkeeper. Nothing else is treated as decided.** Anything not stated is listed as an open question at the bottom rather than assumed.
</callout>
## Confirmed environment — stated, not inferred
<table header-row="true">
<tr>
<td>Element</td>
<td>Stated fact</td>
</tr>
<tr>
<td>**Workstation OS**</td>
<td>Windows</td>
</tr>
<tr>
<td>**Workstation delivery**</td>
<td>A single executable handed over directly. No app store, no shop-side build step</td>
</tr>
<tr>
<td>**Server OS**</td>
<td>Ubuntu Server</td>
</tr>
<tr>
<td>**Server storage**</td>
<td>HDD</td>
</tr>
<tr>
<td>**Server containerization**</td>
<td>**Every server-side service runs under Podman.** Not optional</td>
</tr>
<tr>
<td>**Backend framework**</td>
<td>Oat++</td>
</tr>
<tr>
<td>**Build environment**</td>
<td>GitHub Actions. Nothing is compiled on shop hardware</td>
</tr>
<tr>
<td>**Workstation runtime shape**</td>
<td>Qt UI **plus an embedded Oat++ local engine** running as a background thread inside the same process</td>
</tr>
<tr>
<td>**Transport**</td>
<td>HTTPS and WebSockets between local engine and remote backend</td>
</tr>
<tr>
<td>**Workstation hardware**</td>
<td>8 GB DDR4, Intel Core i5-7500, HDD</td>
</tr>
</table>
## Topology
```plain text
┌──────────────────────────────────────────┐
│  Ubuntu Server  —  everything in Podman  │
│                                          │
│   Caddy (TLS)                            │
│     └─ Oat++ remote backend              │
│          └─ PostgreSQL (system of record)│
│          └─ object store (small docs)    │
│   WAL archive → offsite backup           │
└────────────────▲─────────────────────────┘
                 │  HTTPS + WSS
                 │  (idempotent, delta, chunked)
                 ▼
┌──────────────────────────────────────────┐
│  Windows Workstation  —  one executable  │
│                                          │
│   Qt UI  ──in-process calls──┐           │
│                              ▼           │
│   Oat++ local engine (background thread) │
│     └─ SQLite (WAL) local store + outbox │
│     └─ Sync Orchestrator                 │
└──────────────────────────────────────────┘
```
---
## Storage — the question from the planning conversation, now closed
The earlier debate was whether SQLite or PostgreSQL is the system of record. This topology answers it without compromise, because there are two machines.
<table header-row="true">
<tr>
<td>Store</td>
<td>Role</td>
<td>What happens if it is lost</td>
</tr>
<tr>
<td>**PostgreSQL on the Ubuntu server, in Podman**</td>
<td>**The system of record.** Exact `NUMERIC` money, row-level security, WAL archiving for point-in-time recovery, page checksums</td>
<td>Restore from WAL archive to any moment. This is the copy that must never be lost, and the reason for choosing Postgres over a file database</td>
</tr>
<tr>
<td>**SQLite on the workstation, WAL mode**</td>
<td>Local store for offline work plus the outbox. Authoritative **for writes that have not yet synced**, a replica for everything else. Per answer 2 it is **load-bearing, not a disposable cache**</td>
<td>Anything already acknowledged by the server re-syncs. **Unsynced work would be lost**, which is why the workstation gets its own snapshot backup and a startup integrity check rather than relying on "it will sync eventually"</td>
</tr>
</table>
**Consequence of the HDD on the server.** Postgres on spinning disk is dominated by random I/O and WAL `fsync` latency, so: put the WAL on a separate physical disk if a second one exists, raise `checkpoint_timeout` and `max_wal_size` so checkpoints are infrequent and large rather than frequent and small, keep `shared_buffers` generous because every cache miss is a seek, and never disable `fsync` to compensate. If the server is ever upgraded, an SSD there buys more than any other change.
**Non-negotiable regardless of engine.** Automatic backups to a second physical device and offsite, the data directory excluded from antivirus and never inside a synced folder, an integrity check at startup that refuses to open a damaged store silently, and **a restore that has actually been rehearsed**. The parent plan already lists the unrehearsed restore as an open risk; it outranks every choice on this page.
---
## The local engine boundary — the most important design call here
The workstation runs an embedded Oat++ engine, and the temptation is to have the Qt UI talk to it over `localhost`. That would mean every screen turns C++ structures into JSON, pushes them through a loopback socket, and parses them back. On this hardware that is pure waste, and it puts an HTTP listener on a shop PC for no benefit.
**Decision: the local engine is linked in as a library, not reached over the network.**
<table header-row="true">
<tr>
<td>Path</td>
<td>Mechanism</td>
</tr>
<tr>
<td>UI → local engine (every screen, every write)</td>
<td>Direct in-process calls plus Qt signals and slots. No serialization, no sockets</td>
</tr>
<tr>
<td>Local engine → remote backend</td>
<td>Oat++ HTTP client and WebSocket client. This is the only place a wire format exists</td>
</tr>
<tr>
<td>Loopback HTTP listener</td>
<td>**Off by default.** Only enabled if something genuinely external needs it — an embedded web view, a second local tool. When enabled it binds to **port 0** so the OS assigns a free ephemeral port, and the engine reads the assigned port back and hands it to the UI. Nothing is hardcoded to 8080</td>
</tr>
</table>
**Single writer rule.** All database access — UI reads, UI writes, and the sync worker — goes through the local engine, which owns exactly one write connection. WAL mode allows concurrent readers, `busy_timeout` is already set to 5000 ms in the workstation defaults, and no code outside the engine opens the SQLite file. Serialising writes at the engine is what actually prevents lock errors; WAL alone does not.
**Single instance rule.** The application takes a named lock at startup. A second copy either focuses the running window or refuses to start. Two processes sharing one store on a shop PC is a corruption path, not a feature.
---
## Synchronization contract
### Idempotency
Every mutation carries a client-generated UUID `idempotency_key`. The remote backend stores it and, on a repeat of the same key, returns the original result instead of creating a second record. This is what makes a timeout safe to retry, and it matters most for the records that must never duplicate: issued invoices, allocated payments, and consumed agreement quantity.
### Cursor — one deliberate change from the proposal
A millisecond `last_synced_timestamp` is not safe as a sync cursor. Two rows can share a millisecond, and workstation and server clocks drift. **The server assigns a monotonic change sequence number**, the client stores the last sequence it has fully applied, and delta pulls ask for everything after that sequence. Wall-clock timestamps remain in the data as information; they are never the cursor.
### The outbox
States: `pending → in flight → acknowledged → applied`, plus `conflicted` and `failed`. Ordering is preserved per dependency chain so a child record never arrives before its parent — a payment allocation cannot land before its payment. Batches of 50 to 100 records per iteration, so a workstation that has been offline for weeks drains steadily rather than timing out on one enormous request.
### The Sync Orchestrator
One dedicated C++ manager, and the only component allowed to start network work. It owns the network state, guarantees a single sync operation at a time, applies backoff, and cancels cleanly on sleep and shutdown. Everything else that wants to sync raises a request to it.
### Conflicts
Detected by comparing the server's version of a record against the version the client last applied. Automatic merge is not attempted on anything financial. A conflict becomes an attention item for the shopkeeper, consistent with the rule that every necessary decision is human-supervised.
---
## Network adaptivity
Detection uses Qt's native operating-system hooks — `QNetworkInformation` for reachability, transport type, and metered status — rather than polling the server to discover whether it is reachable.
<table header-row="true">
<tr>
<td>Network state</td>
<td>Detection</td>
<td>Sync strategy</td>
<td>Resource behaviour</td>
</tr>
<tr>
<td>**Excellent / LAN**</td>
<td>Low latency, unmetered</td>
<td>Real-time push and pull</td>
<td>Persistent WebSocket. Attachments and files sync immediately</td>
</tr>
<tr>
<td>**Metered / mobile hotspot**</td>
<td>OS reports a metered connection</td>
<td>Core records only</td>
<td>Pause large asset transfer and log upload. Text-sized changes only</td>
</tr>
<tr>
<td>**Weak / flaky**</td>
<td>Repeated timeouts, high loss, high latency</td>
<td>Exponential backoff</td>
<td>**Close the WebSocket** rather than let it reconnect in a loop. Occasional HTTP batch checks at 5s → 10s → 20s → 40s, capped at 5 minutes</td>
</tr>
<tr>
<td>**Offline**</td>
<td>No reachable interface, or DNS failure</td>
<td>Local queue only</td>
<td>All network threads stopped. The application runs entirely from the local store, and says so plainly in the interface</td>
</tr>
</table>
The transition rule that matters: the WebSocket is the fast path, never the correctness path. Anything that arrived only over the socket must also be discoverable by a delta pull, so a missed push is invisible rather than a lost record.
---
## Background service registry
Every background service is listed with its trigger, where it runs, and its budget. A service not on this list does not exist — this is how the idle-CPU and memory targets stay true.
<table header-row="true">
<tr>
<td>Service</td>
<td>Trigger</td>
<td>Runs in</td>
<td>Budget / rule</td>
</tr>
<tr>
<td>**Presence and latency**</td>
<td>WebSocket ping/pong when connected; OS network events when not</td>
<td>Local engine</td>
<td>**No periodic polling of the server.** Reachability comes from the OS, not from traffic</td>
</tr>
<tr>
<td>**Outbox drain**</td>
<td>New outbox row, network becomes available, or idle</td>
<td>Local engine</td>
<td>Serialised. Chunked. Never concurrent with itself</td>
</tr>
<tr>
<td>**Delta pull**</td>
<td>WebSocket push, reconnection, application start, and a slow safety interval</td>
<td>Local engine</td>
<td>Cursor-based. Never a full dataset download</td>
</tr>
<tr>
<td>**Attachment transfer**</td>
<td>Queued, gated by network class</td>
<td>Local engine</td>
<td>Suspended on metered and weak connections</td>
</tr>
<tr>
<td>**Local maintenance**</td>
<td>Zero user activity, on a long interval</td>
<td>Local engine</td>
<td>Purge expired temporary rows and old logs. `VACUUM` and index rebuild only when idle and on mains power, never during shop hours</td>
</tr>
<tr>
<td>**Integrity check**</td>
<td>Application start</td>
<td>Local engine</td>
<td>Refuses to open a damaged store silently</td>
</tr>
<tr>
<td>**Tray and lifecycle**</td>
<td>OS sleep, wake, session end, window close</td>
<td>Qt</td>
<td>Pause and resume network threads safely. Flush and close the store before sleep</td>
</tr>
<tr>
<td>**Token renewal**</td>
<td>Ahead of expiry</td>
<td>Local engine</td>
<td>Refresh in the background so the shopkeeper never meets an unexpected login</td>
</tr>
<tr>
<td>**Attention rule evaluation**</td>
<td>On relevant data change, plus once at day rollover</td>
<td>Local engine</td>
<td>Deterministic and local. No network dependency</td>
</tr>
</table>
---
## Secrets and security
- **No token, password or key in plaintext**, and none in an ordinary SQLite table. Remote credentials are encrypted with the Windows Data Protection API so they are bound to the operating-system login session.
- **Dependency caution:** the commonly suggested keychain wrapper is a third-party library, not a Qt module. Since Qt licensing is already a tracked risk in the parent plan, calling the Windows API directly avoids adding another licence to audit for a small amount of code.
- Server side, TLS terminates at Caddy, and PostgreSQL row-level security is already the locked tenancy boundary.
- The protocol's permission set stays explicit and versioned, as it is today.
---
## Wire format
- **Control and human-visible payloads: JSON.** Debuggable, and the volume is trivial.
- **Bulk synchronization: a compact binary encoding, and MessagePack is the pragmatic choice.** It needs no schema compiler, maps cleanly onto existing structures, and can be introduced one endpoint at a time.
- **Correction to a common claim:** Oat++ does not ship a native Protocol Buffers mapper. Its object mapper is JSON-oriented, so any binary format means writing a mapper or handling the body directly. That is a real cost, which is why binary is worth doing for bulk sync and not worth doing for everything.
- **Compression by network class:** on for large text payloads over metered or slow links, off on LAN where it only burns CPU.
- **Zero-copy handling** of large bodies to avoid duplicating strings — with the caveat that a buffer must not be referenced after the connection that owns it is gone. This is a known source of late-surfacing crashes.
---
## Build and delivery — GitHub Actions
Nothing compiles on shop hardware. This removes the compile-time problem that the i5-7500 and the HDD would otherwise create.
<table header-row="true">
<tr>
<td>Job</td>
<td>Runner</td>
<td>Produces</td>
</tr>
<tr>
<td>**Workstation build**</td>
<td>Windows runner</td>
<td>The Windows deliverable. Prebuilt Qt installed by action rather than built from source. vcpkg binary caching and a compiler cache, both persisted in Actions cache</td>
</tr>
<tr>
<td>**Server build**</td>
<td>Ubuntu runner</td>
<td>The Oat++ backend container image, pushed to a registry. The server pulls it and runs it under Podman with Quadlet unit files</td>
</tr>
<tr>
<td>**Test lanes**</td>
<td>Both</td>
<td>Protocol contract tests, plus the server built in Debug, Release, and with the address and undefined-behaviour sanitizers, as the current build results already do</td>
</tr>
<tr>
<td>**Compatibility gate**</td>
<td>Any</td>
<td>A change to the protocol's wire major version fails the build unless it is accompanied by a deliberate version bump. Prevents a workstation executable meeting an incompatible backend</td>
</tr>
</table>
### Two delivery realities worth stating plainly
1. **"A single executable" is a folder or an installer, not literally one file.** The locked decision to ship Qt dynamically linked under LGPL means the Qt libraries travel alongside the executable, together with their licence notices and the ability to relink. The practical deliverable is a self-contained directory or an installer that needs nothing downloaded at run time and contains no embedded browser engine.
2. **An unsigned Windows executable triggers operating-system warnings** on a fresh machine. Not a blocker for a shop that receives the file directly from you, but it becomes one the moment a second customer does, so a code-signing certificate belongs on the same list as the Qt commercial licence — a cost gated to the point of selling to someone else.
---
## Trapdoors — adopted, with the ones that were missing
<table header-row="true">
<tr>
<td>Trap</td>
<td>Handling</td>
</tr>
<tr>
<td>Fixed local port collides, or a second instance fails</td>
<td>Bind to port 0 if a listener exists at all; prefer no listener</td>
</tr>
<tr>
<td>Database busy errors under concurrent access</td>
<td>Single writer inside the engine, WAL mode, busy timeout already configured</td>
</tr>
<tr>
<td>Loopback HTTP overhead for ordinary UI work</td>
<td>In-process calls instead</td>
</tr>
<tr>
<td>Duplicate records after a retried request</td>
<td>Idempotency keys</td>
</tr>
<tr>
<td>Sync worker and UI corrupting shared state</td>
<td>One orchestrator, one write path</td>
</tr>
<tr>
<td>Weeks of offline work overwhelming a request</td>
<td>Chunked batches</td>
</tr>
<tr>
<td>**WAL growth on an HDD**</td>
<td>Checkpoint tuning on the server; on the workstation, checkpoint during idle so a shop-hours write never waits on a seek</td>
</tr>
<tr>
<td>**Antivirus scanning the store mid-write**</td>
<td>Exclude the data directory. Document it as part of installation</td>
</tr>
<tr>
<td>**The store placed in a cloud-synced folder**</td>
<td>Refuse to run from one. This destroys file databases routinely</td>
</tr>
<tr>
<td>**Clock skew between workstation and server**</td>
<td>Server-assigned sequence numbers as the sync cursor, never client wall-clock time</td>
</tr>
<tr>
<td>**Abrupt shutdown mid-write**</td>
<td>Durable write settings on both stores. Power loss is the expected case, not the exceptional one</td>
</tr>
<tr>
<td>**A push arriving but never being reconciled**</td>
<td>Every pushed change must also be reachable by a delta pull</td>
</tr>
</table>
---
## Environment answers — now stated
<table header-row="true">
<tr>
<td>#</td>
<td>Question</td>
<td>Answer</td>
</tr>
<tr>
<td>1</td>
<td>Server location and how common is offline</td>
<td>**Offline is a rare state.** The server is normally reachable, so the real-time path is the common case and offline is genuine insurance rather than the daily mode</td>
</tr>
<tr>
<td>2</td>
<td>Is the server always on</td>
<td>**The local store is both a convenience and load-bearing.** It is not treated as a disposable cache: unsynced local work must survive on its own</td>
</tr>
<tr>
<td>3</td>
<td>Concurrent devices and users</td>
<td>**Two devices, two people: one for the shop owner, one for a staff member.** Revised from the earlier single-operator answer. These are two genuine simultaneous users with different rights</td>
</tr>
<tr>
<td>4</td>
<td>Windows target</td>
<td>**Windows only, 64-bit only.** One build target</td>
</tr>
<tr>
<td>5</td>
<td>Server specification</td>
<td>*Still to be pinned down. Postgres is tuned conservatively until then*</td>
</tr>
<tr>
<td>6</td>
<td>Second physical disk on the server</td>
<td>**No.** One HDD holds data and WAL together</td>
</tr>
<tr>
<td>7</td>
<td>Container registry</td>
<td>**GitHub Container Registry, private.** The server pulls from it directly</td>
</tr>
<tr>
<td>8</td>
<td>Design files</td>
<td>**They stay only on shop volumes.** No copies on the server</td>
</tr>
</table>
---
## What those answers change
### Answer 3 is the significant one: numbering can no longer be purely local
Two devices that can each work offline can each mark an invoice issued while disconnected — the owner at the counter and the staff member on the other machine, at the same moment. The locked rule is that the final number is assigned at issue and never reused, so two devices independently choosing "the next number" would collide, and one of them would have to be renumbered after the customer already had the paper. That is unacceptable.
**Resolution: reserved number blocks per device.**
- Each device registers and is allocated blocks of numbers per series from the server in advance — for example a block of 100.
- Issuing offline consumes from the device's own block, so the number is final and unique the moment the customer receives it. No provisional numbers, no renumbering.
- The Sync Orchestrator tops the block up whenever it is online and the block is running low, so a device is never caught offline with an empty block.
- Gaps between blocks are expected and legitimate. Series are gapless *per device*, not globally, and the numbering pattern should encode the device so a gap is explainable rather than alarming.
### Other consequences of three devices
<table header-row="true">
<tr>
<td>Area</td>
<td>Consequence</td>
</tr>
<tr>
<td>**Conflict handling — detection plus one resolution screen**</td>
<td>Two people on two devices means genuinely simultaneous edits are now possible, and offline divergence still is. Keep per-record version comparison with the server as arbiter, and build **one honest "these two disagree, choose which wins" screen** resolved by whoever holds the right. Still no field-level three-way merge and no automatic merge on anything financial — two users is not enough concurrency to justify that machinery</td>
</tr>
<tr>
<td>**Credit limit and agreement quantity**</td>
<td>Now a realistic case, not a hypothetical: the staff member and the owner can each accept work that individually fits a credit limit or a quantity cap and jointly breaches it. The check is advisory on the device and **re-validated on the server at sync**; a breach discovered on sync becomes an attention item for the owner, never a silent rejection of work already promised</td>
</tr>
<tr>
<td>**Audit**</td>
<td>Every record now carries **who** did it as well as which device. With two people this is real accountability, and it is the reason sign-in exists at all</td>
</tr>
<tr>
<td>**Permissions — no longer postponed**</td>
<td>The condition that justified deferring them ("one shopkeeper, no staff") no longer holds. Sign-in, enforced rights, roles, and permission-aware navigation move into the first wave. The named-rights groundwork already specified is what makes this configuration rather than a rewrite</td>
</tr>
<tr>
<td>**Sensitive actions on the staff device**</td>
<td>Rights the staff member does not hold are hidden or blocked with a stated reason, never silently failing. A blocked attempt can raise a request to the owner rather than dead-ending. Enforcement happens in the local engine **and again on the server at sync**, because a client-side check alone is advice, not a rule</td>
</tr>
<tr>
<td>**Payment allocation**</td>
<td>Server-side invariants on allocation totals stay, because they protect against a retried request and a diverged device just as much as against two users. They are cheap and they guard money</td>
</tr>
</table>
### Answer 2 — the local store needs its own protection
Because unsynced local work is load-bearing, the workstation gets its own backup path rather than relying on "it will sync eventually": a periodic local snapshot of the SQLite store to a second location on the machine or an attached drive, taken with an online-backup mechanism rather than a file copy, plus an integrity check at startup. Only work that has already been acknowledged by the server is safe to lose locally.
### Answer 6 — no WAL separation, so tuning shifts
One HDD carries both data and WAL, so the earlier advice to split them does not apply. Instead: fewer, larger checkpoints; generous `shared_buffers` because every miss is a seek; `fsync` never disabled; and backups must leave the machine entirely, because a single disk means a single point of failure for both the data and its write-ahead log. **The offsite copy is not optional here — it is the only real redundancy the server has.**
### Answer 7 — private registry means the server needs credentials
A private GitHub Container Registry pull requires a read-only token on the server, stored as a systemd credential and referenced by the Quadlet unit's auth file — never baked into an image or left in a plain file. Images are pulled **by digest** so a re-pull cannot silently deliver something different, and the unit tolerates a failed pull by continuing to run the existing image rather than leaving the shop with nothing.
### Answer 8 — the server object store shrinks to almost nothing
Design files never leave the shop, so the server holds only small documents: signature captures, photos of supplier bills, and generated PDFs if they are kept at all. That collapses bandwidth and backup size, and it means the design-file index on the workstation is authoritative with no server-side mirror to reconcile against.
### Answer 4 — one build target
Windows 64-bit only means a single Actions matrix entry, one compiler baseline, and no 32-bit compatibility work. The minimum Windows build still needs pinning, since it sets the runtime and API floor.
---
## Remaining open items
1. **Server specification** — pin down memory, CPU and disk size before Postgres is tuned for real.
2. **Minimum Windows version** — sets the compiler and API baseline.
3. **Do signature captures and supplier bill photos sync to the server, or stay local like design files?** They are small, and having them off-machine is what makes them survive a disk failure — but the rule "files stay on shop volumes" could be read either way. Recommendation: sync these, because they are evidence, and evidence that only exists on one HDD is not evidence.
<callout icon="🧭">
	**Reading rule.** This page governs runtime shape, synchronization, background services and delivery. The feature specification page governs what the features are and how they are used. The parent implementation plan governs language, library, phase and licensing decisions.
</callout>
