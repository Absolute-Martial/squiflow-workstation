// Phase 3.5: the outbox.
//
// Two failures are being defended against, and both are the kind that are
// discovered by a customer rather than by a test suite: a change sent twice,
// and a change sent in the wrong order. Most of what follows is about those.
//
// The clock here is a variable rather than the wall clock, so backoff can be
// asserted exactly instead of approximately.

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <squiflow/protocol/operation_table.hpp>

#include "engine/storage/memory_store.hpp"
#include "engine/storage/store.hpp"
#include "engine/sync/outbox.hpp"
#include "support/check.hpp"

namespace {

using squiflow::engine::Blob;
using squiflow::engine::EnqueueResult;
using squiflow::engine::MemoryStore;
using squiflow::engine::Outbox;
using squiflow::engine::OutboxEntry;
using squiflow::engine::OutboxState;
using squiflow::engine::Store;
using squiflow::engine::StoreError;
using squiflow::engine::Transaction;
using squiflow::engine::backoff_for_attempt;
using squiflow::protocol::OperationId;
using squiflow::testing::check;
using squiflow::testing::report;
using squiflow::testing::section;

std::int64_t g_now = 1'000'000;

std::int64_t fake_clock() {
    return g_now;
}

Blob payload_of(const std::string& text) {
    return Blob{text.begin(), text.end()};
}

OutboxEntry make_entry(const std::string& key, OperationId operation,
                       const std::string& record) {
    OutboxEntry entry;
    entry.idempotency_key = key;
    entry.operation = operation;
    entry.record_id = record;
    entry.payload = payload_of("payload for " + key);
    return entry;
}

class Fixture {
public:
    Fixture() : outbox_(fake_clock) {
        g_now = 1'000'000;
        Outbox::define(store_);
    }

    MemoryStore& store() {
        return store_;
    }

    const Outbox& outbox() const {
        return outbox_;
    }

    template <typename Callable>
    void in_transaction(Callable&& callable) {
        auto transaction = store_.begin();
        callable(*transaction);
        transaction->commit();
    }

    EnqueueResult enqueue(const std::string& key, OperationId operation,
                          const std::string& record) {
        EnqueueResult result = EnqueueResult::Enqueued;
        in_transaction([&](Transaction& transaction) {
            result = outbox_.enqueue(transaction, make_entry(key, operation, record));
        });
        return result;
    }

    std::vector<OutboxEntry> claim(std::size_t batch = Outbox::kDefaultBatch) {
        std::vector<OutboxEntry> claimed;
        in_transaction([&](Transaction& transaction) {
            claimed = outbox_.claim(transaction, batch);
        });
        return claimed;
    }

    OutboxState state_of(const std::string& key) {
        const auto entry = outbox_.get(store_, key);
        return entry ? entry->state : OutboxState::Failed;
    }

private:
    MemoryStore store_;
    Outbox outbox_;
};

template <typename Callable>
bool refuses(Callable&& callable) {
    try {
        callable();
    } catch (const StoreError&) {
        return true;
    }
    return false;
}

void test_what_may_be_queued() {
    section("what may be queued");

    Fixture fixture;

    check(refuses([&fixture] { fixture.enqueue("", OperationId::party_create, "p1"); }),
          "an entry with no idempotency key is refused");
    check(refuses([&fixture] { fixture.enqueue("k1", OperationId::party_create, ""); }),
          "an entry pointing at no record is refused");

    // A local-only operation has nothing to send; an online-required one was
    // never allowed to happen offline. Either in the outbox is an upstream
    // bug, and a silent one if it is tolerated here.
    check(refuses([&fixture] { fixture.enqueue("k2", OperationId::document_print, "i1"); }),
          "a local-only operation is refused");
    check(refuses([&fixture] { fixture.enqueue("k3", OperationId::person_create, "u1"); }),
          "an online-required operation is refused");

    check(fixture.enqueue("k4", OperationId::party_create, "p1") == EnqueueResult::Enqueued,
          "a synchronizable operation is accepted");

    const auto entry = fixture.outbox().get(fixture.store(), "k4");
    check(entry.has_value(), "the entry can be read back");
    check(entry->state == OutboxState::Pending, "it starts pending");
    check(entry->attempts == 0, "with no attempts yet");
    check(entry->due_at == g_now, "and is due immediately");
    check(entry->payload == payload_of("payload for k4"), "the payload survived the round trip");
    check(entry->module() == squiflow::protocol::ModuleId::parties,
          "the module comes from the operation table, so the two cannot disagree");
}

void test_enqueuing_is_itself_idempotent() {
    section("enqueuing twice");

    Fixture fixture;
    check(fixture.enqueue("same", OperationId::party_create, "p1") == EnqueueResult::Enqueued,
          "the first enqueue is accepted");

    // A caller retrying its own write is not an error, and must not produce a
    // second copy of the change.
    check(fixture.enqueue("same", OperationId::party_update, "p1") ==
              EnqueueResult::AlreadyQueued,
          "the second is reported as already queued rather than throwing");
    check(fixture.outbox().counts(fixture.store()).total() == 1, "and only one entry exists");

    const auto entry = fixture.outbox().get(fixture.store(), "same");
    check(entry->operation == OperationId::party_create,
          "the first entry was not overwritten by the second attempt");
}

void test_claiming_a_batch() {
    section("claiming a batch");

    Fixture fixture;
    for (int index = 0; index < 5; ++index) {
        fixture.enqueue("k" + std::to_string(index), OperationId::party_create,
                        "record-" + std::to_string(index));
    }

    check(refuses([&fixture] { fixture.claim(0); }), "a batch of zero is refused");
    check(refuses([&fixture] { fixture.claim(Outbox::kMaximumBatch + 1); }),
          "a batch above one hundred is refused");

    const auto first = fixture.claim(3);
    check(first.size() == 3, "a batch of three returns three");
    check(first[0].idempotency_key == "k0" && first[2].idempotency_key == "k2",
          "in the order they were made");
    check(first[0].attempts == 1, "claiming counts as an attempt");
    check(fixture.state_of("k0") == OutboxState::InFlight, "claimed entries are in flight");

    const auto second = fixture.claim(50);
    check(second.size() == 2, "the next claim returns only what is left");
    check(second[0].idempotency_key == "k3", "continuing in order");

    check(fixture.claim().empty(), "nothing is claimed twice");
}

void test_one_record_is_sent_in_order() {
    section("one record, in order");

    Fixture fixture;
    // Three changes to the same invoice, then one to a different party.
    fixture.enqueue("a1", OperationId::invoice_draft_create, "invoice-1");
    fixture.enqueue("a2", OperationId::invoice_draft_update, "invoice-1");
    fixture.enqueue("a3", OperationId::payment_allocate, "invoice-1");
    fixture.enqueue("b1", OperationId::party_create, "party-9");

    const auto claimed = fixture.claim();
    check(claimed.size() == 2, "only one change per record goes out at a time");
    check(claimed[0].idempotency_key == "a1", "the earliest change to the invoice");
    check(claimed[1].idempotency_key == "b1", "and the unrelated record is not held up");

    // A payment arriving before the invoice it pays makes no sense on the
    // server, so the second change waits for the first to land.
    check(fixture.state_of("a2") == OutboxState::Pending, "the second change is still waiting");

    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.outbox().mark_applied(transaction, "a1", 101);
    });

    const auto next = fixture.claim();
    check(next.size() == 1 && next[0].idempotency_key == "a2",
          "once the first has landed, the second goes");
}

void test_the_ordinary_path() {
    section("the ordinary path");

    Fixture fixture;
    fixture.enqueue("k1", OperationId::party_create, "p1");
    const auto claimed = fixture.claim();

    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.outbox().acknowledge(transaction, "k1");
    });
    check(fixture.state_of("k1") == OutboxState::Acknowledged, "the server confirmed receipt");
    check(!fixture.outbox().drained(fixture.store()),
          "acknowledged is not finished; it has no sequence yet");

    check(refuses([&fixture] {
              fixture.in_transaction([&fixture](Transaction& transaction) {
                  fixture.outbox().mark_applied(transaction, "k1", 0);
              });
          }),
          "applying without a sequence is refused");

    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.outbox().mark_applied(transaction, "k1", 77);
    });

    const auto entry = fixture.outbox().get(fixture.store(), "k1");
    check(entry->state == OutboxState::Applied, "and then it was applied");
    check(entry->server_sequence == 77, "carrying the sequence the server assigned");
    check(fixture.outbox().drained(fixture.store()), "the outbox is drained");
    check(claimed.size() == 1, "one entry made the whole trip");

    check(refuses([&fixture] {
              fixture.in_transaction([&fixture](Transaction& transaction) {
                  fixture.outbox().acknowledge(transaction, "k1");
              });
          }),
          "an applied entry cannot be acknowledged again");
    check(refuses([&fixture] {
              fixture.in_transaction([&fixture](Transaction& transaction) {
                  fixture.outbox().mark_applied(transaction, "missing", 5);
              });
          }),
          "an unknown key is refused rather than ignored");
}

void test_backoff() {
    section("backoff");

    check(backoff_for_attempt(1) == 5000, "the first wait is five seconds");
    check(backoff_for_attempt(2) == 10000, "then ten");
    check(backoff_for_attempt(3) == 20000, "then twenty");
    check(backoff_for_attempt(4) == 40000, "then forty");
    check(backoff_for_attempt(9) == Outbox::kMaximumBackoffMs, "capped at five minutes");
    check(backoff_for_attempt(1000) == Outbox::kMaximumBackoffMs,
          "and the cap holds for absurd inputs rather than overflowing");

    Fixture fixture;
    fixture.enqueue("k1", OperationId::party_create, "p1");
    fixture.claim();

    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.outbox().retry_later(transaction, "k1", "the connection dropped");
    });

    const auto entry = fixture.outbox().get(fixture.store(), "k1");
    check(entry->state == OutboxState::Pending, "a failed send goes back to pending");
    check(entry->due_at == g_now + 5000, "due five seconds from now");
    check(entry->last_error == "the connection dropped", "the reason is kept");

    check(fixture.claim().empty(), "it is not claimed again while it is waiting");

    g_now += 4999;
    check(fixture.claim().empty(), "not even one millisecond early");

    g_now += 1;
    const auto retried = fixture.claim();
    check(retried.size() == 1 && retried[0].attempts == 2,
          "once due, it goes again and the attempt is counted");
}

void test_giving_up() {
    section("giving up");

    Fixture fixture;
    fixture.enqueue("k1", OperationId::party_create, "p1");

    // Ten attempts in, the line is not the problem. Waiting longer only hides
    // it from the person who could fix it.
    for (int attempt = 0; attempt < Outbox::kAttemptsBeforeGivingUp; ++attempt) {
        const auto claimed = fixture.claim();
        check(claimed.size() == 1, "attempt " + std::to_string(attempt + 1) + " was claimed");
        fixture.in_transaction([&fixture](Transaction& transaction) {
            fixture.outbox().retry_later(transaction, "k1", "the server refused");
        });
        g_now += Outbox::kMaximumBackoffMs;
    }

    check(fixture.state_of("k1") == OutboxState::Failed, "after ten attempts it stops retrying");
    check(fixture.claim().empty(), "a failed entry is not retried automatically");
    check(!fixture.outbox().drained(fixture.store()),
          "and the outbox is not considered drained while it sits there");
}

void test_conflict_blocks_one_record_only() {
    section("a conflict");

    Fixture fixture;
    fixture.enqueue("a1", OperationId::invoice_draft_update, "invoice-1");
    fixture.enqueue("a2", OperationId::invoice_draft_update, "invoice-1");
    fixture.enqueue("b1", OperationId::party_update, "party-2");

    fixture.claim();
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.outbox().mark_conflicted(transaction, "a1", "the owner changed it first");
        fixture.outbox().mark_applied(transaction, "b1", 12);
    });

    check(fixture.state_of("a1") == OutboxState::Conflicted, "the entry is marked conflicted");
    check(fixture.claim().empty(),
          "a conflict is never retried automatically; a person decides");
    check(fixture.state_of("a2") == OutboxState::Pending,
          "the next change to that invoice waits behind it");

    // The important half: one stuck record must not stop the shop.
    fixture.enqueue("b2", OperationId::party_update, "party-2");
    const auto claimed = fixture.claim();
    check(claimed.size() == 1 && claimed[0].idempotency_key == "b2",
          "other records keep moving");

    const auto counts = fixture.outbox().counts(fixture.store());
    check(counts.conflicted == 1 && counts.pending == 1 && counts.applied == 1,
          "the counts add up");
    check(counts.unfinished() == 3, "three entries still need something to happen");
}

void test_recovery_after_a_crash() {
    section("recovery after a crash");

    Fixture fixture;
    fixture.enqueue("k1", OperationId::party_create, "p1");
    fixture.enqueue("k2", OperationId::party_create, "p2");
    fixture.claim();

    check(fixture.state_of("k1") == OutboxState::InFlight, "both were in flight");

    // The power went out here. Whether the server saw these is unknowable.
    std::size_t recovered = 0;
    fixture.in_transaction([&fixture, &recovered](Transaction& transaction) {
        recovered = fixture.outbox().recover(transaction);
    });

    check(recovered == 2, "both interrupted entries were recovered");
    check(fixture.state_of("k1") == OutboxState::Pending, "they go back to pending");

    // Sending them again may put a duplicate on the wire, and that is fine:
    // the idempotency key lets the server recognise the second copy. Guessing
    // that they probably arrived is what loses a sale.
    const auto again = fixture.claim();
    check(again.size() == 2, "and they are sent again rather than assumed delivered");
    check(again[0].idempotency_key == "k1",
          "with the same idempotency key, so the server can tell it is not new");
    check(again[0].attempts == 2, "the second attempt is counted");

    std::size_t nothing = 1;
    fixture.in_transaction([&fixture, &nothing](Transaction& transaction) {
        fixture.outbox().mark_applied(transaction, "k1", 1);
        fixture.outbox().mark_applied(transaction, "k2", 2);
        nothing = fixture.outbox().recover(transaction);
    });
    check(nothing == 0, "recovery on a clean start finds nothing to do");
}

void test_pruning() {
    section("pruning");

    Fixture fixture;
    fixture.enqueue("old", OperationId::party_create, "p1");
    fixture.enqueue("new", OperationId::party_create, "p2");
    fixture.enqueue("stuck", OperationId::party_create, "p3");
    fixture.claim();

    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.outbox().mark_applied(transaction, "old", 1);
        fixture.outbox().mark_conflicted(transaction, "stuck", "someone else changed it");
    });

    const std::int64_t cutoff = g_now + 1;
    g_now += 10'000;
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.outbox().mark_applied(transaction, "new", 2);
    });

    std::size_t pruned = 0;
    fixture.in_transaction([&fixture, &pruned, cutoff](Transaction& transaction) {
        pruned = fixture.outbox().prune_applied(transaction, cutoff);
    });

    check(pruned == 1, "only the older applied entry was pruned");
    check(!fixture.outbox().get(fixture.store(), "old").has_value(), "it is gone");
    check(fixture.outbox().get(fixture.store(), "new").has_value(),
          "the recent one is kept");
    check(fixture.outbox().get(fixture.store(), "stuck").has_value(),
          "and nothing unfinished is ever pruned, however old");

    check(fixture.outbox().for_record(fixture.store(), "p3").size() == 1,
          "entries can be found by the record they touch");
    check(fixture.outbox().in_state(fixture.store(), OutboxState::Conflicted).size() == 1,
          "and by state, for the screen that shows what needs attention");
}

}  // namespace

int main() {
    test_what_may_be_queued();
    test_enqueuing_is_itself_idempotent();
    test_claiming_a_batch();
    test_one_record_is_sent_in_order();
    test_the_ordinary_path();
    test_backoff();
    test_giving_up();
    test_conflict_blocks_one_record_only();
    test_recovery_after_a_crash();
    test_pruning();
    return report();
}
