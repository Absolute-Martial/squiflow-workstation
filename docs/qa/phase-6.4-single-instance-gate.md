# Phase 6.4 single-instance quality gate

- Stable database-derived identity without path disclosure.
- Machine-wide Windows mutex and auto-reset activation event with explicit authenticated-user ACL.
- POSIX `flock` verification path and deterministic fake.
- Secondary processes cannot become primary and signal activation without opening the database.
- Abrupt process death releases ownership; symlink lock paths fail closed.
- Permanent suite: 32 checks, 0 failed.
- Full strict gate: 5,122 assertions across 33 programs, 0 failed.
- Integrity: 307 files; header self-containment: 137 headers.
- Independent CMake build reached 100%; CTest passed 29/29.

The Windows source is not compiled or executed in this Linux sandbox; this limit is explicit rather than counted as verified.
