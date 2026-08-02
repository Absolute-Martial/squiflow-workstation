# Implementation plan — phases, and what can actually be verified

Source page id: 6803da2420774321be16f06a7084423c

---

<callout icon="⚠️">
	**Read this before the code.** I checked the build environment rather than assuming it. What can be *compiled and run* here is a genuine subset of the project, and I will not describe anything as verified when it was not.
</callout>
## What the build environment actually has
<table header-row="true">
<tr>
<td>Tool</td>
<td>Present?</td>
<td>Consequence</td>
</tr>
<tr>
<td>C++ compiler</td>
<td>**Yes** — GCC 11.5.0</td>
<td>Pure C++ compiled **and executed**. **Both lanes now build at ****`-std=c++23`****.** But a compiler can accept that flag and still ship only part of the C++23 *library*, and this one does: measured `__cplusplus` is **202100**, not 202302. `std::span`, `ranges`, `source_location`, `to_underlying` are present; **`std::expected`****, ****`std::format`****, ****`std::print`****, ****`byteswap`****, ****`if consteval`****, deducing ****`this`**** are not.** So the rule is enforced rather than trusted: the integrity pass **fails the build** on `<expected>`, `<format>`, `<print>`, `<stacktrace>`, `<flat_map>`, `<generator>`. The two lanes cannot silently drift apart</td>
</tr>
<tr>
<td>CMake</td>
<td>**No**</td>
<td>The CMake files are written to the plan but **cannot be executed here**. They are unverified until built on a real machine. A plain makefile drives the verification lane instead</td>
</tr>
<tr>
<td>Qt</td>
<td>**No**</td>
<td>**Nothing in the interface layer can be compiled here at all.** QML and view models will be written, and honestly marked unverified</td>
</tr>
<tr>
<td>SQLite headers</td>
<td>**No** (runtime library only, no headers, and no permission to install)</td>
<td>The data layer cannot be compiled here without a substitute. Options are discussed at the phase where it bites</td>
</tr>
<tr>
<td>Package installation</td>
<td>**No** — no administrator rights in the sandbox</td>
<td>None of the above can be fixed by installing something</td>
</tr>
</table>
**The honest summary: roughly the domain layer, the protocol spine, the module graph, the rules engine and the workflow logic can be compiled and run here. Storage, sync transport, platform code and the entire interface cannot.** Every phase below states which side of that line it falls on.
---
## Phases
<table header-row="true">
<tr>
<td>Phase</td>
<td>What it delivers</td>
<td>Verification</td>
</tr>
<tr>
<td>**1 — Setup and the protocol spine**</td>
<td>Repository skeleton, all CMake files, and the thing everything else depends on: the module list with tiers, the dependency graph, the rights list, the operation table</td>
<td>**Compiled and executed.** Tests prove the graph is acyclic, core is closed under dependency, operation identifiers are unique, and deactivation closure behaves</td>
</tr>
<tr>
<td>**2 — Engine, domain half**</td>
<td>Record identity, lifecycle states, numbering, money, quantity, snapshots, approval and signature types, the rights check</td>
<td>**Compiled and executed** — this layer touches no database and no Qt by design</td>
</tr>
<tr>
<td>**3 — Engine, storage half**</td>
<td>Database gate, single writer, migration runner, outbox, cursor, conflict rules</td>
<td>**Partly.** Logic behind an interface is compiled; the SQLite-facing implementation is written but unverified here</td>
</tr>
<tr>
<td>**4 — The twelve modules**</td>
<td>Each module's domain and service layers, its tables, its operations</td>
<td>**Domain and service compiled and tested.** Data layer written, unverified</td>
</tr>
<tr>
<td>**5 — Workflows**</td>
<td>The cross-module sequences: counter sale, quote to order, issue invoice, cancel and reissue, apply agreement, take payment, record purchase</td>
<td>**Compiled and executed against in-memory fakes** — the most valuable tests in the project</td>
</tr>
<tr>
<td>**6 — Platform and application shell**</td>
<td>Windows interfaces plus their fakes, startup order, composition root, activation</td>
<td>**Fakes compiled**; the Windows implementations written, unverified</td>
</tr>
<tr>
<td>**7 — Interface**</td>
<td>Theme, controls, patterns, module screens, view models</td>
<td>**Not verifiable here.** Written and marked as such</td>
</tr>
<tr>
<td>**8 — Server**</td>
<td>Sync endpoints, identity, PostgreSQL schema, media worker, update proxy, container files</td>
<td>**Not verifiable here** — no Oat++, no PostgreSQL</td>
</tr>
<tr>
<td>**9 — Pipeline and packaging**</td>
<td>CI workflows, staging, manifest, signing, installer, updater</td>
<td>Scripts written; **the manifest and hashing tools can be run** here</td>
</tr>
</table>
## Rules for every phase
- **The architecture rules are enforced by code, not by discipline.** Cross-references between modules, rights and operations go through enumerations, so a mistake becomes a compile error rather than a runtime surprise.
- A phase ends with its tests passing, or with an explicit statement of what could not be run and why.
- **No invented numbers.** Budgets get filled in from the real machine.
- Nothing is called done because it looks done.
---
## Sub-phases, and what "done" is allowed to mean
A phase like "build the twelve modules" is not a goal, it is a mood. There is no moment where you can say it is finished, so it never is, and everything found at the end is found on top of work already built. **Each phase is therefore cut into sub-phases**, and a sub-phase has to fit a shape: it names its files, it fits in one sitting, it ends in something that *runs*, and the next one does not start until it passes every gate.
### The five gates
<table header-row="true">
<tr>
<td>Gate</td>
<td>What it catches</td>
</tr>
<tr>
<td>**1 · Integrity**</td>
<td>Encoding, BOMs, CRLF, tabs, trailing whitespace, non-ASCII, missing `#pragma once`, a `.cpp` not including its own header first, `.def` files drifted out of X-macro shape, and **orphaned sources named by no build file**</td>
</tr>
<tr>
<td>**2 · Self-containment**</td>
<td>Every header compiles **alone, twice**, with nothing included before it. A header that works only because something else came first is a trap for whoever includes it next</td>
</tr>
<tr>
<td>**3 · Compilation**</td>
<td>`-Werror` with the full set. `-Wconversion` and `-Wsign-conversion` matter most here: this software computes money, and a silent narrowing is a wrong number on an invoice, not a style problem</td>
</tr>
<tr>
<td>**4 · Execution**</td>
<td>Compiling proves the shape of the code. Only running proves the behaviour</td>
</tr>
<tr>
<td>**5 · Honesty**</td>
<td>Any file that cannot be verified here is **listed by name**, never quietly counted as done</td>
</tr>
</table>
One command runs all of it, and fails with a non-zero exit code:
```plain text
make -f tools/sandbox/Makefile check
```
### The plan now lives in the repository
Under `docs/plan/` — so it travels with the code and is reviewable in the same commit:
- **`phases.md`** — all nine phases broken into sub-phases, each with its files and its done-condition. The intention.
- **`status.md`** — what is actually finished, with the output that proves it. The record.
- **`language-and-verification.md`** — the C++23 rules and the measured gap.
- **`README.md`** — the gates, and how to run them.
`phases.md` and `status.md` are separate on purpose: **a plan that edits itself to match what happened is not a plan.**
### Phase 1, stated properly
<table header-row="true">
<tr>
<td>#</td>
<td>Sub-phase</td>
<td>Done when</td>
</tr>
<tr>
<td>1.1</td>
<td>Repository skeleton and build logic</td>
<td>Release, debug, sanitizer and headless configurations exist; module declaration helper and graph checks written. **CMake unproven — none installed here**</td>
</tr>
<tr>
<td>1.2</td>
<td>The module graph</td>
<td>Twelve modules with tiers; cycles detected and named; core proven closed under dependency; activation closure computed</td>
</tr>
<tr>
<td>1.3</td>
<td>Rights</td>
<td>43 rights, each owned by exactly one module</td>
</tr>
<tr>
<td>1.4</td>
<td>The operation table</td>
<td>67 operations carrying right, sync class and offline rule; lookup by name rejects unknowns rather than guessing</td>
</tr>
<tr>
<td>1.5</td>
<td>Offline rules and staff exceptions</td>
<td>Counter-sale exceptions are data, and a test refuses any entry contradicting its operation's own offline rule</td>
</tr>
<tr>
<td>1.6</td>
<td>The verification harness</td>
<td>One command runs language probe, integrity, self-containment and both test programs</td>
</tr>
</table>
**Phase 1 is complete when** the operation table, the rights list and the module graph cannot disagree with each other without something failing — and that failure can be produced by one command.
Phase 2 is cut the same way, 2.1 through 2.7: identity and time · quantity · money · lifecycle · numbering · snapshots and signatures and approvals · rights and session and capability.
---
## Where it actually stands
```plain text
== language ==
__cplusplus 202100
compiler   g++ (GCC) 11.5.0
standard   c++23

== integrity ==
50 files checked, all files pass

== header self-containment ==
19 headers, 0 not self-contained

== protocol ==
12 modules · 43 rights · 67 operations
44 of 67 usable offline · core 6, extra 6
all checks passed

== engine ==
93 checks, 0 failed
```
**Phases 1 and 2: done.** Phase 3 onward: not started.
<callout icon="⚠️">
	**Corrections to the record — kept because a status file that only records successes is not a record.**<br><br>**The Phase 2 files were lost, and the download did not contain them.** The archive handed over as "phases 1 and 2" held only the 43 Phase 1 files. The engine code was missing from both the working tree and the archive while being described as delivered. Regenerated and re-verified.<br><br>**The regenerated files were the pre-fix versions** — earlier hand-fixes were never folded back into the generator, so two compilation errors returned. `staff_offline.def` had been lost entirely, and `staff_offline_exception` was called in two places and declared in none. Fixed in the tree itself this time.<br><br>**The language was C++20, not C++23** — the sandbox makefile said one thing while CMake said another.<br><br>**The integrity pass found three problems on its first run.** Two were the checker being wrong. One was real: a makefile recipe wrote to a directory before creating it, and the language probe had been silently reporting `__cplusplus 0` — a check that reports success without measuring anything is worse than no check.
</callout>
