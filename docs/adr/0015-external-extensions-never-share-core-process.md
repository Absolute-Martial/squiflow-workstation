# 0015: External extensions never share the core process

**Status:** Accepted

**Context:** Odoo and Frappe demonstrate productive module inheritance and
hooks, but those mechanisms execute with application authority and can replace
core behavior. Package signing does not sandbox code, and process separation
without restricted operating-system capabilities is incomplete. SquiFlow also
needs commercial entitlements to remain authoritative even when integrations
or future extensions are installed.

**Decision:** distinguish four trust classes:

1. built-in modules are reviewed SquiFlow source, statically composed and
   trusted;
2. remote applications use authenticated operation APIs and signed webhooks;
3. any future local extension runs in a dedicated restricted process or a
   qualified WebAssembly host; and
4. data-only customization uses validated metadata and cannot execute code.

External code is never loaded into the server or workstation process as a DLL,
shared library, Qt plugin, Python/Lua/JavaScript runtime script, or native C++
plugin. It cannot import or replace command handlers, authorization,
entitlement verification, migrations, audit, token storage, provider adapters,
or domain implementations. It never receives database credentials or direct
storage access.

A declarative manifest requests identity, compatibility, permissions, events,
UI mounts, network domains, data classes, and resource budgets. Installation
stores a separate administrator-approved grant. The effective capability is the
intersection of request, grant, tenant, current actor, entitlement, and server
policy. A manifest is never a grant.

Package verification binds publisher, package id, version, complete file list,
and artifact hashes to a trusted signature before activation. It also rejects
revoked keys and rollback. Signature validity proves provenance and integrity,
not safety or authorization.

Commercial entitlements are signed documents verified only by trusted server
code at module activation and authoritative command dispatch. Claims are bound
to tenant, product/edition, feature, serial, issuer/key id, format version, and
validity interval. Extensions receive only reduced operation results; they do
not receive signing keys or an entitlement-writing interface. Dependency
license compliance remains a separate legal/SBOM/notices process.

The initial release implements built-in modules and remote integrations only.
A local extension host and public marketplace are deferred. If a concrete local
use case appears, compare a restricted subprocess protocol with Extism/direct
Wasmtime and qualify resource limits, host functions, package verification,
crash isolation, revocation, upgrade, and uninstall before enabling code.

For a self-hosted installation controlled by its administrator, perfect local
anti-tamper enforcement is impossible. Stronger enforcement requires an
external authority or hardware-rooted trust. SquiFlow documents this boundary
rather than claiming secrecy is security.

**Consequences:** external integrations cannot corrupt host memory or reach
internal APIs merely by being installed. API/version design, serialization,
resource budgets, credential rotation, and revocation become explicit work.
Some same-process customizations are less convenient, and a local extension has
IPC/Wasm overhead. The initial product avoids marketplace and sandbox
complexity until demand justifies it.

**Enforcement:** `tools/sandbox/check_extension_boundaries.py` rejects dynamic
library loading and embedded scripting runtimes under `server/`. Phase 8.9 owns
manifest/trust/entitlement contracts; Phase 8.11 owns remote app identities,
permissions, APIs, and webhooks; Phase 9 owns artifact signing and supply-chain
verification.
