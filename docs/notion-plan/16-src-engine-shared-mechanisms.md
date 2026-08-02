# src/engine/ — shared mechanisms

Source page id: 5a9fc6243f9f4a218d677db3749cee82

---

<callout icon="⚙">
	**Purpose.** The mechanisms every module borrows. A mechanism owns no business entity — that is exactly what distinguishes it from a module.
</callout>
## `storage/`
<table header-row="true">
<tr>
<td>File</td>
<td>Contract</td>
</tr>
<tr>
<td>`database.*`</td>
<td>**The only file that opens the database.** Everything else receives a handle. Write-ahead journaling, a busy timeout, a capped page cache</td>
</tr>
<tr>
<td>`writer.*`</td>
<td>The single-writer gate. Every write passes through it, which is what makes "one writer" enforceable rather than aspirational. Maintenance takes the gate at lowest priority and yields between slices</td>
</tr>
<tr>
<td>`statement.*`</td>
<td>Prepared-statement cache, bounded</td>
</tr>
<tr>
<td>`migration_runner.*`</td>
<td>Forward-only, transactional, **one global numbering sequence** across all modules. Refuses duplicate numbers</td>
</tr>
<tr>
<td>`integrity.*`</td>
<td>Startup check, and an honest failure path: say the store is damaged, offer to rebuild from the server, never pretend</td>
</tr>
<tr>
<td>`maintenance.*`</td>
<td>Checkpoint, incremental vacuum, statistics. Idle-gated, bounded slices, **plus a pass at shutdown and a forced pass if none has run in days**</td>
</tr>
</table>
## `records/`
<table header-row="true">
<tr>
<td>File</td>
<td>Contract</td>
</tr>
<tr>
<td>`identity.hpp`</td>
<td>Record identifier, created and updated stamps, soft deletion, the version used for conflict detection</td>
</tr>
<tr>
<td>`audit.*`</td>
<td>Who did what, on which device, when. Written in the same transaction as the change or not at all</td>
</tr>
<tr>
<td>`numbering.*`</td>
<td>Reserved blocks per device, top-up, **gaps are legitimate**, burned numbers on cancellation</td>
</tr>
<tr>
<td>`lifecycle.*`</td>
<td>Draft → issued → cancelled → replaced. One implementation so quotations, invoices and agreements cannot disagree about what cancelled means</td>
</tr>
<tr>
<td>`snapshot.*`</td>
<td>Freezes values at the moment of agreement, so a later price change cannot rewrite history</td>
</tr>
<tr>
<td>`approval.*`</td>
<td>**Moved here from being a module.** An approval belongs to the thing approved; it owns nothing itself</td>
</tr>
<tr>
<td>`signature.*`</td>
<td>Stroke data plus a rendered image. **Never a lossy format** — thin strokes are exactly what lossy compression destroys, and this is evidence</td>
</tr>
<tr>
<td>`reference.*`</td>
<td>A generic pointer to any record, so tasks and attention items can refer to anything without depending on it</td>
</tr>
</table>
## `sync/`
`outbox`, `cursor`, `orchestrator`, `transport`, `codec_msgpack`, `conflict`.
The rules that matter: **one sync at a time, held by a single permit**; the cursor is a server-assigned sequence, never a timestamp; the conflict rule is a version check resolving to the **owner's version**, with the losing version retained and visible; the codec is the component that gets fuzzed, because it parses bytes from the network.
## `identity/`
`session`, `rights` (generated from the protocol's declarations), `capability`.
**One function answers whether an operation may run**, given the person's rights, the device, and the connection state. The staff device's offline exception is data in a table, not a condition scattered across screens.
## `files/` and `services/`
`object_ref` and a bounded `thumbnail_cache` whose size is tracked as entries are written, so eviction is a query rather than a directory walk.
`service`, `supervisor`, `triggers`, `idle_detector`, `health` — one supervisor owning all threads, one coarse recurring tick in the whole program, mandatory cancellation, a failure budget that raises an attention item instead of retrying forever.
## Done when
The engine can be exercised with **no module present at all**: open, migrate, write through the gate, produce an audit row, allocate a number, drive a lifecycle, queue an outbox entry, and run a maintenance pass.
