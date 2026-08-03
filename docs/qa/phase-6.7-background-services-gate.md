# Phase 6.7 background-services gate

Verified 2026-08-03.

Implemented a bounded 32-service registry, 64-entry queue, two `std::jthread` lanes, cooperative stop tokens, prompt condition-variable wakeup, trigger coalescing, reserved synchronization capacity, network and idle gates, bounded diagnostics, failure disablement, monotonic scheduling, separate calendar rollover input, and bounded shutdown.

The static policy rejects service-owned threads and recurring timer APIs including QTimer, Win32 timers, sleeps, detach, timer-resolution changes, and shell execution. Tasks and completion callbacks run without the supervisor mutex, and diagnostics use immutable atomic shared-pointer snapshots.

Evidence: 28 focused checks, 0 failed; full strict gate 5,210 assertions, 0 failed; static policy 6 files passed; independent CMake build passed; CTest 33/33 passed.
