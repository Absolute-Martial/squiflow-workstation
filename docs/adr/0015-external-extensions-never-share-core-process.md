# ADR 0015: External extensions never share the core process

- Status: Accepted
- Date: 2026-08-06

## Context

Odoo-style addons can import core implementation, share a database schema, and
run with the host process's credentials. A licensing check is then only an
ordinary function call an addon can bypass. The same design in a C++ desktop or
server process would also expose memory, secrets, storage handles, thread state,
and every loaded library to untrusted extension code.

## Decision

Third-party or separately licensed extensions never load into workstation,
server API, scheduler, or worker core processes. They run in an independently
sandboxed process/container and communicate only through a versioned protocol.
The host authenticates the extension identity, authorizes every requested
capability, applies tenant scope, validates bounded messages, enforces time and
resource budgets, and records audit evidence.

First-party modules remain compile-time components governed by the declared
module graph. A module may not provide a hidden dynamic-loader escape hatch.
Extensions do not receive raw database credentials, encryption keys, storage
handles, user tokens, filesystem paths, or an in-process object pointer.

License/entitlement verification is authoritative at the host boundary and at
every protected operation. It is never delegated to extension UI code.

## Consequences

- Extension crashes, dependency conflicts, and memory corruption are isolated.
- Capability revocation and protocol compatibility can be tested centrally.
- Calls have serialization and process-boundary overhead.
- Rich extensions require explicit APIs instead of private implementation
  imports; missing capabilities must be added deliberately.
- An extension host, package signature/quarantine flow, quotas, observability,
  and compatibility policy are required before third-party execution ships.
