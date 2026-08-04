# Phase 5.8 -- Prepare a document for approval or email

Status: planned, not started. Blocks nothing in Phase 6 or 7. Blocked by
nothing either -- it can be implemented any time after 5.1 (workflow
framework), 2.6 (approvals/signatures/audit) and 3.5 (outbox) are in place,
which they are.

## Decision this sub-phase is built on

**D5, decided:** the shopkeeper marks a prepared document approved by hand.
The system never reads or parses email replies to infer approval. If that
ever changes it is a new sub-phase, not a revision of this one.

## Goal

Give every document-producing module (quotations, receivables/statements,
agreements) one shared, audited path to:

1. Freeze an immutable snapshot of a record as document content.
2. Let a human with the right approve that snapshot, by hand.
3. Record an explicit, separate intent to send -- which Phase 8.7 (mail) is
   the only thing ever allowed to act on.

Nothing in this sub-phase sends anything. "Send" here means "durably record
 that a human pressed the button"; the actual transport is server-side and
out of scope until 8.7.

## Scope

- A `document_preparation` workflow, following the 5.1 workflow framework
  rule: one transaction, one audit entry, one refusal path.
- A `PreparedDocument` engine value type: source record reference, revision
  number, frozen content (already-rendered fields, not live pointers back to
  the mutable record), prepared-by, prepared-at.
- An `Approval` record reusing the Phase 2.6 approval/signature value types,
  never inventing a second approval concept.
- A `SendIntent` record: who asked to send, when, to which
  address/recipient, and nothing else -- no transport metadata, because that
  belongs to 8.7.
- Migration 25 adding the prepared-document, approval, and send-intent
  tables.

## Non-goals

- No SMTP, no mail server, no attachment transport. That is 8.7.
- No PDF rendering. That is 7.5; this sub-phase's "content" is structured
  data, not a rendered file. 7.5 renders a `PreparedDocument` to bytes; this
  sub-phase only freezes the data those bytes will come from.
- No reply parsing, no approval inference, per D5.
- No re-opening of an approved document. A correction is a new revision
  through a new `document_preparation` call, never a mutation of an approved
  one.

## Files

| File | Purpose |
| --- | --- |
| `src/engine/documents/prepared_document.hpp/.cpp` | `PreparedDocument`, `Approval` reuse, `SendIntent` value types |
| `src/workflows/document_preparation.hpp/.cpp` | The workflow: prepare, approve, request-send |
| `src/storage/migrations/migration_025_prepared_documents.*` | New tables |
| `tests/workflows/document_preparation_test.cpp` | Harsh tests, see below |
| `docs/qa/phase-5.8-document-preparation-gate.md` | Evidence, written after the gate passes |

## Invariants

1. Preparing a document never mutates the source record; it reads a
   snapshot and writes a new, independent row.
2. An approval requires an explicit right (reuse or extend the Phase 2.6
   approval right; do not invent a parallel rights concept).
3. A `SendIntent` can only be created against an *approved* prepared
   document. Attempting to request a send on an unapproved or superseded
   document is refused, not silently queued.
4. A prepared document is immutable once created. "Correcting" it always
   creates a new revision referencing the same source record; the old
   revision is retained, never deleted, matching the 5.5 cancel/reissue
   precedent.
5. One workflow call, one transaction, one audit row, one outbox row --
   exactly the 5.1 rule, no exception carved out for this workflow.
6. Approving or requesting a send are each their own call with their own
   right check; neither is a side effect of the other.

## Tests (`tests/workflows/document_preparation_test.cpp`)

- Normal: prepare a quotation snapshot, approve it, request a send.
- Invalid: prepare against a nonexistent source record id.
- Invalid: approve without the approval right.
- Invalid: request a send on an unapproved document.
- Invalid: request a send on a superseded (revised) document.
- Empty: prepare a record with no content fields populated yet (e.g. a
  quotation with zero lines) -- refused with a named reason, not a blank
  document.
- Boundary: prepare the maximum content size the value type allows;
  one byte over is refused.
- Malformed: a workflow request missing the source record type.
- Concurrency: two `document_preparation` calls against the same source
  record at once produce two independent revisions, never one corrupted
  row.
- Rollback: a failure partway through persistence leaves no partial
  `PreparedDocument`, `Approval`, or `SendIntent` row and no orphaned audit
  or outbox entry.
- Replay: the outbox entry for a prepared document replays idempotently.

## Sequence

| Sub-step | Content |
| --- | --- |
| 5.8.0 | Preflight: confirm 2.6 approval type and 3.5 outbox shapes are reusable as-is; no changes expected. |
| 5.8.1 | Engine value types: `PreparedDocument`, `SendIntent`. |
| 5.8.2 | Workflow definition and registration in the 5.1 workflow registry. |
| 5.8.3 | Migration 25. |
| 5.8.4 | Full test suite above, strict gate, CMake gate, gate document. |

## Acceptance criteria

- All tests above pass with zero failures.
- Full strict gate and independent CMake gate both pass with this sub-phase
  included.
- `docs/qa/phase-5.8-document-preparation-gate.md` written with exact
  numbers, no rounding.
- `docs/plan/todo.md` row for 5.8 checked only after the gate document
  exists.
