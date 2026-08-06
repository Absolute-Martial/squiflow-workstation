# Phase 8 -- Hical and replaceable provider implementation plan

Status: Hical 2.6.7 source supplied by the user, inspected, and vendored as an
auditable snapshot. It is not yet linked into the product. The current sandbox
cannot qualify Hical's documented compiler/dependency floor, PostgreSQL, or a
real network runtime, so those gates remain machine-only.

This plan answers two separate questions:

1. where Hical is useful now; and
2. how SquiFlow can use other C/C++ projects for missing capabilities without
   making a later Hical implementation or provider replacement a breaking
   rewrite.

It complements `phase-8-server-plan.md`; it does not add another server phase.
The broader capability register and open-source shortlist are in
`phase-8-backend-capability-map.md`.

## Evidence from the supplied source

The uploaded `CMakeLists.txt` identifies Hical 2.6.7. The archive is pinned in
`external/server/hical/SQUIFLOW_SOURCE_PIN` by SHA-256 because a Git commit SHA
is not present.

The inspected implementation provides:

- HTTP/1.1 server, routing, route groups, middleware, bounded header/body
  limits, graceful shutdown, TLS, cookies, sessions, gzip, CORS, security
  headers, rate limiting, health endpoints, static/range responses, and error
  handling;
- Boost.Asio coroutine handlers plus a synchronous route fast path;
- WebSocket, SSE, heartbeat, compression, and origin/subprotocol controls;
- Boost.JSON helpers, OpenAPI generation, configuration, and logging; and
- buffered multipart parsing and optional MySQL middleware.

Important limits for SquiFlow:

- database support is MySQL-oriented, not PostgreSQL;
- route, middleware, JSON, WebSocket, and coroutine APIs expose Hical/Boost
  types directly;
- no outbound HTTP or SMTP client is provided;
- no AVIF encoder or object-store client is provided;
- multipart parsing owns the complete body and each part in memory, so it is
  not evidence of bounded streaming upload support; and
- upstream documents GCC 14+, Clang 20+, or MSVC 2022+, while SquiFlow's
  portable sandbox lane currently uses an older GCC and has no CMake package
  graph for Boost/OpenSSL/zlib.

These limits are reasons for adapters and explicit gates, not reasons to fork
or rewrite Hical.

## Non-negotiable dependency direction

```text
external/protocol + engine/domain/workflows
                    ^
                    |
           server application services
                    ^
                    |
      SquiFlow-owned ports and endpoint contracts
        ^           ^             ^            ^
        |           |             |            |
 Hical HTTP     libpqxx PG    libcurl I/O   libavif codec
 adapter         adapter        adapters       adapter
        \           |             |            /
                 composition root
```

Allowed dependencies point upward/inward. Provider adapters may depend on a
third-party project and a SquiFlow port. A port may not depend on its adapter.
The composition root is the only place concrete providers are assembled.

### Build targets

Implement these as separate targets so the linker graph enforces the design:

- `squiflow_server_core`: endpoint contracts, route catalog, problem responses,
  authentication context, application services; no Hical, Boost, libpqxx,
  libcurl, or libavif;
- `squiflow_server_http_hical`: request/response mapping, route registration,
  middleware wiring, lifecycle, and Hical-only realtime sessions;
- `squiflow_server_pg_libpqxx`: PostgreSQL stores, transactions, pool, and
  migration execution;
- `squiflow_server_io_curl`: outbound HTTP, artifact fetch, and SMTP transport;
- `squiflow_server_avif_libavif`: decode validation and bounded preview encode;
- optional future object-store adapters such as filesystem, MinIO, or S3; and
- `squiflow_server`: composition root and process entry point only.

A static policy test fails if `<hical/...>` appears outside
`server/src/adapters/http/hical/`, if `<pqxx/...>` appears outside the
PostgreSQL adapter, or if curl/libavif types appear outside their adapters.

## The inbound HTTP seam

Do not attempt to mirror every method on `hical::HttpServer`. Own the stable
shape needed by SquiFlow:

- `ApiRequest`: method, canonical route id, owned path/query parameters,
  selected bounded headers, trace id, remote address, optional authenticated
  principal, content type, and bounded body bytes;
- `ApiResponse`: status, ordered headers, content type, and owned body bytes;
- `Problem`: stable machine code, safe message, trace id, and field errors,
  serialized consistently as an RFC 9457-style response;
- `Endpoint`: an application handler over those values; and
- `RouteSpec`: route id, method, path template, body limit, authentication
  requirement, required protocol right, offline/online policy, and handler.

`HicalRequestMapper` copies only the values an endpoint is allowed to retain.
This is necessary because Hical request headers and targets may be string views
into a connection buffer. No endpoint retains `hical::HttpRequest&`.
`HicalResponseMapper` is the only code that constructs
`hical::HttpResponse`.

Hical's `Awaitable`, router, middleware callbacks, `HttpServer::ioContext()`,
Boost.JSON values, WebSocket sessions, and SSE sessions stop at this adapter.
Application services remain ordinary C++ calls with explicit cancellation and
bounded-worker ownership. Do not make Hical's event loop the service locator
for PostgreSQL, mail, media, or update workers.

### Route ownership

Routes are declared once in the SquiFlow route catalog. The Hical adapter
iterates/registers that catalog. Authentication and rights are application
policies attached to `RouteSpec`; Hical middleware extracts transport inputs
but cannot become the only authority. The service re-runs authorization before
mutation.

OpenAPI, if enabled, is generated from the same SquiFlow catalog/schema model.
Do not annotate domain DTOs with Hical macros. This keeps route and wire-schema
ownership stable if Hical is replaced.

## Provider choices for missing capabilities

### PostgreSQL: libpqxx adapter

Use PostgreSQL 18 through a pinned libpqxx provider. Hical's optional database
feature remains disabled because the supplied backend is MySQL. Domain and
endpoint code never sees `pqxx::connection`, `pqxx::transaction`, SQL results,
or Hical database middleware.

Keep SQL inside PostgreSQL stores. The application depends on existing typed
store/service contracts. A small transaction coordinator may group the
specific stores that must commit atomically; do not create a generic SQL API
and leak query strings upward.

If Hical later adds PostgreSQL, evaluate it with the same store/pool/migration
conformance suite. A `hical_pg` adapter may replace `libpqxx` only if it meets
transaction, cancellation, pool-bound, prepared-statement, and error-mapping
requirements. Nothing above the adapter changes.

### Outbound HTTP, update fetch, and SMTP: libcurl adapters

Hical is an inbound server, not an outbound client. Use pinned libcurl behind
small capability ports:

- `ArtifactFetcher` for update-proxy upstream reads;
- `HttpClient` only for genuinely generic provider APIs; and
- `MailTransport` for SMTP or an HTTP mail provider.

The mail application owns idempotency, claim/retry state, and delivery records.
The curl adapter owns protocol details, TLS verification, timeouts, response
limits, and redacted diagnostics. A later Hical outbound client can implement
these same ports without changing mail/update code.

### AVIF: libavif adapter

Use pinned libavif behind `ImageTranscoder`. The port accepts a trusted staged
input reference plus explicit output limits and returns metadata/error values,
not libavif structs. Decode dimensions and resource budgets are checked before
full conversion. Worker concurrency, retries, and queue depth remain owned by
SquiFlow's bounded supervisor.

If Hical later exposes media helpers, that does not move conversion into an
HTTP handler. It can only become another `ImageTranscoder` implementation.

### Blob/object storage

Start with a filesystem `BlobStore` adapter using atomic publish and
content-hash identity. Add MinIO/S3 only when deployment requires it. If added,
use a dedicated provider adapter and keep bucket names, SDK handles, and URLs
out of media/file domain records. Records store stable blob ids and hashes.

Do not adopt the full AWS SDK pre-emptively. Its size and configuration surface
are not justified for a single-machine initial deployment.

### Logging, cryptography, and observability

Keep the already selected spdlog-backed SquiFlow dispatcher and libsodium token
implementation. Hical access logs may feed that dispatcher through one adapter;
Hical logging must not become a second independent retention/rotation policy.
Do not replace opaque tokens with Hical JWT simply because JWT middleware is
available.

Define a minimal `MetricsSink` in 8.13 with no-op and deterministic recorders.
Spike OpenTelemetry C++ against prometheus-cpp only when the deployment has a
collector/export requirement; adopt one telemetry stack, not two competing
ownership paths.

## The large-upload gap

Phase 8.5 must begin with a real spike. The supplied Hical request model owns
the body as `std::string`, and multipart parts own their data. Therefore:

- Hical uploads are capped to a measured small-body limit until proven safe;
- oversized requests are rejected before application handling;
- originals are staged outside the application transaction and referred to by
  a stable upload id;
- an `UploadReceiver` port is introduced only if the product needs bodies too
  large for the proven cap; and
- candidates for a streaming implementation are evaluated behind that port,
  potentially as a dedicated internal upload listener/sidecar routed by the
  edge proxy. The rest of the media workflow must not know which receiver won.

If upstream Hical later adds bounded incremental body callbacks, implement a
new Hical `UploadReceiver` and run the same cancellation, size-limit, checksum,
partial-file cleanup, and backpressure suite. Do not redesign media endpoints.

## Dependency replacement protocol

Every provider adoption or replacement follows the same sequence:

1. write or update the SquiFlow capability contract from product requirements,
   not from the candidate library API;
2. add deterministic fake/recording implementations and contract tests;
3. implement one adapter with all third-party includes private to its target;
4. run provider conformance tests against a real dependency/runtime;
5. wire it only in the composition root behind an explicit CMake provider
   selection;
6. add failure-injection tests for timeout, cancellation, resource exhaustion,
   malformed data, and shutdown; and
7. switch the default in a dedicated dependency commit with rollback notes.

Never expose provider error enums. Map them once to stable SquiFlow fault codes
and retain low-level details only in redacted diagnostics.

## Hical build and pin policy

Initial application configuration:

```text
HICAL_BUILD_TESTS=OFF
HICAL_BUILD_EXAMPLES=OFF
HICAL_ENABLE_REFLECTION=OFF
HICAL_WITH_DATABASE=OFF
HICAL_WITH_OPENAPI=OFF
HICAL_WITH_MIMALLOC=OFF
HICAL_ENABLE_MEMORY_TRACKING=OFF (production)
```

A separate qualification job builds Hical's upstream tests with the pinned
Boost, OpenSSL, zlib, compiler, and operating-system matrix. Use the bundled
picohttpparser first to reduce package ambiguity; changing to the system copy is
its own dependency decision.

Before release, replace the archive-only identity with an exact upstream commit
or release artifact whose hash reproduces the reviewed source. Never track
`main`, a moving vcpkg registry head, or a version range.

## Implementation sequence

### 8.1a -- framework-neutral endpoint core

Add `ApiRequest`, `ApiResponse`, `Problem`, `Endpoint`, `RouteSpec`, route
catalog, body/header limits, and direct unit tests. Implement `/health/live`
and `/health/ready` as application endpoints against a `ReadinessProbe` port.

### 8.1b -- Hical adapter

Add Hical request/response mappers, route registration, error boundary,
trace-id extraction/generation, connection/body/header limits, and graceful
shutdown. No business endpoint includes Hical.

### 8.1c -- provider composition

Add typed configuration and the executable composition root. Validate every
provider selection before listening. Secrets never appear in logs. Build Hical
only when `SQUIFLOW_WITH_SERVER=ON` and the HTTP provider is `hical`.

### 8.1d -- dependency and replacement gates

Add include-boundary scans, a fake inbound adapter, direct endpoint tests, and
loopback Hical contract tests. Prove that replacing the inbound adapter does not
recompile domain/workflow libraries.

### 8.2 onward -- capability adapters

Implement PostgreSQL/libpqxx, curl, libavif, and storage adapters only in the
sub-phase that needs them. Each one receives a focused conformance suite and
its own Conventional Commit. Do not batch all provider dependencies into 8.1.

## Required tests and gates

- `server_core` builds and tests with no Hical/Boost/PostgreSQL/curl/libavif;
- direct endpoint tests and Hical loopback tests produce identical status,
  headers, body, auth failure, and problem codes;
- malformed/oversized headers and bodies are rejected at the transport edge;
- request-buffer lifetimes are tested by retaining mapped values after the
  Hical request object is destroyed;
- concurrent handlers share no mutable framework request/session state;
- shutdown stops admission, cancels/drains bounded work, and closes providers
  in the documented order;
- a dependency policy scan proves third-party include confinement;
- Hical upstream tests pass in the dependency-qualification lane;
- PostgreSQL tests use a real PostgreSQL instance for locks, sequences,
  migrations, and transaction behavior; and
- provider fault mapping and redaction are tested, not inspected manually.

## Acceptance criteria

This isolation work is accepted when Hical serves the health and one protected
sample endpoint through the route catalog, while the same endpoint passes
direct tests with no Hical dependency; static checks prove no Hical type leaks;
the executable can substitute fakes for every not-yet-built provider; and the
provider matrix has real conformance gates for each adopted project.

A future Hical PostgreSQL, upload, client, or codec implementation must be
adoptable by adding/replacing one adapter target and composition-root selection.
Any plan requiring domain, protocol, workflow, endpoint, or migration changes
for that switch fails this acceptance criterion.
