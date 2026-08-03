# Phase 6.2 quality gate - a log a shop counter can live with

Verdict: **PASS**
Verified: 2026-08-03, GCC 11.5.0, `-std=c++23`, CMake 4.4.2
Schema: unchanged (migration 25). This sub-phase adds no table and no operation.

## 1. Feature under test

Structured logging with levels, one deterministic single-line format, size
rotation, numbered generations, and a **hard total byte budget** across the
whole log family. The shop counter has one disk, no operator, and no log
shipper, so the log is not allowed to become the incident.

## 2. Plan, as agreed before any code was written

| Item | Delivered |
| --- | --- |
| Event shape and levels | `src/platform/log_record.{hpp,cpp}` |
| Time seam plus system clock | `src/platform/log_clock.{hpp,cpp}` |
| One safe line, redaction, bounds | `src/platform/log_formatter.{hpp,cpp}` |
| Sink interface | `src/platform/log_sink.hpp` |
| Storage seam | `src/platform/log_storage.hpp` |
| Standard-library storage | `src/platform/local_log_storage.{hpp,cpp}` |
| Rotation and the hard cap | `src/platform/rotating_log_file.{hpp,cpp}` |
| Front door, thread-safe | `src/platform/logger.{hpp,cpp}` |
| Fakes before a second caller | `src/platform/testing/fake_log_storage.hpp`, `manual_log_clock.hpp`, `recording_log_sink.hpp` |
| Permanent strict test programme | `tests/platform/logging_test.cpp` |
| Build wiring | `src/platform/CMakeLists.txt`, `tests/CMakeLists.txt`, `tools/sandbox/Makefile` |

## 3. Behaviour proven

- **A message can never forge a second entry.** Newlines, carriage returns,
  tabs, quotes, backslashes, embedded zero bytes, and every other control
  character are escaped, so one record is one line no matter what a caller or
  a remote server puts in a string.
- **A credential never reaches the file.** Fields whose names look like
  passwords, tokens, secrets, passphrases, API keys, private keys, or
  credentials are written with the value replaced. The field itself is still
  recorded, so the diagnostic survives without the secret. Ordinary business
  names such as `invoice_key` are left alone.
- **Everything is bounded.** Message, field name, field value, field count,
  and the whole line each have a limit, and truncation is visible rather than
  silent. Dropped fields are counted in the line itself.
- **Timestamps are exact and defensive.** UTC, fixed width, computed without
  `gmtime`, and clamped at the epoch so a machine with a wrong clock cannot
  produce a timestamp that sorts above everything else. A clock that jumps
  backwards is recorded without complaint, and rotation never depends on it.
- **The budget is a promise.** After every rotation the family is measured and
  the oldest generations are deleted until it fits. When the only remaining
  way to honour the cap is to discard the live file, the live file is
  discarded and the loss is counted.
- **A misbehaving disk never stops the shop.** A refused append, a full
  volume, a file held open so it cannot be renamed, a delete that is refused,
  and a size that cannot be read are each detected, counted, and survived. No
  path throws.
- **Concurrency is real, not assumed.** Eight threads logging simultaneously
  produce exactly their own lines, each whole, none interleaved; four threads
  driving rotation still leave the family inside the cap.

## 4. Tests

| Level | Count | Where |
| --- | --- | --- |
| Unit: the pinned dispatcher version | 1 | `tests/platform/logging_test.cpp` |
| Unit: levels, timestamps, escaping, redaction, line shape | 96 | `tests/platform/logging_test.cpp` |
| Unit: logger behaviour and counters | 22 | same file |
| Unit: policy correction and generation naming | 16 | same file |
| Unit: rotation, hard cap, hostile storage, against the fake | 545 | same file |
| Integration: real temporary directory, real files | 30 | same file |
| Concurrency: eight-thread and four-thread runs | 7 | same file |
| **Total for 6.2** | **717 checks, 0 failed** | |

Hostile cases included: a line larger than the entire log file; a log family
inherited from an older version that already exceeds the cap; a budget too
small to hold two files; zero kept generations; a four-gigabyte requested file
size; a value half a megabyte long; thirty-nine fields where thirty-two are
allowed; a category containing an `=` and a quote; a nameless field; and a
storage name of `../escape.log`, which is refused rather than resolved and
leaves nothing outside the logs directory.

## 5. Gate results

```text
integrity            285 files checked, all pass
headers              127 headers, 0 not self-contained
strict suite         31 programs, 4,502 assertions, 0 failed
CMake 4.4.2 lane     configure clean, build clean, CTest 31/31 passed, 0.56 s
git diff --check     clean
```

## 5.1 Approved dependency revision

After this gate first passed, the owner supplied spdlog 1.17.0 and chose it as
the dispatcher. Quill 12.1.0 was supplied afterwards, evaluated, and rejected;
both the choice and the rejection are argued in
`docs/adr/0006-spdlog-is-the-logging-dispatcher.md`.

The swap changed no public signature and no caller. Escaping, redaction, the
length caps, rotation and the hard total budget remain SquiFlow code, because
spdlog offers none of them. One check was added, pinning the vendored version
through `logging_backend_version()` so an unreviewed upgrade fails the build.
Three defects were found and fixed while wiring it, listed in section 6.

Every gate above was re-run from scratch after the swap.

## 6. Defects found and fixed during the gate

1. **Three source files were reported written but were not on disk.**
   `log_sink.hpp`, `log_storage.hpp`, and `local_log_storage.{hpp,cpp}` were
   missing, which the build discovered as a missing make prerequisite rather
   than as a compile error. Severity: blocking. Root cause: a write that
   reported success without persisting. Fix: the files were written again and
   their presence confirmed by listing the directory before compiling.
   Regression guard: the strict lane cannot link without them, and the
   integrity gate requires every `.cpp` to appear in a build list.
2. **Two test expectations were wrong, not the code.** The expected sanitised
   category counted one underscore too many, and the rotation section wrote
   4,020 bytes into a 4,096-byte file and then asserted that rotation had
   happened. Severity: minor, test-only. Fix: the expected string was
   corrected and the loop now writes forty lines, which cannot fit. Both were
   verified as test errors by reading the produced output rather than by
   loosening the assertion.

No defect of any severity remains open.

## 7. Honest limits of this gate

- No Qt logging category bridge exists yet. It belongs with the shell in
  Phase 6.8, where there is an application to install a message handler into,
  and it will be a sink, not a second logging system.
- Crash-time flushing is proven only as far as the sink boundary. Writing a
  log line from inside a crash handler is Phase 6.3 and is deliberately not
  claimed here.
- `LocalLogStorage` opens, appends, flushes, and closes on every line. That is
  correct and durable, and it is slower than a held-open handle. Logging is
  not on the invoice path, so this is not optimised before it is measured.
- clang-tidy, clazy, and cppcheck are still not installed on this machine;
  their configuration is committed and runs on the Windows lane.

## 8. Verdict

No critical or major defect. Every required gate passed. Phase 6.2 is
approved.

## 9. Sub-phase 6.2a - the capabilities Quill was wanted for

Verdict: **PASS**
Verified: 2026-08-03, GCC 11.5.0, `-std=c++23`

Quill 12.1.0 was supplied and evaluated as a replacement dispatcher, and
rejected: swapping dispatchers again would have created work rather than saved
it, and none of what was actually wanted lives in the dispatcher. The
capabilities were built in SquiFlow's own code instead, against spdlog.

| Step | Delivered | Proven by |
| --- | --- | --- |
| 1 Per-category levels | `src/platform/log_level_policy.{hpp,cpp}` | Longest dotted prefix wins on a dot boundary; 32-rule ceiling; a configuration string the parser accepts round-trips |
| 2 Repeat throttling | `src/platform/log_throttle.{hpp,cpp}` | Identity is level, category and message; first sighting always speaks; Fatal never throttled; eviction settles what it owed; a backwards clock releases |
| 3 Deferred run-up | `src/platform/log_backtrace.{hpp,cpp}` | Ring keeps the newest on shrink; released records marked `backtrace="1"`; the dispatcher floor is forced to Debug while armed, without which released records would be discarded on the way out |
| 4 Asynchronous delivery | `src/platform/async_log_sink.{hpp,cpp}` | Bounded queue, drop-oldest, one counted `dropped="N"` declaration per gap, order preserved, shutdown drains, `flush()` a real guarantee |
| 5 Periodic flush | same | A quiet sink flushes unasked, then goes properly idle; zero and negatives mean off; a periodic flush never releases a waiting `flush()` |

### 9.1 Defects found and fixed

1. **Round-trip defect, step 1 (major).** `to_configuration()` built level names
   from the display vocabulary, where `Warning` is `"WARN "`, while the parser
   accepts only `warning`. The logger emitted a configuration string its own
   parser rejected. Fixed in the code; the display and configuration
   vocabularies are now kept deliberately separate.
2. **Dispatcher pre-emption, steps 1 and 3 (major).** spdlog holds one
   threshold. Without forcing the backend floor down, per-category levels and
   released run-up records would both be discarded inside the dispatcher, and
   the features would appear to work while producing nothing.
3. **Dead self-cancelling code, step 3 (minor).** A duplicated counter
   assignment was caught by reading the file before building it, and the
   function was rewritten rather than patched.
4. **Two mistaken expectations in the combined test (no code defect).** The
   throttle identifies a record by level, category and message and deliberately
   ignores fields, so two hundred invoices differing only in a number field are
   one repeated message to it. Records held back by level count as `suppressed`
   while records held back by the throttle count as `rate_limited`. Both
   expectations were corrected against the observed values; no assertion was
   loosened.

### 9.2 Gate results

| Gate | Result |
| --- | --- |
| Sandbox build, `-Werror` with the full warning set | Clean |
| Logging programme | 1,276 checks, 0 failed |
| Full suite | 31 programmes, 5,061 assertions, 0 failed |
| Integrity | 293 files checked, all pass |
| Header self-containment | 131 headers, 0 not self-contained |
| Whitespace | `git diff --check` clean |

### 9.3 Honest limits

- The crash handler in 6.3 cannot safely take the asynchronous queue's mutex
  from a signal or exception handler. Writing a crash record will need a path
  that does not go through the queue. That design is owed by 6.3 and is not
  claimed here.
- Periodic flushing is proven with real short intervals and a bounded wait
  whose releaser is the sink's own timer. It is not proven against a machine
  under heavy load, which this environment cannot arrange honestly.

### 9.4 Verdict

No critical or major defect remains open. Every required gate passed.
Sub-phase 6.2a is approved.
