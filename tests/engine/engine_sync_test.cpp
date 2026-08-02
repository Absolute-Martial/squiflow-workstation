// Phase 3.6: the sync cursor and the conflict rule.
//
// The cursor half is about never reading the same batch twice and never
// skipping one. The conflict half is about a decision the shopkeeper already
// made - the owner's version wins, and the losing version is kept - being
// written down once, in a function with no database in it, rather than
// re-argued on every screen.

#include <cstdint>
#include <string>
#include <vector>

#include <squiflow/protocol/module_id.hpp>
#include <squiflow/protocol/operation_table.hpp>

#include "engine/storage/memory_store.hpp"
#include "engine/storage/store.hpp"
#include "engine/sync/conflict.hpp"
#include "engine/sync/cursor.hpp"
#include "engine/sync/outbox.hpp"
#include "support/check.hpp"

namespace {

using squiflow::engine::Blob;
using squiflow::engine::ConflictFacts;
using squiflow::engine::ConflictLog;
using squiflow::engine::ConflictOutcome;
using squiflow::engine::Cursor;
using squiflow::engine::MemoryStore;
using squiflow::engine::Outbox;
using squiflow::engine::OutboxEntry;
using squiflow::engine::OutboxState;
using squiflow::engine::StoreError;
using squiflow::engine::Transaction;
using squiflow::engine::decide_conflict;
using squiflow::protocol::ModuleId;
using squiflow::protocol::OperationId;
using squiflow::testing::check;
using squiflow::testing::report;
using squiflow::testing::section;

std::int64_t g_now = 2'000'000;

std::int64_t fake_clock() {
    return g_now;
}

template <typename Callable>
bool refuses(Callable&& callable) {
    try {
        callable();
    } catch (const StoreError&) {
        return true;
    }
    return false;
}

class Fixture {
public:
    Fixture() : cursor_(fake_clock), outbox_(fake_clock), conflicts_(fake_clock) {
        g_now = 2'000'000;
        Cursor::define(store_);
        Outbox::define(store_);
        ConflictLog::define(store_);
    }

    MemoryStore& store() {
        return store_;
    }
    const Cursor& cursor() const {
        return cursor_;
    }
    const Outbox& outbox() const {
        return outbox_;
    }
    const ConflictLog& conflicts() const {
        return conflicts_;
    }

    template <typename Callable>
    void in_transaction(Callable&& callable) {
        auto transaction = store_.begin();
        callable(*transaction);
        transaction->commit();
    }

    // Queues a change and takes it as far as "in flight", which is the only
    // state a conflict can arrive in.
    OutboxEntry sent(const std::string& key, OperationId operation,
                     const std::string& record) {
        OutboxEntry entry;
        entry.idempotency_key = key;
        entry.operation = operation;
        entry.record_id = record;
        const std::string text = "payload for " + key;
        entry.payload = Blob{text.begin(), text.end()};

        std::vector<OutboxEntry> claimed;
        in_transaction([&](Transaction& transaction) {
            outbox_.enqueue(transaction, entry);
            claimed = outbox_.claim(transaction, 1);
        });
        return claimed.front();
    }

private:
    MemoryStore store_;
    Cursor cursor_;
    Outbox outbox_;
    ConflictLog conflicts_;
};

void test_a_new_device() {
    section("a new device");

    Fixture fixture;
    const auto position = fixture.cursor().position(fixture.store(), ModuleId::parties);
    check(position.sequence == 0, "a cursor that was never written reads as zero");
    check(position.never_pulled(), "which is how a new device is recognised");
    check(position.module == ModuleId::parties, "and it knows which module it belongs to");

    check(fixture.cursor().never_pulled(fixture.store()).size() ==
              squiflow::protocol::kModuleCount,
          "every module needs a first full pull");
    check(fixture.cursor().all(fixture.store()).size() == squiflow::protocol::kModuleCount,
          "one cursor per module, whether or not it has ever been written");
}

void test_moving_forward() {
    section("moving forward");

    Fixture fixture;
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.cursor().advance(transaction, ModuleId::parties, 40);
    });

    auto position = fixture.cursor().position(fixture.store(), ModuleId::parties);
    check(position.sequence == 40, "the cursor moved");
    check(position.last_success_at == g_now, "and remembers when it last succeeded");
    check(!position.never_pulled(), "it is no longer a new module");

    check(fixture.cursor().position(fixture.store(), ModuleId::catalog).sequence == 0,
          "one module's cursor does not move another's");

    // A gap is normal: the server assigns sequences shop-wide, so a parties
    // pull sees 40 then 57.
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.cursor().advance(transaction, ModuleId::parties, 57);
    });
    check(fixture.cursor().position(fixture.store(), ModuleId::parties).sequence == 57,
          "a gap in the numbers is not an error");

    // Standing still is not an error either: an empty batch means there was
    // nothing new.
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.cursor().advance(transaction, ModuleId::parties, 57);
    });
    check(fixture.cursor().position(fixture.store(), ModuleId::parties).sequence == 57,
          "an empty batch leaves it where it was");

    // Going backwards would pull and re-apply the same batch on every cycle
    // from now on, forever.
    check(refuses([&fixture] {
              fixture.in_transaction([&fixture](Transaction& transaction) {
                  fixture.cursor().advance(transaction, ModuleId::parties, 56);
              });
          }),
          "a cursor is never allowed to move backwards");
    check(fixture.cursor().position(fixture.store(), ModuleId::parties).sequence == 57,
          "and the refusal left it untouched");
}

void test_a_failed_pull() {
    section("a failed pull");

    Fixture fixture;
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.cursor().advance(transaction, ModuleId::orders, 12);
        fixture.cursor().record_failure(transaction, ModuleId::orders, "the line dropped");
        fixture.cursor().record_failure(transaction, ModuleId::orders, "the line dropped");
    });

    auto position = fixture.cursor().position(fixture.store(), ModuleId::orders);
    check(position.sequence == 12, "a failed pull does not move the cursor");
    check(position.consecutive_failures == 2, "but the failures are counted");
    check(position.last_error == "the line dropped", "with the reason kept for the screen");

    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.cursor().advance(transaction, ModuleId::orders, 13);
    });
    position = fixture.cursor().position(fixture.store(), ModuleId::orders);
    check(position.consecutive_failures == 0, "a success clears the run of failures");
    check(position.last_error.empty(), "and clears the message with it");
}

void test_a_full_pull() {
    section("asking for everything again");

    Fixture fixture;
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.cursor().advance(transaction, ModuleId::catalog, 900);
        fixture.cursor().reset(transaction, ModuleId::catalog);
    });

    const auto position = fixture.cursor().position(fixture.store(), ModuleId::catalog);
    check(position.sequence == 0, "a reset device asks for everything again");
    check(position.never_pulled(), "and looks like a new one, which is exactly what it is");

    // Having reset, moving forward from zero must be allowed - it is not a
    // backwards move.
    fixture.in_transaction([&fixture](Transaction& transaction) {
        fixture.cursor().advance(transaction, ModuleId::catalog, 5);
    });
    check(fixture.cursor().position(fixture.store(), ModuleId::catalog).sequence == 5,
          "and the full pull can then move it forward again");
}

void test_the_rule() {
    section("the rule itself");

    ConflictFacts facts;
    facts.module = ModuleId::parties;
    facts.record_id = "party-1";

    facts.local_change_by_owner = true;
    facts.remote_change_by_owner = false;
    check(decide_conflict(facts).outcome == ConflictOutcome::LocalWins,
          "the owner's change on this device beats the staff device's");

    facts.local_change_by_owner = false;
    facts.remote_change_by_owner = true;
    check(decide_conflict(facts).outcome == ConflictOutcome::RemoteWins,
          "and the owner's change elsewhere beats this device's");

    facts.local_change_by_owner = false;
    facts.remote_change_by_owner = false;
    check(decide_conflict(facts).outcome == ConflictOutcome::RemoteWins,
          "with no owner involved, the server holds the version of record");

    facts.local_change_by_owner = true;
    facts.remote_change_by_owner = true;
    check(decide_conflict(facts).outcome == ConflictOutcome::RemoteWins,
          "and the owner on two devices is settled the same way, not by comparing clocks");

    // The one case the program is not allowed to settle at all.
    facts.record_is_final = true;
    facts.local_change_by_owner = true;
    facts.remote_change_by_owner = false;
    check(decide_conflict(facts).outcome == ConflictOutcome::NeedsAPerson,
          "an issued document is never overwritten automatically, even for the owner");
    check(!decide_conflict(facts).reason.empty(),
          "and every decision carries a reason a person can read");
}

void test_the_server_version_stands() {
    section("the server's version stands");

    Fixture fixture;
    const auto entry = fixture.sent("c1", OperationId::party_update, "party-1");
    fixture.sent("c2", OperationId::party_update, "party-2");

    ConflictFacts facts;
    facts.module = ModuleId::parties;
    facts.record_id = "party-1";
    facts.local_change_by_owner = false;
    facts.remote_change_by_owner = true;
    facts.remote_sequence = 88;

    squiflow::engine::ConflictDecision decision;
    fixture.in_transaction([&](Transaction& transaction) {
        decision = fixture.conflicts().resolve(transaction, fixture.outbox(), entry, facts);
    });

    check(decision.outcome == ConflictOutcome::RemoteWins, "the remote change won");
    check(!fixture.outbox().get(fixture.store(), "c1").has_value(),
          "the local change is no longer queued");

    // The half that matters: nothing was thrown away.
    const auto kept = fixture.conflicts().for_record(fixture.store(), "party-1");
    check(kept.size() == 1, "the losing version was kept");
    check(kept[0].payload == entry.payload, "in full, exactly as it was written");
    check(kept[0].operation == OperationId::party_update, "with what it was trying to do");
    check(kept[0].reason == decision.reason, "and why it lost");
    check(!kept[0].seen_by_a_person, "waiting for someone to look at it");

    // And the record it touched is free again, so later work on that party is
    // not stuck behind a change that has already been settled.
    fixture.sent("c3", OperationId::party_update, "party-1");
    check(fixture.outbox().get(fixture.store(), "c3")->state == OutboxState::InFlight,
          "the record is no longer blocked");
}

void test_this_device_version_stands() {
    section("this device's version stands");

    Fixture fixture;
    const auto entry = fixture.sent("c1", OperationId::party_update, "party-1");

    ConflictFacts facts;
    facts.module = ModuleId::parties;
    facts.record_id = "party-1";
    facts.local_change_by_owner = true;
    facts.remote_change_by_owner = false;

    fixture.in_transaction([&](Transaction& transaction) {
        const auto decision =
            fixture.conflicts().resolve(transaction, fixture.outbox(), entry, facts);
        check(decision.outcome == ConflictOutcome::LocalWins, "the local change won");
    });

    const auto queued = fixture.outbox().get(fixture.store(), "c1");
    check(queued->state == OutboxState::Pending, "it is queued again");
    check(queued->due_at == g_now, "to go immediately rather than after a backoff");
    check(queued->attempts == 1,
          "and the conflict is not counted against the give-up limit; the line was fine");
    check(queued->idempotency_key == "c1",
          "with the same key, so the server still recognises it as one change");
    check(fixture.conflicts().count(fixture.store()) == 0,
          "nothing was superseded on this side, so nothing was kept");
}

void test_a_person_has_to_decide() {
    section("a person has to decide");

    Fixture fixture;
    const auto entry = fixture.sent("c1", OperationId::invoice_draft_update, "invoice-7");

    ConflictFacts facts;
    facts.module = ModuleId::receivables;
    facts.record_id = "invoice-7";
    facts.local_change_by_owner = true;
    facts.record_is_final = true;

    fixture.in_transaction([&](Transaction& transaction) {
        const auto decision =
            fixture.conflicts().resolve(transaction, fixture.outbox(), entry, facts);
        check(decision.outcome == ConflictOutcome::NeedsAPerson, "it was not settled");
    });

    check(fixture.outbox().get(fixture.store(), "c1")->state == OutboxState::Conflicted,
          "the entry sits in the outbox as conflicted");

    const auto waiting = fixture.conflicts().needing_attention(fixture.store());
    check(waiting.size() == 1, "and it appears on the list a person works through");

    fixture.in_transaction([&fixture, &waiting](Transaction& transaction) {
        fixture.conflicts().mark_seen(transaction, waiting[0].id);
    });
    check(fixture.conflicts().needing_attention(fixture.store()).empty(),
          "once seen it leaves the list");
    check(fixture.conflicts().count(fixture.store()) == 1,
          "but it is still kept; seeing it is not deleting it");

    check(refuses([&fixture] {
              fixture.in_transaction([&fixture](Transaction& transaction) {
                  fixture.conflicts().mark_seen(transaction, "nothing-like-this");
              });
          }),
          "marking an unknown one seen is refused rather than ignored");
}

void test_what_resolution_refuses() {
    section("what resolution refuses");

    Fixture fixture;
    const auto entry = fixture.sent("c1", OperationId::party_update, "party-1");

    ConflictFacts facts;
    facts.module = ModuleId::parties;
    facts.record_id = "a-different-party";

    check(refuses([&] {
              fixture.in_transaction([&](Transaction& transaction) {
                  fixture.conflicts().resolve(transaction, fixture.outbox(), entry, facts);
              });
          }),
          "a conflict about another record cannot be applied to this entry");

    // Discarding is only for a change that has been dealt with some other
    // way. A change that has never left the device has not been dealt with by
    // anyone.
    fixture.in_transaction([&fixture](Transaction& transaction) {
        OutboxEntry fresh;
        fresh.idempotency_key = "never-sent";
        fresh.operation = OperationId::party_create;
        fresh.record_id = "party-3";
        fixture.outbox().enqueue(transaction, fresh);
    });
    check(refuses([&fixture] {
              fixture.in_transaction([&fixture](Transaction& transaction) {
                  fixture.outbox().discard(transaction, "never-sent");
              });
          }),
          "an unsent change cannot be quietly discarded");
    check(fixture.outbox().get(fixture.store(), "never-sent").has_value(),
          "and it is still there afterwards");
}

}  // namespace

int main() {
    test_a_new_device();
    test_moving_forward();
    test_a_failed_pull();
    test_a_full_pull();
    test_the_rule();
    test_the_server_version_stands();
    test_this_device_version_stands();
    test_a_person_has_to_decide();
    test_what_resolution_refuses();
    return report();
}
