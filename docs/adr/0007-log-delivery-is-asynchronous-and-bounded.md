# 0007 - Log delivery is asynchronous, bounded, and never silently lossy

Status: Accepted, 2026-08-03

## Context

Until sub-phase 6.2a the logger wrote on the caller's thread. On a developer
machine that is invisible. On the machine this product is actually for, it is
not: a spinning disk waking up, an antivirus scanner deciding to inspect the
log file, or a data folder somebody redirected onto a network share can all
stall a single write for a long time. That stall would be felt by whoever is
standing at the counter waiting for an invoice to print, and it would be felt
for a reason the shop does not care about at all.

The obvious fix, handing lines to a background thread, brings its own set of
ways to be wrong, and the ways to be wrong are worse than the original problem.
An unbounded queue turns a logging storm into an out-of-memory crash. A queue
that drops lines quietly produces a log that is believed and is not true, which
is worse than having no log. A background thread that has not got round to the
write yet loses the last records before a crash, which are precisely the ones
somebody will be looking for.

## Decision

`AsyncLogSink` wraps a target sink, owns one writer thread, and hands lines to
it through a bounded first-in-first-out queue.

1. **The queue is bounded**, default 1024 entries, hard maximum 65536. A depth
   of zero is raised to one rather than treated as unbounded. Memory cost is
   therefore bounded by depth times `kMaxLogLineLength`, which is a number that
   can be stated rather than hoped about.

2. **When the queue is full the oldest line is dropped, not the newest.** Under
   pressure the recent lines are the ones that explain what is happening now.
   Dropping the newest would discard exactly the evidence being waited for.

3. **No gap is ever silent.** Dropped lines are counted, and as soon as the
   queue moves again the writer emits a real formatted record at warning level
   in the `logging` category carrying `dropped="N"`, placed immediately before
   the first line that follows the gap. If that declaration cannot itself be
   written, the debt is put back rather than lost. One declaration covers one
   gap, not one per line that follows it.

4. **Ordering is never disturbed.** One writer thread and a first-in-first-out
   queue mean lines reach the disk in the order they were written, and the
   target is touched only by the writer thread, so wrapped sinks are not
   required to be thread-safe.

5. **`flush()` is a real guarantee.** It returns only once everything queued
   before the call has reached the target and the target has itself been
   flushed. Because the logger already flushes after anything at Error or above,
   a fatal record is on the disk before the call that wrote it returns. This is
   what makes asynchronous delivery acceptable at all.

6. **Quiet work is flushed without being asked**, after two seconds of quiet by
   default, capped at five minutes so a mistyped configuration cannot mean
   "effectively never", and switched off entirely by zero or a negative value.
   The interval is armed only when lines have actually reached the target since
   the last flush, so an idle machine waits indefinitely and costs no wake-ups.

7. **A periodic flush does not advance the flush generation.** Advancing it
   could release a caller inside `flush()` whose own lines were still sitting in
   the queue, quietly breaking the one guarantee `flush()` exists to give.
   Periodic flushes are counted separately instead.

8. **Shutdown drains.** The destructor stops the writer only after the queue is
   empty and a final flush has completed. The last thing written before a
   machine is switched off is usually the reason it is being switched off.

9. **The drop counters stay on the sink, not on `LoggerCounters`.** A `Logger`
   does not know whether its sink is asynchronous, and inventing fields on it
   that are always zero for a synchronous sink would be a lie of a small but
   corrosive kind. `AsyncLogCounters` reports `submitted`, `written`, `dropped`,
   `gap_reports`, `flushes`, `periodic_flushes` and `peak_depth`, and the
   arithmetic `written + dropped == submitted` is asserted by the tests. Whoever
   assembles the logging stack at startup owns both objects and can report both.

## Consequences

A log line now costs a mutex, a string move and a condition-variable notify on
the caller's thread, and nothing else. The disk is somebody else's problem.

The log can be incomplete under sustained pressure, and that is deliberate: a
bounded log that says what it lost is better than an unbounded one that takes
the application down, and far better than one that lies by omission. Every kind
of loss in this stack is now declared somewhere a reader will find it. Records
held back by level are counted as `suppressed`; records held back by the
throttle are counted as `rate_limited` and declared in the log as `repeated="N"`
or a `throttled="summary"` line; records released from the run-up ring are
marked `backtrace="1"`; records dropped by the queue are declared as
`dropped="N"`; whole generations discarded by the hard budget are counted as
`discarded_files`. A combined test drives all of these in one stack and
reconciles the numbers exactly.

`peak_depth` is the number that tells an operator whether the configured depth
is anywhere near enough, and it is the first thing to look at if gaps appear.

One consequence is deferred rather than solved: the crash handler in sub-phase
6.3 cannot safely take this queue's mutex from a signal or exception handler.
Writing the crash record will need a path that does not go through the queue,
and that design is owed by 6.3 rather than assumed here.
