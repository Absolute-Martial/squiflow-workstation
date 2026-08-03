# ADR 0012: Bounded background supervisor

Status: Accepted

SquiFlow background services are functions, not thread owners. A central executor owns exactly two `std::jthread` workers: a synchronization-reserved lane and a shared lane. Tasks receive `std::stop_token`; worker waits register `std::stop_callback` so cancellation wakes them promptly. Shutdown stops admission, clears queued work, requests stop on both workers before joining either, and reports a bounded deadline result.

The supervisor owns bounded registration, trigger coalescing, failure budgets, network/idle gates, and immutable diagnostic snapshots published through atomic shared-pointer operations. It never invokes service callbacks while holding its registry mutex. `steady_clock` governs elapsed intervals; local calendar dates are supplied separately for day rollover because a monotonic clock has no timezone or midnight semantics. One externally owned coarse timer drives all periodic evaluation.
