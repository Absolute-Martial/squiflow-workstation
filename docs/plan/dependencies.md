# Dependencies and pinned versions

Checked on 2026-08-01. Every version here was looked up, not remembered.
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

### 2. Oat++ has no current release, and this has to be faced

Oat++'s latest *release* is **1.3.0**. Version **1.4.0 has been "in
development" on master for years**, and upstream itself tells people to use the
`1.3.0-latest` tag in the meantime.

There is no version of "use the latest package" that produces a good answer
here. The three options, honestly:

| Option | Cost |
|---|---|
| Pin the `1.3.0-latest` tag | A stable, known artifact, but it is old and receives no fixes |
| **Pin one specific commit of the 1.4.0 branch** | Current code and current fixes, but the API can move under us, and we own any breakage |
| Reconsider the framework | Real work, but the server is not written yet, which is the only cheap moment to ask |

**Recommendation: pin a specific 1.4.0 commit through a vcpkg overlay port,
never a branch.** A submodule or port tracking a branch is a build that changes
without anyone changing it. This matches the decision already on record that
Oat++ is patched via an overlay port and never forked.

This is worth a deliberate answer before Phase 8, not a shrug.

---

## The pins

| Dependency | Pinned | Latest available | Note |
|---|---|---|---|
| **Qt** | **6.11.1** | 6.11.1 | Moving to 6.12 LTS when it ships. Dynamically linked, LGPLv3 |
| **SQLite** | **3.53.4** | 3.53.4 (2026-07-24) | See the WAL warning below. Statically linked |
| **PostgreSQL** | **18.4** | 18.4 stable; 19 is in beta | Beta is not a system of record. 18 is supported to 2030-11 |
| **CMake** | **4.4.1** | 4.4.1 | `cmake_minimum_required` stays at 3.28 so an older machine can still configure |
| **libavif** | **1.4.2** | 1.4.2 | Server-side conversion only. Qt ships no AVIF plugin; the workstation-side plugin is still a spike |
| **Oat++** | **1.4.0 at a pinned commit** | 1.3.0 released | See finding 2 |
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
- Qt arrives from the official binaries, pinned. Everything else arrives
  through vcpkg with a pinned baseline. Nothing is vendored as copied source.

## Next check due

When Qt 6.12 LTS ships, expected September 2026. That is also the moment the
Windows 10 support horizon becomes concrete rather than announced.
