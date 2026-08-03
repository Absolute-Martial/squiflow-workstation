# Phase 6.3 CrashCatch quality gate

Phase 6.3 installs CrashCatch behind a lifecycle-controlled platform interface, records lock-free bounded breadcrumbs, bypasses the asynchronous queue for the final crash line, restores prior handlers on uninstall, and leaves restart presentation to the shell.

## Evidence

- CrashCatch header 1.5.0 pinned unchanged; MIT notice stored only in packaging licenses.
- 29 permanent crash-handler checks, 0 failed.
- Full strict Makefile gate passed: 300 integrity files, 134 self-contained headers, all test programs 0 failed.
- Independent CMake configure/build completed to 100% with warnings as errors.
- Windows MiniDumpWriteDump adapter is written and structurally reviewed, but not compiled in this Linux sandbox.

## Failure boundaries

No upload callback and no CrashCatch dialog are enabled. The POSIX real-signal path terminates and is not used inside the deterministic test program. Crash artifacts remain under the resolved Crash directory.
