# ADR 0013: Server frameworks and providers are adapters

Status: Accepted

Hical is the selected inbound HTTP framework for Phase 8, but it is not the
server architecture. Only the Hical transport adapter and composition root may
name Hical, Boost.Asio awaitables, or Boost.JSON. Endpoint/application code
accepts SquiFlow-owned bounded request values and returns SquiFlow-owned
responses. Domain, protocol, workflows, persistence, media, mail, and update
services never include Hical headers.

Capabilities Hical does not provide are supplied by narrowly scoped provider
adapters behind SquiFlow-owned ports. PostgreSQL initially uses libpqxx,
outbound HTTP and SMTP use libcurl, AVIF conversion uses libavif, opaque token
cryptography continues to use libsodium, and existing logging continues through
SquiFlow's spdlog-backed dispatcher. Hical's MySQL middleware, JWT identity,
configuration ownership, and logging ownership are not adopted merely because
they exist.

A port is introduced only at a real volatility or test seam, not around every
third-party call. Driver types stay inside their adapter target. The executable
composition root selects concrete adapters; application libraries depend only
inward. Static include-policy checks and provider conformance tests enforce
that direction.

If Hical later gains PostgreSQL, streaming upload, mail, or another missing
capability, adoption means implementing the existing SquiFlow port and running
the same conformance suite. It does not change domain types, endpoint contracts,
protocol payloads, migrations, or callers. Replacing Hical itself likewise
requires a new inbound transport adapter and route mapping, not a rewrite of
application services.

The uploaded Hical 2.6.7 source snapshot is pinned by archive SHA-256 until an
exact upstream commit is recorded. Production release still requires a real
compiler/dependency qualification lane and an exact immutable source pin.
