# ADR 0008: CrashCatch and bounded crash evidence

Status: accepted, 2026-08-03.

CrashCatch 1.5.0 is pinned as the platform crash engine behind SquiFlow's `CrashHandler` interface. Its built-in dialog and upload callback are disabled. Windows writes a DbgHelp minidump and CrashCatch report into the Phase 6.1 Crash directory; SquiFlow adds a bounded breadcrumb sidecar and a best-effort direct log line that bypasses `AsyncLogSink`.

The breadcrumb ring uses fixed storage, lock-free atomic publication, bounded text and oldest-first output. Each slot is atomically published so concurrent readers skip an incomplete record rather than print torn evidence.

The MIT notice lives once in `packaging/licenses/CrashCatch-MIT.txt`, not under `external/`. The supplied header is unchanged and pinned by SHA-256 in `external/crashcatch/README.squiflow.md`.

Limits: the Windows source cannot be compiled in this Linux lane. CrashCatch's Linux handler allocates before `fork()` and therefore is not claimed to be strictly async-signal-safe. The deterministic test lane uses `FakeCrashHandler`; restart UI remains Phase 6.8.
