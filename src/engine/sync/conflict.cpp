#include "engine/sync/conflict.hpp"

#include <string>
#include <utility>

namespace squiflow::engine {
namespace {

const std::string kTable = "superseded_version";
const std::string kId = "id";
const std::string kModule = "module";
const std::string kRecord = "record_id";
const std::string kOperation = "operation";
const std::string kPayload = "payload";
const std::string kReason = "reason";
const std::string kRecordedAt = "recorded_at";
const std::string kSeen = "seen_by_a_person";

Row to_row(const SupersededVersion& version) {
    Row row;
    row.set(kId, Value::text(version.id));
    row.set(kModule, Value::integer(static_cast<std::int64_t>(version.module)));
    row.set(kRecord, Value::text(version.record_id));
    row.set(kOperation, Value::integer(static_cast<std::int64_t>(version.operation)));
    row.set(kPayload, Value::binary(version.payload));
    row.set(kReason, Value::text(version.reason));
    row.set(kRecordedAt, Value::integer(version.recorded_at));
    row.set(kSeen, Value::boolean(version.seen_by_a_person));
    return row;
}

SupersededVersion from_row(const Row& row) {
    SupersededVersion version;
    version.id = row.get(kId).text_or("");
    version.module = static_cast<protocol::ModuleId>(row.get(kModule).integer_or(0));
    version.record_id = row.get(kRecord).text_or("");
    version.operation =
        static_cast<protocol::OperationId>(row.get(kOperation).integer_or(0));
    if (const Blob* payload = row.get(kPayload).as_binary()) {
        version.payload = *payload;
    }
    version.reason = row.get(kReason).text_or("");
    version.recorded_at = row.get(kRecordedAt).integer_or(0);
    version.seen_by_a_person = row.get(kSeen).boolean_or(false);
    return version;
}

}  // namespace

std::string_view to_string(ConflictOutcome outcome) noexcept {
    switch (outcome) {
        case ConflictOutcome::RemoteWins:
            return "the server's version stands";
        case ConflictOutcome::LocalWins:
            return "this device's version stands";
        case ConflictOutcome::NeedsAPerson:
            return "a person has to decide";
    }
    return "unknown";
}

ConflictDecision decide_conflict(const ConflictFacts& facts) noexcept {
    if (facts.record_is_final) {
        // An issued invoice or quotation is not something either side may
        // overwrite. The shop's rule for a wrong document is cancel and
        // reissue, performed by a person, not a merge performed by a program.
        return {ConflictOutcome::NeedsAPerson,
                "the record has been issued; correction is cancel and reissue"};
    }

    if (facts.local_change_by_owner && !facts.remote_change_by_owner) {
        return {ConflictOutcome::LocalWins, "the owner made the change on this device"};
    }

    if (facts.remote_change_by_owner && !facts.local_change_by_owner) {
        return {ConflictOutcome::RemoteWins, "the owner made the change on the other device"};
    }

    // Same authority on both sides: two staff changes, or the owner on two
    // devices. Nothing distinguishes them except the clocks, and two machines
    // never agree on the time, so the system of record decides.
    return {ConflictOutcome::RemoteWins, "the server holds the version of record"};
}

const std::string& ConflictLog::table_name() {
    return kTable;
}

void ConflictLog::define(Store& store) {
    store.define_table(kTable, kId);
}

ConflictLog::ConflictLog(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) {
        throw StoreError("the conflict log needs a clock");
    }
}

void ConflictLog::retain(Transaction& transaction, const OutboxEntry& entry,
                         const std::string& reason) const {
    SupersededVersion version;
    // Keyed by the idempotency key: the losing change is identified by exactly
    // the same name the server knows it by, and retaining twice cannot make
    // two copies.
    version.id = entry.idempotency_key;
    version.module = entry.module();
    version.record_id = entry.record_id;
    version.operation = entry.operation;
    version.payload = entry.payload;
    version.reason = reason;
    version.recorded_at = clock_();
    version.seen_by_a_person = false;

    if (transaction.find(kTable, version.id)) {
        transaction.replace(kTable, version.id, to_row(version));
    } else {
        transaction.insert(kTable, to_row(version));
    }
}

ConflictDecision ConflictLog::resolve(Transaction& transaction, const Outbox& outbox,
                                      const OutboxEntry& entry,
                                      const ConflictFacts& facts) const {
    if (entry.record_id != facts.record_id) {
        throw StoreError("the conflict is about a different record than the entry");
    }

    const ConflictDecision decision = decide_conflict(facts);

    switch (decision.outcome) {
        case ConflictOutcome::RemoteWins:
            // The local change is not applied, but it is not lost either. It
            // is kept in full, and the person who made it is told.
            retain(transaction, entry, decision.reason);
            outbox.discard(transaction, entry.idempotency_key);
            break;

        case ConflictOutcome::LocalWins:
            // Nothing is lost on this side, so nothing is retained. The change
            // goes again, on top of what the server now holds.
            outbox.resend(transaction, entry.idempotency_key, decision.reason);
            break;

        case ConflictOutcome::NeedsAPerson:
            // Kept, and the record stays blocked behind it, because sending
            // anything further about that record would be building on a
            // question nobody has answered.
            retain(transaction, entry, decision.reason);
            outbox.mark_conflicted(transaction, entry.idempotency_key, decision.reason);
            break;
    }

    return decision;
}

void ConflictLog::mark_seen(Transaction& transaction, const std::string& id) const {
    const auto row = transaction.find(kTable, id);
    if (!row) {
        throw StoreError("no superseded version with id '" + id + "'");
    }
    SupersededVersion version = from_row(*row);
    version.seen_by_a_person = true;
    // Marking it seen does not delete it. The kept version stays readable for
    // as long as the record does.
    transaction.replace(kTable, id, to_row(version));
}

std::vector<SupersededVersion> ConflictLog::for_record(const Store& store,
                                                       const std::string& record_id) const {
    Query query{kTable};
    query.where_equals(kRecord, Value::text(record_id)).order_by(kRecordedAt);
    std::vector<SupersededVersion> versions;
    for (const auto& row : store.select(query)) {
        versions.push_back(from_row(row));
    }
    return versions;
}

std::vector<SupersededVersion> ConflictLog::needing_attention(const Store& store) const {
    Query query{kTable};
    query.where_equals(kSeen, Value::boolean(false)).order_by(kRecordedAt);
    std::vector<SupersededVersion> versions;
    for (const auto& row : store.select(query)) {
        versions.push_back(from_row(row));
    }
    return versions;
}

std::size_t ConflictLog::count(const Store& store) const {
    return store.count(kTable);
}

}  // namespace squiflow::engine
