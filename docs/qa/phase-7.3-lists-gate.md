# Phase 7.3 -- bounded reusable lists gate

Date: 2026-08-04

## Result

The reusable list contract, bounded PMR-backed page ownership, query
intent validation, stale-generation handling, Qt adapter source, and QML
list states are implemented. All portable behavior and architecture gates
pass. The Qt runtime lane remains unexecuted in this image because no built
Qt 6.11.1 SDK is installed, so 7.3 is recorded as `[~]` pending that
hosted/runtime evidence.

## Delivered contracts

- `ListBridge` validates a bounded, nonempty field manifest and rejects
  unknown, unsortable, or unfilterable fields before a query is emitted.
- Refresh, sort, filter, and next-page requests carry stable field IDs and a
  monotonically increasing generation. A stale worker result cannot replace
  a newer query.
- Filtering is limited to 256 bytes and every page to 100 rows.
- `PagedListCache` retains at most three owned `PageBuffer` instances (300
  rows), each backed by its own lifetime-safe PMR arena. A fourth page evicts
  the oldest; an evicted selection is cleared.
- Rows are owned snapshots (`id`, `title`, `subtitle`), never domain pointers.
- Each of the twelve manifest routes now owns an actual validated list bridge
  with module-appropriate column IDs.
- `ListScreenBridgeQt` exposes rows and column metadata as presentation-safe
  Qt values, queues worker page/failure delivery via `Qt::QueuedConnection`
  and `QPointer`, and emits page intents for composition-root service wiring.
- `DataList.qml` implements loading, empty, error, refresh, next-page,
  selection, sort, and filter presentation without sorting/filtering the
  authoritative row set inside QML.

## Focused evidence

```text
paged list cache: 10 checks, 0 failed
list bridge:      35 checks, 0 failed
manifest/list integration included in:
                   79 checks, 0 failed
```

The list tests cover empty/invalid column sets, duplicate and malformed
fields, unknown sort/filter fields, oversized filters, empty results,
single and multiple pages, exact page-size boundary, 101-row refusal,
stable selection, removed/stale selection, concurrent refresh generations,
retry after malformed input, explicit failure state, cancellation during an
in-flight request, stale completion after close, 300-row retained boundary,
fourth-page eviction, and exact snapshot contents.

## Full local gate

```text
47 test programs
5,484 assertions, 0 failed
174 self-contained headers, 0 failed
integrity: 388 files checked, all pass
Navigation/list policy passed
UI resource policy passed
Qt bridge policy passed
QML boundary policy and six fixtures passed
```

Command:

```text
make -f tools/sandbox/Makefile check
```

## Deferred runtime evidence

The Qt-capable Linux/MSVC lane still needs to prove:

- exact `PagedListModelQt`/`ListScreenBridgeQt` model signals and role values;
- queued page delivery from a worker and delivery after receiver destruction;
- QML loading/empty/error/large-list states;
- keyboard row activation and focus restoration;
- compiled `ModuleListScreen.qml` resource resolution.

No local CMake/CTest result is claimed: this image reports
`cmake: command not found`.

## Commit

```text
84457a2 feat(ui): add bounded reusable module lists
```
