# Phase 5.1A quality gate — workflow protocol and audit persistence

Phase 5.1A establishes the two engine/protocol mechanisms needed by the workflow framework. It does **not** install workflow handlers or alter `modules::Registry`; those changes remain behind the 5.1B approval boundary.

## Delivered surface

- `protocol::workflow_operations()` is generated from `operations/workflows.def`.
- `protocol::is_workflow_operation()` accepts every `OperationId`, including unknown/future values, without indexing outside a table.
- The classified surface is exactly eight unique operations.
- `engine::AuditLog` owns one generic audit table keyed by workflow idempotency key.
- Audit IDs are deterministic across retries, devices, and process restarts.
- Audit rows preserve operation, person, device, timestamp, generic subject, and human-readable detail.
- Typed lookup supports idempotency key, operation, and generic subject.
- Invalid identities, operations, timestamps, subjects, blank details, mismatched deterministic IDs, and duplicate keys are refused before a partial row can remain.
- Decoding malformed stored enum values produces invalid sentinel values rather than undefined behavior.

## Sequential file gates

1. `workflow_table.hpp` included twice in isolation under the full `-Werror` warning set.
2. `workflow_table.cpp` compiled independently; a direct probe proved exactly eight unique workflow operations, ordinary operations excluded, and `OperationId::Count` handled safely.
3. `audit_log.hpp` included twice in isolation under the full `-Werror` warning set.
4. `audit_log.cpp` compiled independently; a direct in-memory probe proved insert, typed lookup, deterministic identity, duplicate refusal, blank-detail refusal, and rollback cleanliness.

## Repository gates

- Strict sandbox gate: passed.
- Integrity: 228 files checked, zero problems before this document was added.
- Header self-containment: 100 headers, zero failures.
- Existing regression programs: 21, all passed; 3,288 assertions, zero failed.
- Independent CMake 4.4.2 configure/build: passed with SQLite disabled only for the documented verification lane.
- Module graph: 12 modules, acyclic, core closed.

## Boundary

Phase 5.1 remains incomplete. 5.1B will define workflow handlers and registry ownership/activation rules. 5.1C will add migration 22, permanent harsh workflow tests, and the final framework integration gate. No 5.1B or 5.1C implementation is included here.
