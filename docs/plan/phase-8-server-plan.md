# Phase 8 -- The server: 8.1-8.8 implementation plan

Status: planned, not started. None of this compiles in the current sandbox
(no Oat++, no PostgreSQL, no network). Every sub-phase below is written to
be compiled and gated on a machine that has those, and honestly marked
`[~]` here until it does.

## Decisions this phase is built on

- **D1, recommendation standing, not yet confirmed by the shopkeeper:** pin
  a specific Oat++ 1.4.0-branch commit through a vcpkg overlay port, never
  a floating branch. This blocks 8.1 (the server cannot be scaffolded
  without picking the framework version it links against). Do not start
  8.1 until D1 is confirmed.
- **Hical stays a Phase 8 adapter candidate, not a decided replacement.**
  If Hical is chosen over (or alongside) Oat++, it is bound to the sync
  transport adapter only; it must never leak into domain, workflows,
  protocol, or the PostgreSQL schema design, and its Boost.MySQL
  middleware is not used for the PostgreSQL design regardless of which
  HTTP framework wins.
- **D5, decided (from 5.8):** approval is by hand. 8.7 (mail) therefore
  sends only what a human already approved and explicitly requested to
  send -- it never infers approval from a reply.

## Shared ground rules for all of 8.1-8.8

- The server is a separate deployable from the workstation; it shares the
  protocol spine (module ids, rights, operation table) by depending on the
  same `external/protocol` headers, never by duplicating enum values.
- Every write endpoint re-runs the same authorization and offline-rule
  checks the workstation's `modules::Registry::run()` already enforces
  client-side -- the server is the authority, the client check is only a
  UX shortcut. Trusting the client is not an option anywhere in this
  phase.
- The server never becomes a second source of business logic. Where
  possible, the server reuses the engine/domain/workflow libraries as a
  linked library, compiled for a server target, rather than reimplementing
  rules in server-only code.
- All money, quantity, and identity types are the same Phase 2 types used
  workstation-side; the wire format is a serialization of those types, not
  a parallel type.

## 8.1 -- Skeleton and configuration

**Goal:** a running Oat++ (or chosen framework) HTTP server process with
health check, structured startup, and typed configuration -- no business
endpoints yet.

**Scope:** `server/CMakeLists.txt`, `server/src/main.cpp`,
`server/src/config.hpp/.cpp` (typed config: bind address, port, database
connection string, log level, secrets source -- loaded from environment
variables and/or a config file, never hardcoded), `server/src/health_
endpoint.hpp/.cpp` (`GET /health` returning process status, build version,
and database reachability), reuse of the Phase 6.2 logging library
compiled for a server (non-Windows) target.

**Invariants:** the server refuses to start with an incomplete or
malformed configuration, naming exactly which setting is missing/invalid --
no partial-startup "try it and see" behavior. Secrets never appear in
startup logs.

**Tests:** normal startup, missing required config key, malformed port/
address, database unreachable at startup (health check reports it, server
still starts so an operator can see the health endpoint), health check
under load (concurrent requests), graceful shutdown on `SIGTERM` closes
the DB pool cleanly.

## 8.2 -- PostgreSQL and migrations

**Goal:** the server-side schema and a migration runner sharing the
ordering discipline already proven in the workstation's Phase 3.3 runner,
adapted to PostgreSQL rather than SQLite.

**Scope:** `server/src/storage/postgres_store.hpp/.cpp`,
`server/src/storage/migration_runner_pg.hpp/.cpp` (reuse the Phase 3.3
migration *ordering and versioning rules*; do not invent a second
migration numbering scheme -- server migrations get their own numbered
series, e.g. `pg_001`, `pg_002`, distinct from the workstation's SQLite
migration numbers, since they are different schemas for different
databases), the server-side schema covering: identity/tokens (8.3), sync
cursor/sequence assignment (8.4), media metadata (8.5), and per-module
server tables only where the server needs authoritative state beyond what
sync already carries.

**Invariants:** every migration is forward-only and idempotent-checked (a
migration that has already run is detected and skipped, never re-applied);
a failed migration leaves the schema at the last successful version, never
half-applied; connection pooling never exceeds a configured hard cap.

**Tests:** normal migration run from empty schema, re-running migrations
is a no-op, a migration failure midway rolls back that single migration's
transaction, out-of-order migration files are rejected, pool exhaustion
under concurrent load is refused with a named error rather than an
unbounded queue.

## 8.3 -- Identity and tokens

**Goal:** device/user authentication for the sync protocol -- issuing,
validating, and revoking tokens tied to the Phase 2.7 session/rights model,
without re-deriving a second identity system.

**Scope:** `server/src/identity/token_issuer.hpp/.cpp`,
`server/src/identity/token_store.hpp/.cpp` (PostgreSQL-backed), token
rotation and revocation endpoints, device registration tied to the same
device concept the workstation's 6.4 single-instance/6.5 secrets already
assume exists per machine.

**Invariants:** tokens are opaque, high-entropy, and never derivable from
user/device identifiers; a revoked token is rejected immediately, not
eventually (no cache staleness window past a defined, tested bound);
token secrets at rest are hashed, never stored plaintext, mirroring the
workstation's 6.5 secrets discipline server-side.

**Tests:** normal issue/validate/revoke cycle, expired token rejected,
revoked token rejected immediately, malformed token rejected without
leaking why (no oracle for guessing valid tokens), concurrent revocation
and validation of the same token, token reuse after device re-registration
is explicitly a new token, never the old one un-revoked.

## 8.4 -- Sync endpoints, idempotency, sequence assignment

**Goal:** the server side of the outbox/cursor contract the workstation's
3.5/3.6 already define client-side -- accept pushed outbox batches, assign
server sequence numbers, and serve delta pulls.

**Scope:** `server/src/sync/push_endpoint.hpp/.cpp` (accepts a batch of
outbox entries, each with its client-generated idempotency key -- a
replayed push with the same key is a no-op, matching the workstation's own
retry-cannot-double-charge invariant from 3.5), `server/src/sync/pull_
endpoint.hpp/.cpp` (delta pull from a server-assigned sequence, matching
3.6's cursor contract exactly), `server/src/sync/conflict_resolution.hpp/
.cpp` (the owner's version wins, the losing version retained -- same rule
as 3.6, server-authoritative side).

**Invariants:** idempotency keys are unique per device, never reused
across devices to resolve a conflict; sequence numbers are strictly
monotonic and gap-free per tenant; a losing conflicting version is never
silently discarded -- it is retained exactly as 3.6 already specifies
client-side, now enforced server-side too.

**Tests:** normal push/pull cycle, replayed push with the same idempotency
key is a no-op, two devices pushing conflicting changes -- owner wins,
loser retained, concurrent pushes from many devices under load, pull from
a sequence number the tenant does not yet have (clamped, not an error
storm), malformed/oversized batch rejected without partial application.

## 8.5 -- Media, including the AVIF conversion worker

**Goal:** accept uploaded design files (Phase 4.13's `files` module
records) and produce AVIF-converted previews server-side, so the
workstation's 7.6 thumbnails do not require every client to have a heavy
codec stack.

**Scope:** `server/src/media/upload_endpoint.hpp/.cpp`,
`server/src/media/avif_worker.hpp/.cpp` (a bounded worker pool converting
uploaded originals to AVIF previews, mirroring the workstation's Phase 6.7
"bounded worker lanes, no service owning a thread" discipline server-side),
content-hash verification on upload matching the 4.13 identity rule
(device, volume, file id, content hash) so a re-upload of unchanged
content is detected and skipped.

**Invariants:** the worker pool has a hard concurrency and queue-depth
cap; an oversized or corrupt upload is rejected before conversion is
attempted, never allowed to crash a worker; a conversion failure is
recorded per file, retried a bounded number of times, then surfaced as a
named failure state rather than retried forever.

**Tests:** normal upload/convert cycle, duplicate upload by content hash
is a no-op, oversized upload rejected, corrupt/malformed image rejected,
queue-depth cap enforced under load, conversion failure retried and then
surfaced, concurrent uploads of the same file id from two devices.

## 8.6 -- The update proxy

**Goal:** the server endpoint the Phase 9.4 updater polls to learn about
and fetch new workstation releases, without the workstation ever reaching
out to a public update host directly (so releases can be gated/rolled back
server-side).

**Scope:** `server/src/update/manifest_endpoint.hpp/.cpp` (serves the
current release manifest: version, download URL, SHA-256, mandatory/
optional flag), `server/src/update/proxy_cache.hpp/.cpp` (caches the
actual release artifact so the update host is not hit once per
workstation).

**Invariants:** the manifest served is always the one this server
administrator has explicitly published, never automatically the newest
upstream release -- rollout control stays with the operator, matching the
updater's own "blocked while the outbox is not empty" caution from 9.4.
SHA-256 in the manifest is always verified against the cached artifact
before being served, never trusted from upstream metadata alone.

**Tests:** normal manifest fetch, artifact hash mismatch is refused rather
than served, cache miss triggers exactly one upstream fetch even under
concurrent requests (no thundering herd), manifest rollback to a prior
version is honored immediately.

## 8.7 -- Mail: prepared by the system, sent only on confirmation

**Goal:** the only thing in this entire project allowed to actually
transmit an email, and only for a `SendIntent` (from 5.8) that already
carries an explicit human confirmation.

**Scope:** `server/src/mail/send_worker.hpp/.cpp` (consumes `SendIntent`
records synced up from the workstation via 8.4, sends via a configured
SMTP/mail-API provider, records delivery status back), `server/src/mail/
template_render.hpp/.cpp` (renders the 5.8 `PreparedDocument` content to
an email body/attachment -- reusing the same document content the 7.5
renderer will eventually turn into a PDF, never a third independent
rendering of the same fields).

**Invariants:** a `SendIntent` is sent exactly once; a retried delivery
attempt after a transient provider failure uses the same idempotency
discipline as 8.4's push endpoint (a provider-side dedupe key derived from
the `SendIntent`'s own id), never a second independent send. Per D5, no
code path here parses a reply to infer approval -- approval already
happened by hand before the `SendIntent` ever reached the server.

**Tests:** normal send, provider transient failure retried without double
send, provider permanent failure surfaced as a named delivery-failed state,
malformed/incomplete `SendIntent` refused before attempting a send,
concurrent workers never double-send the same `SendIntent` (locking/claim
discipline tested under load).

## 8.8 -- Per-module endpoints

**Goal:** thin server-side endpoints for the handful of operations that
genuinely need server authority beyond generic sync -- e.g. the shared
document numbering sequence (Phase 2.5) when multiple devices must never
collide on the same number, and any online-only operation already flagged
as such in the Phase 1.4 operation table.

**Scope:** one endpoint per online-only/sync-online-required operation
identified in `external/protocol`'s operation table (e.g. the invoice
final-number reservation already described in 5.4, `order_cancel`,
agreement close/reopen) -- each endpoint re-runs the exact same rights and
offline-rule check the operation table declares, server-side, as the
authoritative check.

**Invariants:** an operation the protocol table marks `sync/offline` never
gets a server-only endpoint that bypasses offline capability -- only
operations already declared online-required in Phase 1.4/1.5 get one here.
No endpoint invents authorization rules the operation table does not
already declare.

**Tests:** one normal-path test per online-required operation, one
unauthorized-rights test per endpoint, one offline-rule-violation test
confirming the server, not just the client, refuses an operation the table
marks online-only, concurrent requests for the same numbered resource
(e.g. two devices racing for the same invoice number block) never produce
a duplicate number.

## Cross-cutting sequence for 8.1-8.8

| Order | Sub-phase | Depends on |
| --- | --- | --- |
| 1 | 8.1 Skeleton/config | D1 confirmed |
| 2 | 8.2 PostgreSQL/migrations | 8.1 |
| 3 | 8.3 Identity/tokens | 8.2 |
| 4 | 8.4 Sync endpoints | 8.2, 8.3 |
| 5 | 8.5 Media/AVIF worker | 8.4 (uses the same push/pull path for file records), 4.13 |
| 6 | 8.6 Update proxy | 8.1 only; can run in parallel with 8.4/8.5 |
| 7 | 8.7 Mail | 8.4 (SendIntent sync), 5.8 |
| 8 | 8.8 Per-module endpoints | 8.3, 8.4, and the specific module each endpoint belongs to |

Each sub-phase closes with: focused tests against a real PostgreSQL
instance (never an in-memory fake standing in for PostgreSQL-specific
behavior like sequence/lock semantics), the framework's own test runner,
a gate document under `docs/qa/`, and its own commit -- following the same
discipline as every prior phase, adapted to "real database, real network"
since that is what a server actually is.

## Acceptance criteria for closing Phase 8

Phase 8 is complete only when all eight sub-phases have gate documents
produced on a machine with PostgreSQL and the chosen HTTP framework
available, D1 (or its Hical alternative) is a confirmed, recorded decision
rather than a standing recommendation, and Phase 9's CI (9.1) can actually
build and run the server's own test suite, not just the workstation's.
