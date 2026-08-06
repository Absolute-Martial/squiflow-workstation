# Phase 8 -- The server: 8.1-8.8 implementation plan

Status: planned, with the portable 8.3 token core already implemented. Hical
2.6.7 source is vendored but not linked. The current sandbox has no qualified
Boost/OpenSSL/zlib/Hical runtime lane, PostgreSQL, or real network gate. The
adapter and missing-provider boundaries are defined in
`phase-8-framework-and-provider-isolation.md`.

## Decisions this phase is built on

- **D1, decided by the shopkeeper: Hical, not Oat++.** Hical is the inbound
  HTTP adapter, not the server architecture. Only
  `server/src/adapters/http/hical/` may include Hical/Boost transport types.
  Its MySQL middleware, JWT ownership, configuration ownership, and logging
  ownership are disabled or adapted rather than adopted. The supplied 2.6.7
  archive is pinned by SHA-256; release requires an exact reproducible upstream
  commit/artifact. See ADR 0013 and the provider-isolation plan.
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
- Missing capabilities are independent adapters: libpqxx for PostgreSQL,
  libcurl for outbound HTTP/SMTP, libavif for conversion, and a replaceable
  blob store. Third-party types and error enums never cross those boundaries.
- A future Hical implementation of any missing capability must satisfy the
  existing SquiFlow port/conformance tests; callers and protocol payloads do
  not change.

## 8.1 -- Skeleton and configuration

**Goal:** a running Hical HTTP adapter over a framework-neutral endpoint core,
with health check, structured startup, and typed configuration -- no business
endpoints yet.

**Scope:** `server/CMakeLists.txt`, `server/src/main.cpp`, typed configuration,
framework-neutral `ApiRequest`/`ApiResponse`/`Problem`/`RouteSpec`, health and
readiness endpoints, plus `server/src/adapters/http/hical/` request/response
mappers, route registration, lifecycle, and error boundary. Reuse Phase 6.2
logging through an adapter. Hical headers are forbidden everywhere else.

**Invariants:** the server refuses to start with an incomplete or
malformed configuration, naming exactly which setting is missing/invalid --
no partial-startup "try it and see" behavior. Secrets never appear in
startup logs.

**Tests:** direct endpoint tests without Hical, Hical loopback conformance,
request-lifetime copying, normal startup, malformed/missing configuration,
database-unreachable readiness, concurrent health checks, provider include
boundary, and graceful `SIGTERM` shutdown. The same endpoint response must be
identical through direct and Hical paths.

## 8.2 -- PostgreSQL and migrations

**Goal:** the server-side schema and a migration runner sharing the
ordering discipline already proven in the workstation's Phase 3.3 runner,
adapted to PostgreSQL rather than SQLite.

**Scope:** a pinned libpqxx adapter under `server/src/adapters/postgres/`,
`server/src/storage/postgres_store.hpp/.cpp`,
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
content is detected and skipped. Hical's supplied multipart path buffers
complete bodies/parts, so it is capped to a measured small-body limit. Large-file
support requires the separate `UploadReceiver` spike/adapter described in the
provider-isolation plan.

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
actual release artifact so the update host is not hit once per workstation),
and a libcurl-backed `ArtifactFetcher` adapter. Hical owns inbound delivery
only.

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

**Scope:** `server/src/mail/send_worker.hpp/.cpp`, a libcurl-backed
`MailTransport` adapter for SMTP or an HTTP provider, and application code that
consumes `SendIntent` records synced from the workstation via 8.4 and records
delivery status back; `server/src/mail/template_render.hpp/.cpp` (renders the 5.8 `PreparedDocument` content to
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

Phase 8 is complete only when all eight sub-phases have gate documents from a
machine with PostgreSQL and the pinned Hical dependency graph, provider include
boundaries and conformance suites pass, Hical upstream tests pass in the
qualification lane, and Phase 9 CI builds/runs the server suite. Replacing Hical
or a missing-capability provider must require only an adapter/composition-root
change, never domain, protocol, workflow, endpoint, or migration changes.
