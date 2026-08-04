# Phase 9 -- Build, packaging and release: 9.1-9.6 implementation plan

Status: planned, not started as a gated sub-phase sequence. Note: an
earlier MSVC/CLion/GitHub Actions pipeline was implemented and checkpointed
in a prior session
(`/data/squiflow-checkpoint-msvc-ci-release-pipeline-with-git.zip`,
commits `5cb0de3`/`4af1e2a`/`69bba6c`) but is **not present in the current
repository** after a sandbox reset. Before starting 9.1, check whether that
zip's contents should be restored/re-applied rather than rebuilt from
scratch -- re-derive only what is actually missing.

## Shared ground rules for all of 9.1-9.6

- CI must never claim a number it did not measure. Binary sizes, test
  counts, and coverage are always read from the actual CI run's output,
  never estimated -- matching the "no invented numbers" rule already
  standing for every phase.
- The pipeline must work identically whether triggered from GitHub Actions
  or run locally inside CLion via the same CMake Presets -- no
  CI-only script that a developer cannot reproduce on their own machine.
- Every release artifact is verifiable independently of the pipeline that
  produced it: a SHA-256 published alongside the artifact, checkable by
  anyone without trusting the CI logs.

## 9.1 -- GitHub Actions across the private repositories, App installation token

**Goal:** CI that can check out and build across the workstation repo and
any separate server/protocol repositories, using a GitHub App installation
token rather than a personal access token, so access is scoped and
revocable per-repository rather than per-person.

**Scope:** `.github/workflows/ci.yml` (Linux build+test lane, reusing the
`tools/sandbox/Makefile` strict gate as one job and the CMake configuration
as another, so CI enforces exactly the same two gates a human is expected
to run locally), a GitHub App registered with least-privilege contents/
read access to the protocol and server repositories, and a token-exchange
step (`actions/create-github-app-token` or equivalent) used only inside the
workflow run, never persisted as a repository secret in plaintext beyond
the App's own private key.

**Invariants:** the installation token is scoped to exactly the
repositories the build needs and expires with the job; no workflow step
prints the token; a workflow triggered from a fork never receives the
installation token (matching GitHub's own pull_request_target caution).

**Tests/checks:** a CI run against a clean checkout succeeds end to end; a
CI run with an intentionally failing test fails the job (not silently
green); a simulated cross-repository checkout (protocol repo as a
submodule or sparse checkout) succeeds with the App token and fails
cleanly, with a clear error, when the App lacks access.

## 9.2 -- The ten-step Windows build

**Goal:** a documented, CI-executed, ten-numbered-step Windows build using
Visual Studio 2022/MSVC, vcpkg for pinned dependencies, and
`windeployqt` for runtime staging -- the same steps a developer runs
locally through CLion's CMake Presets, not a CI-only shortcut.

**Scope:** `CMakePresets.json` Windows configure/build presets (already
planned to exist per the dependencies), `.github/workflows/windows-
build.yml`, and a written ten-step sequence, e.g.: (1) checkout, (2) vcpkg
bootstrap/restore from the pinned manifest, (3) CMake configure with the
Windows preset, (4) build the engine/domain/protocol libraries, (5) build
the workstation Qt target, (6) run the portable test suite, (7) run the
Qt-capable test suite, (8) `windeployqt` the built executable plus
qml/imports, (9) stage the AVIF and other Qt plugins from the supplied Qt
6.11.1 build, (10) zip the staged output with a manifest listing every
file and its hash.

**Invariants:** every step's output is an explicit input to the next --
no step silently depends on leftover state from a previous CI run (clean
workspace per run); `windeployqt` staging includes exactly the plugins the
application actually loads (verified against the plugin list from 7.5/7.6),
never a blanket "deploy everything."

**Tests/checks:** the staged zip's executable actually launches headless
(`-platform offscreen` or equivalent) in CI and reaches the point of
requesting activation, proving the deployed Qt runtime is complete; a
missing-plugin regression (a plugin the app needs removed from staging) is
detected by that same headless launch check failing.

## 9.3 -- Automated self-signed signing

**Goal:** every Windows release artifact is signed automatically by CI,
using a self-signed (not yet publicly trusted) certificate to start, with
the signing step structured so swapping to a purchased/EV certificate
later is a configuration change, not a pipeline rewrite.

**Scope:** a CI step invoking `signtool` (or `osslsigncode` if signing from
a non-Windows runner) against the staged executable and installer, with
the certificate and private key held as a CI secret, never committed to
the repository; a documented, explicit warning surfaced in the release
notes that a self-signed certificate will still trigger an
unknown-publisher warning on end-user machines until a trusted certificate
replaces it.

**Invariants:** the signing key never appears in build logs; an unsigned
artifact is never published as a release -- the release job fails closed
if signing fails, rather than shipping an unsigned build with a warning.

**Tests/checks:** CI verifies its own signature after signing
(`signtool verify` or equivalent) before proceeding to publish; a
deliberately broken/expired test certificate causes the job to fail
visibly rather than produce a falsely-"signed" artifact.

## 9.4 -- The updater: side-by-side, directory junction, auto-revert, outbox-blocked

**Goal:** an updater that stages a new version alongside the running one,
switches a directory junction to point at it, automatically reverts after
two consecutive failed starts of the new version, and refuses to update at
all while the workstation's outbox (Phase 3.5) is not empty.

**Scope:** `tools/updater/updater.cpp` (or a dedicated `updater/`
target), consuming the 8.6 manifest endpoint, staging the new version in a
sibling directory (e.g. `app-<version>/`), and repointing a junction
(`CreateSymbolicLink`/`mklink /J` equivalent via the Win32 API) that the
shortcut/start-menu entry actually targets; a small persisted "start
attempt" counter checked at the *new* version's own startup (via the
already-existing `StartupSequence`, so this reuses 6.8's fixed order rather
than adding a parallel one) that reverts the junction if two consecutive
attempts fail before reaching `Running`.

**Invariants:** the updater checks the outbox's actual pending-entry count
(via the same storage interface Phase 3.5 already exposes) before staging
an update, and refuses -- with a named, visible reason -- if it is
nonzero, so a device is never updated out from under an unsynced
transaction; a revert always restores the junction to the last known-good
version, never deletes it, until a new version has itself proven at least
one successful start.

**Tests/checks:** normal update with outbox empty succeeds; update
attempt with a nonzero outbox is refused with the exact count reported;
two consecutive failed starts of a staged version trigger an automatic
revert; a successful start of a staged version resets the failure
counter; a revert during a revert (double failure of the fallback itself)
is surfaced as a fatal, human-visible state rather than looping.

## 9.5 -- Podman Quadlet units

**Goal:** the server (Phase 8) deployed as rootless Podman containers via
Quadlet unit files, not a hand-run `docker run`/`podman run` command, so
deployment is declarative and systemd-managed.

**Scope:** `deploy/quadlet/squiflow-server.container`,
`deploy/quadlet/squiflow-postgres.container` (or an external managed
PostgreSQL, if that is the shopkeeper's actual hosting choice -- a decision
to confirm before writing this unit, similar in spirit to D1), a
`deploy/quadlet/squiflow.network` unit connecting them, and
environment/secret files referenced by path rather than inlined.

**Invariants:** containers run rootless with no unnecessary capabilities;
secrets are never baked into the container image layer, only mounted at
runtime; the Quadlet units restart the service on failure with a bounded
backoff, never an infinite tight restart loop.

**Tests/checks:** `podman-system-generator`-equivalent validation that the
unit files parse and generate the expected systemd units; a local rootless
Podman run of the generated units reaches the 8.1 health-check endpoint
successfully; a deliberate container crash triggers the expected bounded
restart behavior, verified by inspecting `systemctl status`/journal
output.

## 9.6 -- The release, size recorded by CI rather than estimated

**Goal:** the actual GitHub Release, with every published number (zip
size, executable size, SHA-256 of every artifact) read directly from the
CI run that produced them -- never a number typed into release notes by
hand.

**Scope:** `.github/workflows/release.yml`, triggered on a version tag,
chaining 9.2 (build/stage), 9.3 (sign), and a final step that computes
SHA-256 and byte size for every artifact with a command
(`sha256sum`/`Get-FileHash`, `stat`/`Get-Item .Length`) and writes them
into the release body/attached manifest programmatically, then publishes
the GitHub Release with those exact artifacts attached.

**Invariants:** the release job never hand-edits a size or hash into the
release notes template -- every such value is substituted from the CI
run's own measurement step; a release is never published if any upstream
job (build, test, sign) failed, even if a later job could technically
still run -- release publishing is the last, gated step, not an
independent one.

**Tests/checks:** a dry-run release job against a test tag produces a
manifest whose hashes verify against the actual attached files
(`sha256sum -c`); a deliberately failed test-suite job upstream prevents
the release-publish job from running at all (verified via the job's
`needs:`/`if:` gating, not just documentation).

## Cross-cutting sequence for 9.1-9.6

| Order | Sub-phase | Depends on |
| --- | --- | --- |
| 1 | 9.1 GitHub Actions/App token | none (can start once Phase 7/8 have something to build) |
| 2 | 9.2 Windows build | 9.1, CMake Presets, Qt 6.11.1 build products |
| 3 | 9.3 Signing | 9.2 |
| 4 | 9.4 Updater | 9.2 (staged artifact shape), 3.5 outbox (already built) |
| 5 | 9.5 Podman Quadlet | Phase 8 server existing to deploy |
| 6 | 9.6 Release | 9.2, 9.3 all green |

## Acceptance criteria for closing Phase 9

Phase 9 is complete only when a real tagged commit produces a real GitHub
Release through this pipeline, with a human able to download the artifact,
verify its SHA-256 independently, run it on a real Windows machine, and
watch the updater successfully perform one full side-by-side update cycle
against a previous release -- not merely when the workflow YAML exists.
