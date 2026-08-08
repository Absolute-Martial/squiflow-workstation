# Phase 8 workstation/server protocol and communication plan

Status: implementation contract for Phase 8. This plan resolves how the Windows
workstation, Ubuntu server, shared protocol package, local SQLite store, and
PostgreSQL authority communicate. It is subordinate to the compiled protocol
definitions and accepted ADRs; it updates older architecture prose that still
mentions Oat++.

## Source-of-truth order

When documents disagree, use this order:

1. compiled definitions and tests under `external/protocol`;
2. accepted ADRs, especially 0013-0015;
3. `phase-8-workstation-server-protocol-plan.md` and `phase-8-server-plan.md`;
4. `phase-8-framework-and-provider-isolation.md`;
5. `phase-8-backend-capability-map.md`;
6. open-source research documents;
7. older `docs/notion-plan` material, which is historical context only.

The earlier Notion runtime document names Oat++ as the selected framework.
Decision D1 and ADR 0013 supersede that part: **Hical 2.6.7 is the inbound
server adapter**. The workstation UI still talks to its local application
services directly in process; it does not run or call a localhost HTTP server.

## One architecture, three boundaries

```text
Qt/QML UI
  -> QML bridges (GUI-thread presentation only)
  -> AuthenticatedWorkspace/application services
  -> engine + module registry + SQLite/WAL + transactional outbox
  -> SyncOrchestrator + workstation transport adapter
  -> HTTPS/WSS
  -> Caddy TLS/reverse-proxy edge
  -> Hical request/response adapter
  -> transport-neutral server endpoint core
  -> shared engine/workflow rules + server application services
  -> libpqxx adapter -> PostgreSQL system of record
  -> durable jobs -> mail/media/webhook/blob provider adapters
```

Boundary rules:

- UI to local engine is a direct C++ call plus Qt signals/slots; no JSON and no
  loopback socket.
- The workstation transport and server HTTP adapter are the only wire-format
  boundaries.
- Hical, Boost, libpqxx, curl, libavif, ClamAV, queue, storage, and telemetry
  types never enter domain/application headers.
- The server re-runs authorization and domain/workflow rules. Client checks are
  for safe UX, never trust evidence.
- PostgreSQL is authoritative for acknowledged data. SQLite is authoritative
  for unsynced local work and a replica for acknowledged remote work.

## Shared protocol authority

Both executable targets link the same `external/protocol` target. The following
must never be copied into server-local enums or route constants:

- wire major/minor version;
- module ids and dependency graph;
- right ids;
- operation ids, names, owning modules, rights, sync classes, offline rules;
- workflow ids and requirements.

Current baseline is wire `0.1`; the protocol test prints the exact current
counts. Documentation must not freeze counts that can change—CI captures them
from the compiled table.

Compatibility policy:

- different wire major: refuse sync before authentication/mutation;
- same major, server minor newer: permit only capabilities advertised in the
  handshake and understood by the client;
- unknown operation/module/right: reject explicitly, never coerce or ignore;
- numeric operation id and optional diagnostic name must agree when both occur;
- a server must support the current workstation and one approved previous
  minor during rolling upgrades, with contract tests proving the matrix;
- removals or semantic reinterpretations require a major version change.

## Connection establishment

### 1. Local startup

1. Discover secure paths and initialize logs/crash handling.
2. Acquire the single-instance lock.
3. Open SQLite, run migrations/integrity checks, and load device identity.
4. Authenticate the local person and construct `AuthenticatedWorkspace`.
5. Load the encrypted remote token from the OS secret provider.
6. Start `SyncOrchestrator` only after local startup succeeds.

The desktop remains usable offline. Network failure must not block opening
locally authorized offline-capable pages.

### 2. Server discovery and handshake

Planned control route: `GET /api/v1/capabilities`.

The safe JSON response includes:

- wire major/minor and server build identifier;
- enabled module ids and module manifest versions;
- accepted content types/codecs;
- maximum push batch/body and pull page sizes;
- upload limits and supported media capabilities;
- realtime support and heartbeat bounds;
- server time for diagnostics only—not for cursor ordering.

It contains no tenant existence oracle, user information, secrets, database
state, private host paths, or internal stack details.

### 3. Authentication

Planned routes:

- `POST /api/v1/auth/device/register` for an explicitly authorized first
  registration;
- `POST /api/v1/auth/token/rotate` for rotation;
- `POST /api/v1/auth/token/revoke` for explicit revocation.

Tokens are opaque, high-entropy bearer values. The workstation stores them in
the OS secret provider; the server stores only a cryptographic hash plus
bounded metadata. TLS is mandatory. Revocation takes effect immediately within
the tested bound. Error messages do not reveal whether a token, tenant, device,
or username exists.

Each authenticated request resolves a server-side principal containing tenant,
person, device, current rights, module activation, and token generation. The
server never accepts rights, tenant scope, or activation claimed by the body.

## Request envelope and fault model

Every control/sync request carries bounded transport metadata:

- wire major/minor;
- request/correlation id;
- client build and device id from authenticated context;
- declared content type and optional content encoding;
- body length covered by the route limit.

Each mutation item carries:

- client-generated idempotency key;
- operation id and optional diagnostic operation name;
- record id;
- base record version/sequence last applied by the client;
- causation/dependency keys when ordering requires a parent first;
- typed payload bytes encoded by the negotiated codec.

Stable failures use a SquiFlow-owned RFC 9457-style `Problem` shape with HTTP
status, stable machine code, safe message, correlation id, retryability, and
bounded field errors. Provider exceptions, SQL text, host paths, secrets, stack
traces, and Hical/Boost/PostgreSQL error enums are never returned.

## Codec plan

- JSON is the first control/debug codec and the mandatory conformance baseline.
- MessagePack is the candidate bulk-sync codec only after a measured adapter and
  golden-vector suite exist.
- Domain and endpoint code consume SquiFlow DTOs, never a JSON/MessagePack
  library value.
- A `WireCodec` port owns encode/decode, depth/size/type limits, canonical test
  vectors, malformed-input behavior, and round trips.
- Content compression is negotiated and bounded. Decompression ratio and final
  size are checked before allocation-heavy parsing.
- Media/blob bytes are never embedded in a sync mutation payload; they use the
  staged upload protocol.

## Idempotent push

Planned route: `POST /api/v1/sync/push`.

1. Workstation claims 1-100 due outbox entries, preserving dependency and
   same-record order.
2. It sends one bounded batch. A timeout leaves entries retryable with the same
   idempotency keys.
3. Server authenticates, checks wire compatibility, request limits, module
   activation, operation mapping, right, and online/offline policy.
4. Server deduplicates by `(tenant, device, idempotency_key)` and returns the
   original result for a replay.
5. Server runs the shared operation/workflow through an authoritative
   transaction, stores audit/change data, and assigns a tenant-global monotonic
   sequence.
6. Server commits business state, idempotency result, audit, and change log in
   one database transaction.
7. Response gives one result per item: applied, replayed, conflicted, refused,
   or retryable failure, plus sequence/version when applicable.
8. Workstation acknowledges/applies results transactionally and retains losing
   conflict versions for human attention.

A malformed or oversized batch is rejected before partial execution. A valid
mixed batch may use explicit per-item results only if transaction grouping and
dependency rules are preserved; financial cross-record workflows stay atomic.

## Cursor delta pull

Planned route: `POST /api/v1/sync/pull` so cursor sets and module filters remain
in a bounded authenticated body.

- Server change sequence is tenant-global and strictly increasing.
- The workstation stores a cursor per module so disabled or rarely used modules
  can lag independently.
- Each requested module provides `after_sequence` and a page limit.
- Server returns ordered changes and a scanned high-water mark.
- Client applies every returned change and advances that module cursor in the
  same SQLite transaction.
- If apply fails, the cursor does not advance.
- Empty pages may advance to a verified high-water mark so unrelated module
  sequences do not force endless rescans.
- Retention gaps return a named `snapshot_required` result; they never silently
  skip history.

Snapshot/bootstrap is a separate bounded, resumable protocol with a consistent
snapshot token. It must not masquerade as an ordinary delta page.

## Conflict policy

- Compare the server record version with the base version the client last
  applied.
- Financial records never receive field-by-field automatic merging.
- Owner-authoritative rules already encoded in the engine are reused.
- The losing version, payload, actor/device, reason, and sequence are retained.
- Conflict resolution creates a new audited mutation; it never rewrites
  history.
- The workstation exposes unresolved conflicts as attention items.

## Realtime channel

Planned route: authenticated `WSS /api/v1/realtime` through Hical.

WebSocket/SSE messages are hints only: tenant-safe notification type, affected
module, and highest available sequence. They trigger a normal delta pull.
Correctness never depends on receiving a socket message. Reconnect resumes from
SQLite cursors, uses bounded exponential backoff, and closes on weak/offline
network state rather than reconnecting in a loop.

Realtime must have bounded sessions, heartbeat, outbound queue, message size,
fan-out, and slow-consumer eviction. Token revocation closes the session.

## File and media transfer

1. Create or discover file metadata through normal operations.
2. Ask the server for a bounded upload session containing upload id, limits,
   expected hash/size, and expiry.
3. Stream/stage bytes through the approved `UploadReceiver`; Hical's buffered
   multipart path is restricted to its measured safe small-body limit.
4. Verify size/hash, quarantine, scan, and atomically publish a blob id.
5. Enqueue AVIF preview conversion on the durable worker.
6. Publish status/change records; workstation learns completion via delta pull.

Partial files, failed scans, corrupt images, expired sessions, and abandoned
uploads are cleaned by durable bounded jobs. Blob provider URLs never become
business-record identity.

## Network behavior

- LAN/unmetered: immediate small sync and realtime hints.
- Metered: core records continue; large media/log transfer pauses.
- Weak/flaky: close realtime, use exponential HTTPS retries with jitter.
- Offline: no network work; continue local operations allowed by the compiled
  operation table.
- Sleep/shutdown: cancel network work, safely return in-flight outbox entries to
  retryable state, flush SQLite, and release resources in reverse order.

Only the SyncOrchestrator initiates workstation network operations. UI code and
individual modules cannot create ad-hoc HTTP clients.

## Required implementation targets

Workstation:

- `squiflow_workstation_sync_core`: orchestrator, DTOs, batch/cursor policy;
- `squiflow_workstation_wire`: codec and problem mapping;
- one HTTPS/WSS client adapter behind owned transport ports;
- deterministic fake transport for portable tests.

Server:

- `squiflow_server_core`: route catalog, DTOs, auth context, services;
- `squiflow_server_http_hical`: inbound Hical mapping only;
- `squiflow_server_pg_libpqxx`: PostgreSQL stores/migrations;
- `squiflow_server_io_curl`, `squiflow_server_avif_libavif`, blob and scan
  adapters;
- `squiflow_server` and `squiflow_worker` composition roots.

Shared:

- `external/protocol` definitions;
- codec-neutral contract vectors and compatibility fixtures;
- no Qt, Hical, PostgreSQL, or provider dependency.

## Cross-repository/protocol change procedure

1. Add protocol definition and compile-time/runtime invariants first.
2. Add golden request/response/problem vectors.
3. Add server-core behavior against direct endpoint calls.
4. Add workstation fake-transport behavior.
5. Add Hical loopback conformance using the same vectors.
6. Add PostgreSQL integration and concurrent/idempotency tests.
7. Test current/current and approved previous/current compatibility.
8. Update docs and capability map.
9. Commit in protocol -> server core -> adapters -> workstation order.
10. Never deploy a server that requires a protocol the published workstation
    does not understand.

## Phase 8 test matrix

- portable: protocol enums/tables, DTO validation, codecs, endpoint core, fake
  stores/transports, retry/backoff/cancellation, malformed/fuzz corpora;
- Linux: Hical upstream qualification, loopback HTTP/WSS, PostgreSQL 18,
  libpqxx, migrations, RLS, concurrent sync, worker shutdown;
- Windows: workstation HTTPS/WSS adapter against the same contract server;
- integration: duplicate pushes, reply loss, reordering, partial disconnect,
  token expiry/revocation, two-device conflicts, retention gap/bootstrap;
- security: cross-tenant ids, escalated rights in body, decompression bombs,
  oversized/deep payloads, slow clients, SSRF destinations, path traversal,
  malicious files, log/Problem redaction;
- recovery: crash between database stages, worker lease expiry, backup plus
  restore rehearsal, cursor/outbox continuity after restore;
- performance: HDD-aware p50/p95/p99 latency, bounded pool/queue depth, memory
  under maximum batches/uploads, and no unbounded retry storm.

## Open-source reference compass

SquiFlow uses reference systems to find missing capabilities, not to copy their
product architecture.

- **Twenty CRM:** copy the lessons of separate API/worker roles, provider-based
  object storage, durable asynchronous webhooks, workspace identity, and queue
  visibility. Do not copy its TypeScript/NestJS domain, GraphQL duplication,
  dynamic object authority, or mandatory Redis/BullMQ topology.
- **Odoo:** copy manifest/dependency discipline, layered authorization,
  tenant-routing caution, reverse-proxy controls, and isolated scheduled work.
  Do not copy the ORM as security authority, generic remote-call surface, or
  arbitrary addon code inside the core server.
- **Frappe/ERPNext:** copy explicit tenant lifecycle, queue classes, worker
  separation, bounded operator commands, and restore rehearsals. Do not copy
  Bench/site topology or add Redis merely because Frappe uses it.

Capability providers, each behind a SquiFlow port:

- Hical inbound HTTP/WSS/SSE; Caddy TLS edge;
- PostgreSQL 18 + libpqxx authoritative storage;
- libsodium token material; spdlog-backed existing logging;
- libcurl outbound HTTP/SMTP/update fetch;
- libavif preview conversion; ClamAV quarantine scan;
- pgBackRest backup/PITR;
- filesystem blob provider first;
- PGMQ vs owned PostgreSQL queue, Garage, and OpenTelemetry vs prometheus-cpp
  remain measured spikes, not approved dependencies;
- Redis, GraphQL, general BPM/policy engines, dynamic ORM, external identity,
  and full AWS SDK remain deferred/rejected until a measured need exists.

Every adoption requires exact pin/hash, license/notices, CVE/maintenance review,
Linux/MSVC build evidence, bounded-resource/failure tests, adapter containment,
and a rollback provider. Never fork a product framework to make SquiFlow look
like it.

## Definition of complete communication

This boundary is complete only when:

- the same protocol vectors pass direct server core, Hical loopback, and
  workstation client tests;
- current and approved previous wire-minor compatibility passes;
- duplicate/lost/reordered request tests prove exactly-once effects through
  idempotent replay;
- every cursor advances atomically with applied data and retention gaps trigger
  bootstrap;
- token rotation/revocation, rights, activation, and tenant isolation fail
  closed;
- realtime loss has no correctness effect;
- large files use bounded staged transfer and quarantine;
- clean-machine Windows workstation communicates with the Podman deployment;
- backup/restore rehearsal preserves tokens, sequences, idempotency results,
  audit, blobs, and workstation resumption;
- the capability map has no unowned row and all evidence is retained.
