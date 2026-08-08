# Dependencies and pinned versions

Checked on 2026-08-01; the supplied Hical snapshot was inspected on
2026-08-06. Every version here was looked up or read from supplied source, not
remembered.
Where the latest release is not the right choice, the reason is stated.

A version with no evidence behind it is a guess, and guesses in a pin file
become guesses in a build that fails six months later.

---

## The two findings that change decisions

### 1. Qt 6.12 LTS is the last release that will support Windows 10

Windows 10 is now confirmed as the workstation platform, which unblocks Qt 6 -
Qt 6 requires Windows 10 or newer and that requirement is met.

But Qt has announced that **Qt 6.12 LTS will be the last release supporting
Windows 10**, with a five-year support period reaching approximately 2031.
Qt 6.12 is scheduled for September 2026 - roughly a month from now, and it has
not shipped yet.

So the plan is two-step, and it is deliberate:

| When | Pin | Why |
|---|---|---|
| Now, through Phase 7 development | **Qt 6.11.1** | Current release, supported to 2027-03. Everything written against it moves to 6.12 unchanged - Qt minor releases are binary compatible |
| On release of Qt 6.12 LTS | **Qt 6.12 LTS** | The last release supporting Windows 10, supported to about 2031. This is the version the shop actually runs |

Qt 6.8 LTS is the current LTS, but its patch releases beyond the standard
window are **commercial-only**, and this project is on LGPLv3. 6.8 LTS as an
open-source user is just an older release with no patch advantage, so it is
rejected.

### 2. Hical is selected, but remains an adapter

D1 is resolved in favor of Hical. The user supplied a source archive declaring
version **2.6.7**. Because the archive contains no Git metadata, its current
immutable identity is the archive SHA-256:
`a6123395f3896361100737c002703f9a72c8defa7ed72b202b62c5309d96f452`.
Before release, replace that archive-only identity with an exact upstream commit
or release artifact that reproduces the reviewed source.

Hical supplies inbound HTTP, routing, middleware, TLS, WebSocket/SSE, buffered
multipart, and related edge features. It does not determine the rest of the
server stack. PostgreSQL, outbound HTTP/SMTP, AVIF, and blob storage remain
replaceable provider adapters. Hical and Boost transport types are confined to
`server/src/adapters/http/hical/`; see ADR 0013 and
`phase-8-framework-and-provider-isolation.md`.

The supplied source documents Boost 1.82+, OpenSSL, zlib, C++20, and a compiler
floor of GCC 14+, Clang 20+, or MSVC 2022+. Those dependencies and Hical's own
tests must be qualified in the Phase 8 machine lane before the adapter is
counted complete.

---

## The pins

| Dependency | Pinned | Latest available | Note |
|---|---|---|---|
| **Qt** | **6.11.1** | 6.11.1 | Moving to 6.12 LTS when it ships. Dynamically linked, LGPLv3 |
| **SQLite** | **3.53.4** | 3.53.4 (2026-07-24) | See the WAL warning below. Statically linked |
| **PostgreSQL** | **18.4** | 18.4 stable; 19 is in beta | Beta is not a system of record. 18 is supported to 2030-11 |
| **CMake** | **4.4.1** | 4.4.1 | `cmake_minimum_required` stays at 3.28 so an older machine can still configure |
| **libavif** | **1.4.2** | 1.4.2 | Server-side conversion only. Qt ships no AVIF plugin; the workstation-side plugin is still a spike |
| **Hical** | **2.6.7 uploaded snapshot, SHA-256 `a6123395...f452`** | Supplied source | Temporary archive pin; exact reproducible upstream commit/artifact required before release |
| **MSVC** | Visual Studio 2026 toolset | - | CMake 4.2 added the `Visual Studio 18 2026` generator |

### The SQLite warning that matters to us specifically

SQLite **3.52.0 was withdrawn** - it shipped features that were not backward
compatible with prior releases in certain indexed-expression cases.

More important for this project: **3.51.3 fixed a WAL-reset database
corruption bug.** This application uses write-ahead journaling on every
machine, so that bug is directly in our path.

**Minimum acceptable SQLite is 3.51.3. Pinned at 3.53.4.** The build refuses
anything older, checked rather than assumed.

---

## Rules

- **Everything is pinned to an exact version or an exact commit.** No ranges,
  no branches, no "latest".
- **The workstation and the server pin the same protocol commit.** A protocol
  change is one commit, adopted by both sides deliberately.
- **A pin changes only in its own commit**, so a dependency bump is never
  hidden inside a feature change.
- **Version numbers are checked before they are written down**, and this file
  records the date they were checked.
- Qt arrives from official pinned binaries. Normal native dependencies arrive
  through a pinned vcpkg baseline. A user-supplied source snapshot may be
  vendored temporarily for audit/offline work only when its archive hash,
  license, provenance, kept/dropped files, and release-pin exit criterion are
  recorded, as with Hical.

## Next check due

When Qt 6.12 LTS ships, expected September 2026. That is also the moment the
Windows 10 support horizon becomes concrete rather than announced.

## Supplied Phase 7.7 UI build sources

| Source | Pin | Purpose | Runtime? |
|---|---|---|---|
| QWindowKit | 1.5.1.0 snapshot | Qt Core+Quick native window layer | Yes, opt-in |
| qmsetup | supplied snapshot | QWindowKit CMake host helpers | No |
| syscmdline | `0c9f3de8b11bd2f33b03bea5521bf446af4ead69`, archive SHA-256 `97f6bb4d...f3d5481` | qmsetup host utility | No |
| qtimageformats | 6.11.1, archive SHA-256 `b2bf6c68...bac413` | matching image plugin source/provenance | Qt plugin only |

Exact full hashes are in each source's `SQUIFLOW_SOURCE_PIN` and
`README.squiflow.md`. Presence of source does not prove AVIF decoding; the
matching produced/installed plugin must pass Phase 7.10 fixtures.
