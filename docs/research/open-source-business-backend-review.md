# Open-source business-backend architecture review

Checked: 2026-08-06

Purpose: compare mature open-source CRM/ERP backends with the planned SquiFlow
server, identify capabilities that Phase 8 previously overlooked, and shortlist
reusable open-source components without importing another product's domain
model or creating a dependency monolith.

This is architecture research, not evidence that any candidate is already
approved. Every adopted project still needs an exact version/commit, license and
security review, native build proof, failure tests, and a SquiFlow adapter.

The focused extension, plugin, package-signing, and entitlement threat model is
in `open-source-extension-security-review.md`. It follows Odoo/Frappe with
Saleor, Grafana, HashiCorp, VS Code, Nextcloud, Kubernetes, Wasmtime/Extism,
Sigstore, and TUF patterns.

## Reference systems reviewed

### Twenty CRM

Twenty is a TypeScript/NestJS and React system built as an Nx monorepo. Its
published stack uses PostgreSQL for records, Redis/BullMQ for queues, a separate
worker process, and local or S3-compatible object storage. It exposes REST and
GraphQL APIs and emphasizes metadata-defined objects, fields, layouts, apps,
and workflow automation.

Useful patterns for SquiFlow:

- API/web process and background worker are separate deployment roles.
- Workspace identity is explicit and multi-workspace mode is a deployment
  decision, not an inferred property of a request.
- Object storage is a provider: local storage is acceptable initially, while
  S3-compatible storage supports multiple server/worker instances.
- Webhook requests acknowledge quickly and perform durable work on a worker,
  preventing sender timeouts from causing duplicate delivery storms.
- Custom objects/fields and app metadata are versioned definitions rather than
  ad-hoc database changes.
- Worker health and queue visibility are operator-facing capabilities.

Patterns not copied directly:

- Redis/BullMQ is not automatically required for a single-shop server.
  PostgreSQL-backed jobs may provide enough durability with fewer services.
- GraphQL is not added beside the existing operation/sync protocol unless a
  concrete client requires it.
- Twenty's dynamic object engine is not a replacement for SquiFlow's typed
  finance, pricing, workflow, and audit invariants.

### Odoo

Odoo documents a three-tier architecture with PostgreSQL as its data tier and
an addon/module system driven by manifests and dependencies. A module can own
business objects, views, configuration/security data, controllers, reports, and
static resources. Production deployment separates HTTP, cron, and realtime
worker concerns and relies on a reverse proxy for TLS and request controls.

Useful patterns for SquiFlow:

- Every module has a manifest naming identity, version, dependencies, loaded
  data, and installation state.
- Authorization is layered: model/operation access, record-level predicates,
  and field restrictions. Server methods and parameters are never trusted just
  because a client reached them.
- Raw SQL is treated as a security boundary because it can bypass higher-level
  authorization and consistency rules.
- Scheduled work is isolated from request workers and has explicit resource
  limits.
- Tenant/database routing and database listing are treated as security
  controls, not convenience configuration.
- Reverse proxy trust, forwarded headers, HTTPS, timeouts, and request-size
  limits are explicit production settings.

Patterns not copied directly:

- SquiFlow does not expose a general remote-callable method surface. Only the
  route/operation catalog is callable.
- It does not build a universal ORM whose hooks become the only place security
  is enforced. Application services re-check authorization before mutation.
- It does not load unsigned arbitrary C++ plugins into the server process.

### Frappe / ERPNext

Frappe combines apps, tenant sites, and benches. Each site has a separate
database; a bench owns the runtime and installed apps. Production roles include
proxy, application, database, cache/queue, background workers, realtime, and an
operator agent/control plane. Its job system separates short, default, and long
queues so long work does not exhaust web workers.

Useful patterns for SquiFlow:

- Treat tenant provisioning, backup, upgrade, restore, and deletion as a
  lifecycle, not just a `tenant_id` column.
- Separate request execution from durable background work.
- Give jobs classes/lanes, hard limits, progress, retries, and worker health.
- Make operational actions reproducible through a narrow control-plane command
  surface with audit, rather than undocumented shell procedures.
- Test backups by restoring them; creation alone is not recovery evidence.

Patterns not copied directly:

- Database-per-tenant is not selected automatically. SquiFlow already plans a
  tenant-scoped PostgreSQL schema and can use row-level security; the final
  isolation mode needs a measured operational decision.
- Redis and Node processes are not adopted merely because Frappe bundles them.

## Gaps found in the former 8.1-8.8 plan

The original Phase 8 covered HTTP, PostgreSQL migrations, tokens, sync, media,
updates, mail, and online module operations. The comparison found five
cross-cutting capability groups that were not owned strongly enough:

1. tenant provisioning, row isolation, lifecycle, quotas, export, deletion,
   module manifests, and safe customization seams;
2. a durable job store, leases, scheduler, worker process, retry/dead-letter
   policy, progress, and queue health;
3. signed webhooks, integration credentials, realtime notifications, delivery
   replay, and connector ownership;
4. blob lifecycle, streaming ingress, quarantine/malware scanning, retention,
   import/export, and orphan reconciliation; and
5. metrics/traces, operator health, backup/PITR, restore drills, disaster
   evidence, and a bounded administrative control plane.

These are now assigned to Phase 8.9-8.13 instead of being left as implied
future work.

## Open-source component shortlist

### Strong current candidates

- **Hical**: inbound HTTP/WebSocket/SSE adapter only; already vendored and
  isolated.
- **PostgreSQL + libpqxx**: authoritative server data and typed C++ adapter.
- **libcurl**: outbound HTTP, SMTP, and artifact fetch adapters.
- **libavif**: bounded preview conversion adapter.
- **libsodium**: opaque-token randomness, hashing, and constant-time compare.
- **spdlog**: existing logging dispatcher behind SquiFlow's logging contract.
- **Caddy**: deployment edge for TLS, request limits, routing, and trusted proxy
  headers.
- **pgBackRest**: PostgreSQL backup, WAL archiving, point-in-time recovery, and
  restore tooling.
- **ClamAV**: quarantine scanning for untrusted uploaded/imported files.

### Candidates requiring a spike

- **PGMQ**: PostgreSQL-native durable queue. Compare its extension and SQL-only
  modes with a small SquiFlow-owned `FOR UPDATE SKIP LOCKED` job table. The
  winner must support leases, visibility timeouts, dedupe, retries, dead
  letters, cancellation, and PostgreSQL 18 without making an extension a hard
  restore dependency accidentally.
- **Garage**: lightweight S3-compatible object store aimed at small self-hosted
  deployments. Compare it with the filesystem adapter only when server and
  worker need shared storage or off-host replication.
- **OpenTelemetry C++**: stable logs/metrics/traces API, but its dependency and
  exporter graph must be measured. Start with a tiny SquiFlow `MetricsSink` and
  adopt the SDK only when an external collector is deployed.
- **prometheus-cpp**: direct metrics exporter candidate if OpenTelemetry is too
  heavy; use one metrics stack, never two competing ownership paths.

### Deliberately deferred or rejected for the initial shop deployment

- **Redis**: useful for Twenty/Frappe scale-out queues and cache, but adds a
  second stateful service. Add only when PostgreSQL/in-process coordination
  fails a measured workload or multiple workers require it.
- **Keycloak or ZITADEL**: good future OIDC/SSO providers. Keep identity behind
  the existing auth port and adopt one only when external SSO/MFA/passkeys are
  a product requirement; do not replace device tokens prematurely.
- **Casbin or another policy engine**: not adopted while protocol rights and
  server authorization already form one auditable authority. A second policy
  language risks contradictory decisions.
- **Temporal, n8n, or a general BPM engine**: not adopted for existing typed
  transactional workflows. Reconsider only for long-running cross-system
  sagas that cannot be represented by the durable job/outbox model.
- **GraphQL**: Twenty benefits from metadata-driven clients; SquiFlow already
  has an operation catalog and sync protocol. Adding GraphQL now doubles API
  authorization, versioning, and testing surfaces.
- **A general dynamic ORM**: rejected. Typed stores and migrations keep money,
  rights, audit, and workflow rules explicit.
- **Full AWS SDK**: deferred; a small S3-compatible provider is preferable if
  object storage is needed.

## Selection rule

Open source is the default place to look, not an automatic approval. Adopt a
component only when all of these are true:

- it closes a named capability and is smaller/safer than maintaining that
  capability ourselves;
- its license, maintenance, release history, CVE response, and transitive
  dependencies are acceptable;
- it builds on the pinned Linux/MSVC toolchains and has deterministic packages;
- it supports bounded resources, cancellation, graceful shutdown, and useful
  fault reporting;
- it fits behind a SquiFlow-owned port without leaking types upward;
- it has a real conformance/failure suite and a rollback provider; and
- operating it does not cost more than the problem it solves.

## Sources

- [Twenty repository and published stack](https://github.com/twentyhq/twenty)
- [Twenty self-host configuration and storage](https://docs.twenty.com/developers/self-host/capabilities/setup)
- [Odoo architecture and modules](https://www.odoo.com/documentation/19.0/developer/tutorials/server_framework_101/01_architecture.html)
- [Odoo module manifests](https://www.odoo.com/documentation/19.0/developer/reference/backend/module.html)
- [Odoo backend security](https://www.odoo.com/documentation/19.0/developer/reference/backend/security.html)
- [Odoo production deployment](https://www.odoo.com/documentation/19.0/administration/on_premise/deploy.html)
- [Frappe architecture](https://docs.frappe.io/customer-guide/scalability/architecture-overview)
- [Frappe background jobs](https://docs.frappe.io/framework/user/en/guides/app-development/running-background-jobs)
- [libpqxx](https://github.com/jtv/libpqxx)
- [PGMQ](https://github.com/pgmq/pgmq)
- [Garage](https://github.com/deuxfleurs-org/garage)
- [pgBackRest](https://github.com/pgbackrest/pgbackrest)
- [OpenTelemetry C++](https://github.com/open-telemetry/opentelemetry-cpp)
- [prometheus-cpp](https://github.com/jupp0r/prometheus-cpp)
- [ZITADEL](https://github.com/zitadel/zitadel)
