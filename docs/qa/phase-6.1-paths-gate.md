# Phase 6.1 quality gate - where the shop's files live

Verdict: **PASS**
Verified: 2026-08-03, GCC 11.5.0, `-std=c++23`, CMake 4.4.2
Schema: unchanged (migration 25). This sub-phase adds no table and no operation.

## 1. Feature under test

The platform path layer. One machine-wide location for everything the shop
cannot afford to lose, one per-account location for cache, both validated,
created, and proven writable before the application is allowed to continue.

## 2. Plan, as agreed before any code was written

| Item | Delivered |
| --- | --- |
| OS-free interface for path resolution | `src/platform/paths.hpp` |
| Layout composition and validation | `src/platform/paths.cpp` |
| Filesystem seam | `src/platform/directory_probe.hpp` |
| Standard-library implementation | `src/platform/local_directory_probe.{hpp,cpp}` |
| Fake for the seam | `src/platform/testing/fake_directory_probe.hpp` |
| Root discovery, shipping lane | `src/platform/path_environment_qt.cpp` |
| Root discovery, verification lane | `src/platform/path_environment_posix.cpp` |
| Permanent strict test programme | `tests/platform/paths_test.cpp` |
| Build wiring | `src/platform/CMakeLists.txt`, `src/CMakeLists.txt`, `tests/CMakeLists.txt`, `tools/sandbox/Makefile` |
| Decisions recorded first | `docs/adr/0002`, `0003`, `0004`, `0005` |
| Governing rules adopted | `docs/engineering/code-of-conduct.md` |

## 3. Behaviour proven

- Records, logs, backups, crash dumps, and the secrets store resolve under the
  machine-wide root. Two different accounts on the same machine resolve to the
  **same** data directory and the same database file, and to **different**
  caches.
- Nothing is trusted: identity names are checked for emptiness, length,
  character set, trailing space or dot, and the full set of reserved Windows
  device names. Roots are checked for absoluteness, traversal, control
  characters, stray colons, and length.
- Every directory is created if missing and then proven writable by an actual
  write, not by a permission bit. A file occupying a directory name, a parent
  that cannot be examined, a creation that fails, and a directory that refuses
  writes each stop startup with a named fault, a named role, and the offending
  path.
- The cache is the only location allowed to disappoint: an absent, relative,
  or uncreatable per-account cache produces a warning and a cache inside the
  data directory. When even that cannot be made, startup refuses.
- Resolution is idempotent: a second run produces an identical layout and
  creates nothing.
- An unresolved layout refuses to answer, by exception, because asking is a
  programming error rather than a runtime condition.

## 4. Tests

| Level | Count | Where |
| --- | --- | --- |
| Unit, against the fake | 108 | `tests/platform/paths_test.cpp` |
| Integration, against a real temporary directory | 15 | same file |
| Discovery shape | 6 | same file |
| **Total for 6.1** | **129 checks, 0 failed** | |

Boundary and hostile cases included: a name exactly at the 48-character limit
and one character over; `COM9` refused and `COM10` accepted; `//server` refused
and `//server/share` accepted; `1:/data` refused; `C:ProgramData` refused; a
composed directory pushed past the usable Windows length; a dangling symlink
reported as occupied rather than missing; and a check that the write probe
leaves no file behind.

## 5. Gate results

```text
integrity            267 files checked, all pass
headers              116 headers, 0 not self-contained
strict suite         30 programs, 3,785 assertions, 0 failed
CMake 4.4.2 lane     configure clean, build clean, CTest 30/30 passed, 0.46 s
git diff --check     clean
```

## 6. Defects found and fixed during the gate

1. **Hex escape swallowed the following letter.** `"C:/Program\x01Data"` is one
   out-of-range hex escape, not a control character followed by `Data`; the
   compiler refused it under `-Werror`. Both control-character literals were
   rewritten as adjacent string literals. Found by the strict build, fixed
   before the first run.

No defect of any severity remains open.

## 7. Honest limits of this gate

- `path_environment_qt.cpp` is **not compiled here**. This machine has no Qt
  and no Windows, so the shipping discovery path is proven by review and by
  the Windows lane, not by this run. Everything it produces is handed to the
  resolver that these 129 checks exercise, so the unproven surface is
  discovery alone.
- `clang-tidy`, `clazy`, and `cppcheck` are not installed on this machine.
  Their configuration is committed; they run in the Windows lane and in CI
  from Phase 9. Recorded as a deviation in the code of conduct rather than
  claimed as passed.
- Sanitizers are available through `SQUIFLOW_ENABLE_SANITIZERS` and were not
  enabled for this run.

## 8. Verdict

No critical or major defect. Every required gate passed. Phase 6.1 is
approved.
