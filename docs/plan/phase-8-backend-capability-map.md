# Phase 8 -- backend capability and open-source component map

Status: planned. This map is the completeness gate for Phase 8. It was created
after comparing SquiFlow with Twenty CRM, Odoo, and Frappe/ERPNext. Detailed
research and sources are in
`../research/open-source-business-backend-review.md`.

A capability may be **owned**, **adopt**, **spike**, **defer**, or **reject**:

- **owned**: product-specific semantics remain SquiFlow code;
- **adopt**: use a pinned open-source provider behind a port;
- **spike**: compare candidates with measured acceptance tests before choosing;
- **defer**: preserve a seam but add no runtime/dependency yet; and
- **reject**: the capability or candidate duplicates an authority or adds more
  operational risk than value.

## Complete backend capability register

### Transport and API

- Inbound HTTP lifecycle, routing, limits, TLS-edge trust, graceful shutdown:
  Hical adapter plus Caddy edge; Phase 8.1.
- Stable route catalog, request/response/problem types, API versioning, OpenAPI
  source, pagination and bounded query semantics: SquiFlow-owned; Phase 8.1.
- Authentication extraction, token issue/rotation/revocation, device identity:
  SquiFlow + libsodium; Phase 8.3.
- Authorization at operation, record/tenant, and sensitive-field projection
  levels: SquiFlow-owned; Phases 8.3, 8.8, 8.9.
- Rate limits, request-size limits, timeout policy, trusted proxy headers and
  abuse controls: Hical/Caddy adapters with SquiFlow policy; Phases 8.1, 8.9.
- Realtime SSE/WebSocket delivery, resume cursor, heartbeat, fan-out and
  backpressure: Hical adapter plus durable notification log; Phase 8.11.

### Data, tenancy, and consistency

- PostgreSQL connection, transaction, prepared-query and pool adapter:
  PostgreSQL + libpqxx; Phase 8.2.
- Forward-only migrations, schema compatibility, startup checks and rollback
  evidence: SquiFlow-owned migration runner over libpqxx; Phase 8.2.
- Tenant provisioning, isolation, row-level security, tenant-scoped sequences,
  quotas, suspend/export/delete lifecycle, and cross-tenant negative tests:
  SquiFlow-owned; Phase 8.9.
- Module manifest identity/version/dependencies/migrations/rights/routes,
  activation state and compatibility: SquiFlow-owned; Phase 8.9.
- Idempotency, authoritative sequence allocation, outbox/inbox, cursor and
  conflict retention: SquiFlow-owned; Phase 8.4.
- Append-only audit, correlation/causation ids, actor/device/tenant attribution,
  export and retention: SquiFlow-owned; Phases 8.4, 8.9.
- Cache invalidation and multi-process coordination: PostgreSQL/in-process
  first; Redis deferred until measured; Phases 8.4, 8.10.

### Background work and integrations

- Durable jobs, queue classes, leases, visibility timeouts, dedupe, retries,
  dead letters, cancellation, progress and worker health: PGMQ versus a narrow
  SquiFlow PostgreSQL table spike; Phase 8.10.
- Scheduler, missed-run handling, singleton/tenant leases and clock/timezone
  semantics: SquiFlow-owned over the durable job store; Phase 8.10.
- Dedicated worker process and bounded lanes so HTTP threads never run heavy
  conversion/mail/import work: SquiFlow-owned composition; Phase 8.10.
- Outbound HTTP, SMTP and update fetch: libcurl adapters; Phases 8.6, 8.7, 8.11.
- Signed webhooks, subscription filters, SSRF-safe destinations, secret
  rotation, retry/replay, delivery audit and loop prevention: SquiFlow-owned
  delivery service over libcurl; Phase 8.11.
- Connector registry and credentials: SquiFlow-owned definitions/secrets;
  external OIDC provider deferred; Phase 8.11.
- Existing business workflows and approval state machines: SquiFlow-owned;
  general BPM engines rejected until a real long-running saga requires one.

### Files, media, and bulk data

- Stable blob ids, hashes, metadata, atomic publish, range/read, delete markers,
  orphan reconciliation and retention: `BlobStore` port; Phase 8.12.
- Local filesystem provider initially; Garage/S3-compatible provider spike only
  when separate server/worker instances need shared/off-host storage.
- Streaming upload, checksum while receiving, quota, partial cleanup and
  backpressure: `UploadReceiver` spike because Hical multipart is buffered;
  Phases 8.5 and 8.12.
- Quarantine and malware scanning for uploads/imports: ClamAV adapter;
  Phase 8.12.
- AVIF preview conversion with dimension/CPU/memory limits: libavif adapter;
  Phases 8.5 and 8.12.
- Bulk import/export with preview, validation, resumability, progress, failure
  report, tenant scope, and audit: SquiFlow jobs over storage ports; Phase 8.12.

### Operations, security, and recovery

- Structured logs and redaction: existing SquiFlow/spdlog path; Phase 8.1.
- Metrics and traces: SquiFlow `MetricsSink`; OpenTelemetry C++ versus
  prometheus-cpp spike when a collector is deployed; Phase 8.13.
- Liveness, readiness, dependency health, worker/queue age, backup age and
  operator-visible degraded state: SquiFlow-owned health model; Phase 8.13.
- PostgreSQL full/incremental backup, WAL archive, point-in-time recovery and
  retention: pgBackRest provider; Phase 8.13.
- Blob/config/secret backup inventory and consistency marker: SquiFlow-owned;
  Phase 8.13.
- Automated restore rehearsal and evidence: operator control plane invoking
  pinned tools through an allowlisted command adapter; Phase 8.13.
- Administrative operations for tenant provision/suspend/export/delete,
  migration, worker drain, backup/restore, key rotation and diagnostics:
  narrow authenticated/audited commands; Phase 8.13.
- Dependency inventory, licenses, SBOM, CVE response and update cadence:
  Phase 9 packaging, with selection started in every Phase 8 provider spike.

### Product extensibility without a dynamic-platform trap

- Typed built-in modules stay compiled and governed by the protocol manifest.
- Safe custom fields may be added through validated metadata and typed value
  kinds after the Phase 8.9 schema spike; they never override money, rights,
  state-machine or audit fields.
- Custom objects, arbitrary server code, unsigned native plugins, and dynamic
  SQL are deferred. A future extension package must be signed, versioned,
  permission-declared, resource-bounded, uninstallable, and tenant-scoped.
- REST/operation APIs remain canonical. GraphQL is rejected until a real client
  demonstrates that it offsets the second authorization/versioning surface.

## Phase ownership after the completeness review

- 8.1: transport-neutral server core, Hical adapter, configuration and health.
- 8.2: PostgreSQL/libpqxx, migrations and transaction boundaries.
- 8.3: identity, device tokens and authentication.
- 8.4: sync, idempotency, sequence and conflict handling.
- 8.5: media conversion and initial bounded upload.
- 8.6: update manifest and artifact proxy.
- 8.7: confirmed mail delivery.
- 8.8: authoritative per-module online operations.
- 8.9: tenancy lifecycle, RLS, module manifests and safe extension seams.
- 8.10: durable jobs, scheduler and worker process.
- 8.11: webhooks, realtime notifications and connector registry.
- 8.12: blob lifecycle, quarantine, malware scan and bulk import/export.
- 8.13: observability, backup/restore and operator control plane.

## Provider approval gate

Before any new open-source component moves from `spike` to `adopt`, record:

1. exact version/commit and source hash;
2. license and notice obligations;
3. maintainer/release/CVE history and supported platforms;
4. transitive dependency and binary-size impact;
5. Linux and MSVC CMake/vcpkg build evidence;
6. bounded-resource, timeout, cancellation and graceful-shutdown behavior;
7. conformance, malformed-input and failure-injection results;
8. data-format/restore compatibility and rollback provider; and
9. the adapter directory and static include-boundary rule.

No candidate is vendored merely because it is open source or because another
ERP/CRM uses it.

## Phase 8 completeness acceptance

Phase 8 cannot close while any capability above lacks an owning phase, explicit
deferral/rejection reason, tests, and operational documentation. At release,
one command must report provider versions, health, queue age, migration status,
backup age, last restore rehearsal, storage usage and unresolved degraded
states without exposing secrets or cross-tenant data.
