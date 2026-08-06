# Hical (vendored server transport source)

- Source: https://github.com/Hical61/Hical (`Hical-main.zip`, uploaded by the
  user on 2026-08-06; the archive does not contain a Git commit SHA)
- Declared version: **2.6.7** from the uploaded `CMakeLists.txt`
- Archive SHA-256:
  `a6123395f3896361100737c002703f9a72c8defa7ed72b202b62c5309d96f452`
- License: MIT; see `LICENSE`
- Upstream language/build floor: C++20, CMake 3.20, Boost 1.82+, OpenSSL,
  zlib; upstream documents GCC 14+, Clang 20+, or MSVC 2022+

## What was kept

The build files, `src/`, upstream tests, public documentation, changelog,
security policy, Conan recipe, and vcpkg manifest. The bundled
`picohttpparser` under `src/third_party` is present.

## What was dropped

Benchmarks, Docker deployment examples, GitHub workflow metadata, release
scripts, sample applications, and agent/editor metadata. None is required to
build or audit `hical_core` inside SquiFlow.

## SquiFlow integration policy

This directory is an auditable source snapshot, not permission for Hical APIs
to spread through the server. Only the future
`server/src/adapters/http/hical/` target may include Hical headers or link
`hical::hical_core`. Domain, protocol, workflows, endpoint/application code,
PostgreSQL code, workers, and tests above the adapter use SquiFlow-owned
contracts.

Initial build settings are deliberately conservative:

- `HICAL_BUILD_TESTS=OFF` and `HICAL_BUILD_EXAMPLES=OFF` in the application
  build; upstream tests run in a separate dependency-qualification job.
- `HICAL_ENABLE_REFLECTION=OFF`; route definitions and wire DTOs stay owned by
  SquiFlow.
- `HICAL_WITH_DATABASE=OFF`; the uploaded implementation provides MySQL, while
  SquiFlow requires PostgreSQL.
- `HICAL_WITH_OPENAPI=OFF` initially; the checked-in API contract is generated
  from SquiFlow's route/schema catalog, not Hical macros.
- `HICAL_ENABLE_MEMORY_TRACKING=OFF` in production after a measured debug-lane
  qualification.
- TLS normally terminates at the deployment edge. If direct Hical TLS is ever
  enabled, it remains an adapter/composition-root choice.

Do not use `HttpServer::ioContext()` as a service locator for unrelated
components. Doing that would couple database, mail, media, and worker
lifecycles to Hical and make a later transport replacement invasive.

## Known capability boundary

The snapshot provides HTTP routing, middleware, TLS, WebSocket, SSE, cookies,
sessions, CORS/security headers, rate limiting, gzip, static/range responses,
health endpoints, OpenAPI support, and buffered multipart parsing.

It does **not** provide the PostgreSQL implementation SquiFlow requires, an
outbound HTTP/SMTP client, AVIF encoding, object storage, or a proven bounded
streaming request-body API for large uploads. Its multipart representation
owns each part in a `std::string`, so Phase 8.5 must not treat it as an
unbounded streaming upload solution.

The replaceable-provider plan and conformance gates are documented in
`docs/plan/phase-8-framework-and-provider-isolation.md` and ADR 0013.
