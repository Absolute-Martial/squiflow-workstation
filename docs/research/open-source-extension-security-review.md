# Open-source extension security review

Odoo, Frappe/ERPNext, SuiteCRM, browser extensions, editor extension hosts, and
service-side connector systems were reviewed for SquiFlow's future extension
boundary.

## Rejected failure pattern

An addon imported into the trusted process can call private implementation,
monkey-patch checks, read ambient credentials, bypass an entitlement helper,
modify shared schema, block the event loop, crash the host, and retain access
until process restart. A manifest or license check does not make that code
untrusted if it still executes with host authority.

## Adopted pattern

SquiFlow adopts capability-based, out-of-process extensions:

1. signed/quarantined package intake and explicit administrator approval;
2. a separate least-privilege process/container per trust and workload class;
3. versioned request/response and event contracts;
4. host-side tenant, identity, module, right, entitlement, and quota checks;
5. idempotency, bounded payloads, deadlines, cancellation, backpressure, and
   circuit breakers;
6. no direct PostgreSQL/SQLite schema access and no shared secrets;
7. complete audit attribution and kill/revoke controls;
8. compatibility tests and staged upgrade/rollback.

Browser/editor extension hosts are useful inspiration for process isolation and
capabilities, but SquiFlow must additionally preserve financial integrity,
tenant isolation, offline rules, and durable idempotency.

## Timing

Enforce the no-in-process rule now. Build the protocol and host only when a real
external extension use case exists. Do not create a speculative plugin SDK
before the core Phase 8 capabilities and ownership boundaries are stable.
