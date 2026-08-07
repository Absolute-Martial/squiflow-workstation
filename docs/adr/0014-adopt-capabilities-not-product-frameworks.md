# ADR 0014: Adopt capabilities, not product frameworks

Status: Accepted

SquiFlow studies mature open-source business systems such as Twenty, Odoo, and
Frappe/ERPNext to discover missing backend capabilities and proven operational
patterns. It does not embed their domain models, ORM, module runtime, queue
stack, or deployment topology wholesale.

A third-party project is adopted only for one named capability behind a
SquiFlow-owned port. The domain, protocol, authorization, audit, workflow,
tenant, and wire contracts remain SquiFlow-owned. Every provider needs an exact
pin, license/security/build review, bounded-resource behavior, conformance and
failure tests, and a rollback implementation. Provider types and errors stay in
the adapter target.

Open source is preferred over writing commodity infrastructure, but fewer
stateful services are preferred over copying a large SaaS topology. PostgreSQL
is the initial coordination and durable-job substrate; Redis, an S3 service, an
external identity provider, or a general workflow engine is added only after a
measured requirement. The initial deployment must remain operable by a small
shop.

The reference-system review found five previously under-owned areas: tenant and
module lifecycle; durable jobs/scheduling; webhooks/realtime integrations; blob
and import/export lifecycle; and observability/backup/control-plane operations.
They are explicit Phase 8.9-8.13 work, not assumptions hidden inside earlier
server phases.
