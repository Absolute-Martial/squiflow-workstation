# 0006 - spdlog is the logging dispatcher, behind the platform door

Status: Accepted, 2026-08-03

## Context

Phase 6.2 first shipped a hand-written dispatcher: a mutex, a level comparison,
a record builder, and a call into `LogSink`. It was complete and proved by 716
checks. The product owner then supplied spdlog 1.17.0 and asked for it to take
that place. Quill 12.1.0 was also considered and rejected; the reasoning is in
section "Alternatives" below.

The conduct requires a dependency to be justified against the standard library
and Qt rather than adopted by preference, so the justification is recorded
plainly, including what the dependency does not buy us.

What spdlog contributes: a mature, widely deployed dispatch core, a level
filter that is cheap on the rejecting path, an error-handler hook that turns a
failing sink into a counted event instead of a thrown exception escaping into
caller code, and an asynchronous logger and sink family that SquiFlow may adopt
later without another dependency decision.

What spdlog does not contribute, and what SquiFlow therefore keeps:

- **One record is one line.** A message containing a newline must not be able to
  forge a second entry. spdlog writes the payload it is handed.
- **A credential never reaches the file.** spdlog has no notion of a sensitive
  field. Redaction by caller discipline is exactly the failure this project
  refuses to accept, so `log_formatter` redacts, not the caller.
- **A hard total budget.** `rotating_file_sink` bounds `max_size x max_files`
  and never reconciles an inherited, already over-budget family of files.
- **Failure without exceptions.** `file_helper` throws on open, write, flush and
  size failures. A log failure must never break the operation that produced it.

## Decision

spdlog is vendored, header-only, at a pinned version under `external/spdlog`,
and is used **only** as the dispatcher inside `src/platform/logger.cpp`.

1. No spdlog type appears in any SquiFlow header. `Logger` holds a PIMPL, so
   `logger.hpp` names no spdlog entity and no caller inherits the include path.
2. The include directory is `PRIVATE` and `SYSTEM` on `squiflow_platform`.
   Nothing above the platform boundary can even name spdlog, and its headers
   never raise our warnings under `-Werror`.
3. `SPDLOG_HEADER_ONLY` is never defined on the command line. spdlog defines it
   itself when `SPDLOG_COMPILED_LIB` is absent, and a second definition is a
   fatal redefinition warning under `-Werror`.
4. SquiFlow formats the line first. spdlog receives one finished string through
   a literal `"{}"` pattern, so no caller text is ever parsed as a format
   specification and a stray brace cannot throw.
5. An adapter sink converts a `false` return from `LogSink::write_line` into a
   `spdlog_ex`, which spdlog routes to our error handler, which counts a sink
   failure. Nothing propagates to the caller.
6. `logging_backend_version()` exposes the pinned version as a string through
   the platform API, and `logger.cpp` holds a `static_assert` on the three
   spdlog version macros. Changing the vendored copy breaks the build and forces
   a review rather than passing silently.

## Alternatives

**Keep the hand-written dispatcher.** Perfectly adequate, roughly sixty lines,
zero supply-chain surface. Rejected only because the owner wants a maintained
core and a documented path to asynchronous logging.

**Quill 12.1.0.** Rejected. Quill is asynchronous by construction: the front end
enqueues and a backend thread started by `Backend::start()` calls
`Sink::write_log`. Our sink contract and our tests are synchronous, so every
record would need a polling `flush_log()`, which discards the one advantage
Quill has. Its speed comes from deferred formatting, which SquiFlow cannot use
because escaping, redaction and the length cap must happen before the line is
queued. On Windows `Backend::start()` installs structured exception and console
control handlers, which collides directly with the Phase 6.3 crash handler.

## Consequences

- One more vendored dependency, 110 header files, MIT licensed, no build step.
- The logging public API is unchanged; no caller was touched by this swap.
- Upgrading spdlog is a deliberate act: the `static_assert` fails first.
- If asynchronous delivery is wanted, it is a change inside `logger.cpp` alone.
