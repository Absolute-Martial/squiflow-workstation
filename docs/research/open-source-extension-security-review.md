# Open-source extension security and entitlement review

Inspected: 2026-08-06

## Question and conclusion

This review asks what SquiFlow should learn from extensible open-source systems
without repeating the failure mode where an add-on executes as privileged core
code, overrides authorization or entitlement checks, reaches the database, or
turns a package signature into a false claim of sandboxing.

The central conclusion is simple:

> Same-process extension code is trusted application code, not a security
> boundary.

Odoo's official documentation confirms that modules are Python packages and
that model inheritance can add or override fields, constraints, and methods of
existing models. Its security tutorial also warns that access checks are tied
to ORM data access, that non-CRUD methods do not necessarily check rights, and
that any deliberate bypass requires explicit checks. This verifies the
architectural concern: a module capable of replacing core behavior cannot be
classified as untrusted. It does **not**, by itself, prove a particular reported
license-bypass incident. That claim needs a reproducible advisory or patch
before SquiFlow records it as a fact.

SquiFlow will therefore separate built-in modules, external integrations, local
extensions, package provenance, and commercial entitlements. None of those is
allowed to stand in for another.

## Reference systems

### Odoo: powerful inheritance, no untrusted-code boundary

Useful techniques:

- manifests declare module identity, version, dependencies, data, and license;
- model and view inheritance allow substantial reuse;
- access rights, record rules, and explicit checks form layered authorization;
- modules can be installed and upgraded in dependency order.

Boundary problem:

- modules execute as Python packages in the application environment;
- modules can override methods and extend existing models;
- privileged APIs, raw SQL, deliberate security bypasses, or missing explicit
  checks can escape the normal declarative policy path;
- therefore a module publisher must be trusted like a core-code contributor.

SquiFlow borrows manifests and dependency validation, not same-process override
power. A built-in module can extend the product only through reviewed source and
a rebuild. An external extension cannot import or replace core implementations.

Sources:

- [Odoo module manifests](https://www.odoo.com/documentation/19.0/developer/reference/backend/module.html)
- [Odoo model inheritance](https://www.odoo.com/documentation/19.0/developer/tutorials/backend.html)
- [Odoo data-access restrictions and explicit checks](https://www.odoo.com/documentation/19.0/developer/tutorials/restrict_data_access.html)
- [Odoo backend security reference](https://www.odoo.com/documentation/19.0/developer/reference/backend/security.html)

### Frappe/ERPNext: maintainable hooks, still trusted code

Frappe hooks can extend DocType classes and behavior. The extension-over-override
guidance is good for upgrade compatibility, but the hook still runs inside the
framework's Python environment. It is an application modularity mechanism, not
an isolation mechanism.

SquiFlow borrows named extension points and additive composition. It does not
allow an external package to replace command handlers, rights checks, token
verification, migrations, audit, or entitlements.

Source:

- [Frappe hooks and DocType extension](https://docs.frappe.io/framework/user/en/python-api/hooks)

### WordPress and Nextcloud: hooks and signatures solve different problems

WordPress hooks make composition easy by allowing callbacks to interact with or
modify core behavior. They also illustrate why hook-based plugins are trusted
runtime code.

Nextcloud's scoped X.509 app signatures provide a useful package-integrity
lesson: a certificate is restricted to an application identifier, and the
signature lets the host detect modified files. Signing proves publisher and
artifact integrity; it does not prove that the code is safe or confine what it
can do once loaded.

SquiFlow borrows publisher/package binding, complete-file manifests, and
integrity verification. It does not treat a valid signature as authorization.

Sources:

- [WordPress hooks](https://developer.wordpress.org/plugins/hooks/)
- [Nextcloud application code signing](https://docs.nextcloud.com/server/stable/developer_manual/app_publishing_maintenance/code_signing.html)

### Saleor: preferred remote-app model

Saleor apps are technology-agnostic clients that communicate through APIs and
webhooks. Their manifest declares identity, compatibility, requested
permissions, UI mounts, and webhook subscriptions. Installed permissions are
stored and can be changed independently of what the manifest requested.

This is the strongest fit for SquiFlow integrations:

- run outside the server process;
- issue a distinct extension identity;
- grant only tenant-scoped operations approved at installation;
- deliver versioned, signed, replayable events;
- make revocation and uninstall server-owned operations;
- never expose database credentials or internal C++ interfaces.

SquiFlow keeps its operation/REST protocol rather than copying Saleor's GraphQL
surface.

Sources:

- [Saleor apps overview](https://docs.saleor.io/developer/extending/apps/overview)
- [Saleor app manifests](https://docs.saleor.io/developer/extending/apps/architecture/manifest)
- [Saleor app permissions](https://docs.saleor.io/developer/extending/apps/architecture/app-permissions)

### Grafana and HashiCorp: subprocess lifecycle and RPC

Grafana starts backend plugins as subprocesses and communicates over gRPC using
the HashiCorp plugin pattern. Grafana also verifies plugin signatures. The
HashiCorp design adds expected checksums and optional TLS for RPC.

Useful techniques:

- a plugin crash or memory fault does not directly corrupt host memory;
- process start, handshake, health, timeout, drain, restart, and termination are
  explicit lifecycle states;
- the RPC schema is a compatibility boundary;
- package hash/signature verification happens before execution.

Limits:

- process separation alone does not restrict filesystem, network, process, or
  database access;
- the child still needs a dedicated OS identity/sandbox and an empty-by-default
  environment;
- local RPC credentials and executable paths must be protected by the host.

SquiFlow may borrow the pattern, not the Go library. A local native extension
would run under a dedicated extension host process and receive only a local,
short-lived, capability-bound channel.

Sources:

- [Grafana backend plugin system](https://grafana.com/developers/plugin-tools/key-concepts/backend-plugins/)
- [Grafana plugin verification](https://grafana.com/docs/grafana/latest/administration/plugin-management/)
- [HashiCorp RPC plugin architecture](https://github.com/hashicorp/go-plugin)

### VS Code: trust UX does not remove broad extension authority

VS Code has extension hosts and explicit workspace, publisher, server, and
network trust decisions. Its own security guidance also warns that extensions
can have broad machine access. The lesson is to make trust visible and
revocable, but not to confuse a separate process or publisher prompt with a
least-privilege sandbox.

SquiFlow borrows trust/revocation UX, changed-manifest re-approval, and clear
permission review. It does not offer a single "trust this publisher forever"
grant that silently covers new rights.

Sources:

- [VS Code extension host](https://code.visualstudio.com/api/advanced-topics/extension-host)
- [VS Code trust boundaries](https://code.visualstudio.com/docs/agents/security)

### Kubernetes operators: declarative, out-of-process controllers

Kubernetes operators extend behavior without modifying Kubernetes core. They
are API clients reconciling declared resources. This provides a useful model for
long-running external automations: desired state, observed state, idempotent
reconciliation, dedicated identity, narrow rights, and explicit status.

SquiFlow can use this technique for future connectors that reconcile external
systems. It should not copy Kubernetes or require a cluster.

Source:

- [Kubernetes operator pattern](https://kubernetes.io/docs/concepts/extend-kubernetes/operator/)

### Wasmtime, Extism, and Envoy: capability-based local computation

WebAssembly exposes host interaction only through imported functions. Wasmtime
documents linear-memory isolation, typed control flow, and capability-oriented
WASI filesystem access. Extism provides a C++ host SDK and explicit host
functions. Envoy demonstrates a narrow versioned Wasm ABI for portable filters.

This is a candidate for deterministic local transformations, validation, or
format conversion when a remote app is unsuitable. It is not an automatic
solution:

- host functions define the real security boundary and must be tiny;
- memory, fuel/instruction count, wall time, output size, host-call count, and
  concurrency require hard limits;
- WASI, filesystem, environment, clock, randomness, and network access remain
  disabled unless individually granted;
- runtime vulnerabilities still require patching and defense in depth;
- business writes still go through normal SquiFlow commands and authorization.

Extism versus direct Wasmtime embedding remains a measured spike. No Wasm
runtime is added until a concrete extension needs local computation.

Sources:

- [Wasmtime security model](https://docs.wasmtime.dev/security.html)
- [Extism C++ host SDK](https://github.com/extism/cpp-sdk)
- [Extism host functions](https://extism.org/docs/concepts/host-functions/)
- [Envoy Wasm extension interface](https://www.envoyproxy.io/docs/envoy/latest/intro/arch_overview/advanced/wasm)

### Sigstore and TUF: supply-chain patterns, not runtime authorization

Sigstore/cosign provides signing and verification for binaries and other
artifacts, with key, keyless, KMS, and transparency options. TUF addresses
rollback, freeze, mix-and-match, endless-data, and unrelated-dependency attacks
against update repositories.

SquiFlow should first use its existing signing/update machinery and libsodium
for a small offline package format. It should spike cosign and TUF if a public
extension repository becomes real. Running public transparency services is not
required for the first shop deployment.

Sources:

- [Sigstore](https://www.sigstore.dev/)
- [cosign](https://github.com/sigstore/cosign)
- [TUF security model](https://theupdateframework.io/docs/security/)

## SquiFlow trust classes

### 1. Built-in module

Reviewed SquiFlow source, compiled with the product, governed by protocol and
module-boundary tests. This is trusted code. It cannot be installed from an
external package at runtime.

### 2. Remote application

Preferred third-party extension. It uses a tenant-scoped identity, operation
API, and signed webhooks. It has no host process, filesystem, or database
access. The server grants a subset of requested permissions and can revoke it.

### 3. Local isolated extension

Exceptional path for low-latency/offline computation. It runs in a dedicated
process or qualified WebAssembly host with deny-by-default capabilities and
hard resource budgets. It never loads into `squiflow_server` or the workstation
process.

### 4. Data-only customization

Validated custom fields, forms, views, templates, and rules over safe typed
metadata. It cannot inject SQL or code, alter core fields, or replace rights,
workflow, money, audit, identity, or entitlement logic.

There is no fifth class for an "untrusted in-process plugin". That combination
is contradictory.

## Entitlement and license boundary

Legal license compliance and product entitlement enforcement are separate:

- dependency/source licenses are handled by inventory, notices, source offers,
  and legal review;
- product entitlements decide whether a tenant may activate a commercial
  feature;
- a package signature proves artifact origin and integrity;
- a permission grant decides which API operations an extension may request.

An extension manifest may **request** permissions and features. It cannot grant
either. SquiFlow's trusted server validates a signed entitlement containing at
least issuer, key id, tenant, product/edition, feature ids, serial, issued/not-
before/expiry instants, and format version. Activation and every protected
command are checked in trusted dispatch code. Extension code never receives the
issuer private key, raw database access, a grant-writing API, or the ability to
replace the verifier.

For an offline shop, the server can validate a signed, time-bounded entitlement
lease and record rollback/replay evidence. For self-hosted software where the
administrator controls the executable and operating system, perfect local
anti-tamper enforcement is impossible. Stronger enforcement requires an
external authoritative service or hardware-rooted keys. The design must state
that limitation honestly instead of depending on obscurity.

## Required extension manifest

A future extension package/registration contains declarative data only:

- immutable publisher and package identifiers;
- package and API semantic versions plus compatibility range;
- requested operation rights, webhook events, UI mounts, and optional host
  capabilities;
- tenant scope and data classes touched;
- executable/Wasm artifact hashes and complete file inventory;
- resource budgets and network-domain allowlist;
- migrations limited to extension-owned storage, if local storage is approved;
- upgrade, rollback, uninstall, data-export, and data-delete behavior;
- license identifier, notices, SBOM/provenance reference, and signature chain.

Install stores the separately approved grant. A changed permission, domain,
artifact hash, publisher, or capability set requires re-approval.

## Required enforcement points

- Verify package identity, hash, signature, compatibility, monotonic version,
  revocation, and complete file inventory before activation.
- Authenticate every API/RPC call as a specific extension installation.
- Intersect requested permissions with administrator-approved grants and the
  current user's/tenant's rights; never trust caller-supplied tenant ids.
- Re-check entitlements and authorization at the authoritative command handler,
  not only at UI, route, plugin, or worker boundaries.
- Prevent direct database, token store, signing key, migration runner, audit
  writer, blob-provider path, and internal adapter access.
- Make network, filesystem, clock, environment, secret, and process access
  individual deny-by-default capabilities.
- Bound payload, output, memory, CPU/fuel, host-call count, concurrency, queue,
  retry, and shutdown time.
- Audit install, grant, activation, invocation, failure, upgrade, revocation,
  uninstall, and data deletion without logging secrets.
- Disable safely: stopping an extension cannot stop login, sync, backup,
  migration, entitlement verification, or core workflows.

## Tests required before any external code runs

- forged publisher/package/signature and modified-file rejection;
- downgrade, rollback, freeze, replay, revoked key, expired entitlement, wrong
  tenant, wrong feature, and future-issued claim rejection;
- manifest requests more rights than granted;
- changed manifest forces re-approval;
- extension attempts direct database/file/network/secret/process access;
- host-function confused-deputy and tenant-substitution attacks;
- crash, infinite loop, memory growth, oversized output, fork/process storm,
  slow shutdown, and repeated restart circuit breaking;
- duplicate/retried business command remains idempotent;
- uninstall/reinstall cannot inherit old credentials or orphan undeclared data;
- extension disabled during login, sync, backup, restore, and migration;
- package and extension-host compromise exercises with documented blast radius.

## Adoption decision

For the initial SquiFlow release:

- built-in modules remain statically composed and rebuild-only;
- remote applications through Phase 8.11 APIs/webhooks are the only planned
  third-party integration path;
- arbitrary native libraries and server-side scripts are rejected;
- no extension marketplace is built;
- Phase 8.9 defines manifests, trust classes, entitlement ownership, and safe
  data-only customization;
- a local subprocess or Wasm extension host is deferred until a concrete use
  case passes a threat model and measured qualification spike.
