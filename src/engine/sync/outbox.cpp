#include "engine/sync/outbox.hpp"

#include <algorithm>
#include <set>
#include <utility>

namespace squiflow::engine {
namespace {

const std::string kTable = "outbox";
const std::string kKey = "idempotency_key";
const std::string kOperation = "operation";
const std::string kModule = "module";
const std::string kRecord = "record_id";
const std::string kPayload = "payload";
const std::string kState = "state";
const std::string kPosition = "position";
const std::string kCreatedAt = "created_at";
const std::string kUpdatedAt = "updated_at";
const std::string kDueAt = "due_at";
const std::string kAttempts = "attempts";
const std::string kSequence = "server_sequence";
const std::string kLastError = "last_error";

Row to_row(const OutboxEntry& entry) {
    Row row;
    row.set(kKey, Value::text(entry.idempotency_key));
    row.set(kOperation, Value::integer(static_cast<std::int64_t>(entry.operation)));
    row.set(kModule, Value::integer(static_cast<std::int64_t>(entry.module())));
    row.set(kRecord, Value::text(entry.record_id));
    row.set(kPayload, Value::binary(entry.payload));
    row.set(kState, Value::integer(static_cast<std::int64_t>(entry.state)));
    row.set(kPosition, Value::integer(entry.position));
    row.set(kCreatedAt, Value::integer(entry.created_at));
    row.set(kUpdatedAt, Value::integer(entry.updated_at));
    row.set(kDueAt, Value::integer(entry.due_at));
    row.set(kAttempts, Value::integer(entry.attempts));
    row.set(kSequence, Value::integer(entry.server_sequence));
    row.set(kLastError, Value::text(entry.last_error));
    return row;
}

OutboxEntry from_row(const Row& row) {
    OutboxEntry entry;
    entry.idempotency_key = row.get(kKey).text_or("");
    entry.operation =
        static_cast<protocol::OperationId>(row.get(kOperation).integer_or(0));
    entry.record_id = row.get(kRecord).text_or("");
    if (const Blob* payload = row.get(kPayload).as_binary()) {
        entry.payload = *payload;
    }
    entry.state = static_cast<OutboxState>(row.get(kState).integer_or(0));
    entry.position = row.get(kPosition).integer_or(0);
    entry.created_at = row.get(kCreatedAt).integer_or(0);
    entry.updated_at = row.get(kUpdatedAt).integer_or(0);
    entry.due_at = row.get(kDueAt).integer_or(0);
    entry.attempts = static_cast<std::int32_t>(row.get(kAttempts).integer_or(0));
    entry.server_sequence = row.get(kSequence).integer_or(0);
    entry.last_error = row.get(kLastError).text_or("");
    return entry;
}

bool blocks_the_record(OutboxState state) noexcept {
    // Anything that has not finished holds the place of everything queued
    // behind it for the same record. A payment must not overtake the invoice
    // it pays.
    switch (state) {
        case OutboxState::Applied:
            return false;
        case OutboxState::Pending:
        case OutboxState::InFlight:
        case OutboxState::Acknowledged:
        case OutboxState::Conflicted:
        case OutboxState::Failed:
            return true;
    }
    return true;
}

}  // namespace

std::string_view to_string(OutboxState state) noexcept {
    switch (state) {
        case OutboxState::Pending:
            return "pending";
        case OutboxState::InFlight:
            return "in flight";
        case OutboxState::Acknowledged:
            return "acknowledged";
        case OutboxState::Applied:
            return "applied";
        case OutboxState::Conflicted:
            return "conflicted";
        case OutboxState::Failed:
            return "failed";
    }
    return "unknown";
}

protocol::ModuleId OutboxEntry::module() const noexcept {
    return protocol::operation(operation).module;
}

std::size_t OutboxCounts::total() const noexcept {
    return pending + in_flight + acknowledged + applied + conflicted + failed;
}

std::size_t OutboxCounts::unfinished() const noexcept {
    return pending + in_flight + acknowledged + conflicted + failed;
}

std::int64_t backoff_for_attempt(std::int32_t attempts) noexcept {
    if (attempts <= 1) {
        return Outbox::kFirstBackoffMs;
    }
    // Doubling, but never shifting far enough to overflow.
    if (attempts > 20) {
        return Outbox::kMaximumBackoffMs;
    }
    const std::int64_t doubled = Outbox::kFirstBackoffMs << (attempts - 1);
    return std::min(doubled, Outbox::kMaximumBackoffMs);
}

const std::string& Outbox::table_name() {
    return kTable;
}

void Outbox::define(Store& store) {
    store.define_table(kTable, kKey);
}

Outbox::Outbox(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) {
        throw StoreError("the outbox needs a clock");
    }
}

std::int64_t Outbox::next_position(const Transaction& transaction) const {
    Query query{kTable};
    query.order_by(kPosition, SortOrder::Descending).take(1);
    const auto rows = transaction.select(query);
    if (rows.empty()) {
        return 1;
    }
    return rows.front().get(kPosition).integer_or(0) + 1;
}

EnqueueResult Outbox::enqueue(Transaction& transaction, const OutboxEntry& entry) const {
    if (entry.idempotency_key.empty()) {
        // Without a key the server cannot recognise a retry, and a retry is
        // the normal case rather than the exception.
        throw StoreError("an outbox entry needs an idempotency key");
    }
    if (entry.record_id.empty()) {
        throw StoreError("an outbox entry needs a record to point at");
    }

    const auto& info = protocol::operation(entry.operation);
    if (info.sync_class != protocol::OperationClass::Synchronizable) {
        // A local-only operation has nothing to send, and an online-required
        // one was never allowed to happen without a connection. Either in the
        // outbox is a bug upstream, and a silent one.
        throw StoreError(std::string{"operation '"} + std::string{info.name} +
                         "' is " + std::string{protocol::to_string(info.sync_class)} +
                         " and does not belong in the outbox");
    }

    if (transaction.find(kTable, entry.idempotency_key)) {
        // Enqueuing is itself idempotent. A caller retrying its own write is
        // not an error, and must not produce a second copy.
        return EnqueueResult::AlreadyQueued;
    }

    const std::int64_t now = clock_();

    OutboxEntry stored = entry;
    stored.state = OutboxState::Pending;
    stored.position = next_position(transaction);
    stored.created_at = now;
    stored.updated_at = now;
    stored.due_at = now;
    stored.attempts = 0;
    stored.server_sequence = 0;
    stored.last_error.clear();

    transaction.insert(kTable, to_row(stored));
    return EnqueueResult::Enqueued;
}

OutboxEntry Outbox::require(const Transaction& transaction, const std::string& key) const {
    const auto row = transaction.find(kTable, key);
    if (!row) {
        throw StoreError("no outbox entry with key '" + key + "'");
    }
    return from_row(*row);
}

void Outbox::store_state(Transaction& transaction, OutboxEntry& entry,
                         OutboxState state) const {
    entry.state = state;
    entry.updated_at = clock_();
    transaction.replace(kTable, entry.idempotency_key, to_row(entry));
}

std::vector<OutboxEntry> Outbox::claim(Transaction& transaction, std::size_t batch) const {
    if (batch < kMinimumBatch || batch > kMaximumBatch) {
        throw StoreError("an outbox batch must be between " + std::to_string(kMinimumBatch) +
                         " and " + std::to_string(kMaximumBatch) + ", not " +
                         std::to_string(batch));
    }

    const std::int64_t now = clock_();

    Query query{kTable};
    query.order_by(kPosition);
    const auto rows = transaction.select(query);

    std::set<std::string> blocked;
    std::vector<OutboxEntry> claimed;

    for (const auto& row : rows) {
        OutboxEntry entry = from_row(row);

        if (blocked.count(entry.record_id) > 0) {
            continue;
        }

        if (entry.state != OutboxState::Pending) {
            if (blocks_the_record(entry.state)) {
                blocked.insert(entry.record_id);
            }
            continue;
        }

        if (entry.due_at > now) {
            // Waiting out its backoff. It still holds the place of everything
            // behind it for the same record.
            blocked.insert(entry.record_id);
            continue;
        }

        if (claimed.size() >= batch) {
            break;
        }

        entry.attempts += 1;
        store_state(transaction, entry, OutboxState::InFlight);
        claimed.push_back(entry);

        // One per record per batch: the next change to the same record cannot
        // be sent until this one has landed.
        blocked.insert(entry.record_id);
    }

    return claimed;
}

void Outbox::acknowledge(Transaction& transaction, const std::string& key) const {
    OutboxEntry entry = require(transaction, key);
    if (entry.state != OutboxState::InFlight) {
        throw StoreError("only an entry in flight can be acknowledged; '" + key + "' is " +
                         std::string{to_string(entry.state)});
    }
    store_state(transaction, entry, OutboxState::Acknowledged);
}

void Outbox::mark_applied(Transaction& transaction, const std::string& key,
                          std::int64_t server_sequence) const {
    if (server_sequence <= 0) {
        // The sequence is what later pulls are measured against. A zero here
        // would quietly reset the cursor.
        throw StoreError("an applied entry needs a server sequence");
    }
    OutboxEntry entry = require(transaction, key);
    if (entry.state != OutboxState::InFlight && entry.state != OutboxState::Acknowledged) {
        throw StoreError("'" + key + "' cannot be applied from " +
                         std::string{to_string(entry.state)});
    }
    entry.server_sequence = server_sequence;
    entry.last_error.clear();
    store_state(transaction, entry, OutboxState::Applied);
}

void Outbox::retry_later(Transaction& transaction, const std::string& key,
                         const std::string& error) const {
    OutboxEntry entry = require(transaction, key);
    if (entry.state != OutboxState::InFlight) {
        throw StoreError("only an entry in flight can be retried; '" + key + "' is " +
                         std::string{to_string(entry.state)});
    }
    entry.last_error = error;

    if (entry.attempts >= kAttemptsBeforeGivingUp) {
        // Ten attempts in, the line is not the problem. Waiting longer only
        // hides it.
        store_state(transaction, entry, OutboxState::Failed);
        return;
    }

    entry.due_at = clock_() + backoff_for_attempt(entry.attempts);
    store_state(transaction, entry, OutboxState::Pending);
}

void Outbox::mark_conflicted(Transaction& transaction, const std::string& key,
                             const std::string& reason) const {
    OutboxEntry entry = require(transaction, key);
    if (entry.state != OutboxState::InFlight && entry.state != OutboxState::Acknowledged) {
        throw StoreError("'" + key + "' cannot conflict from " +
                         std::string{to_string(entry.state)});
    }
    entry.last_error = reason;
    store_state(transaction, entry, OutboxState::Conflicted);
}

void Outbox::mark_failed(Transaction& transaction, const std::string& key,
                         const std::string& error) const {
    OutboxEntry entry = require(transaction, key);
    if (entry.state == OutboxState::Applied) {
        throw StoreError("'" + key + "' has already been applied");
    }
    entry.last_error = error;
    store_state(transaction, entry, OutboxState::Failed);
}

void Outbox::resend(Transaction& transaction, const std::string& key,
                    const std::string& reason) const {
    OutboxEntry entry = require(transaction, key);
    if (entry.state == OutboxState::Applied) {
        throw StoreError("'" + key + "' has already been applied");
    }
    entry.last_error = reason;
    entry.due_at = clock_();
    store_state(transaction, entry, OutboxState::Pending);
}

void Outbox::discard(Transaction& transaction, const std::string& key) const {
    const OutboxEntry entry = require(transaction, key);
    if (entry.state == OutboxState::Pending && entry.attempts == 0) {
        // It has never been sent, so nothing else can have dealt with it.
        // Dropping it here would lose the change with no trace at all.
        throw StoreError("'" + key + "' has never been sent and cannot be discarded");
    }
    transaction.remove(kTable, key);
}

std::size_t Outbox::recover(Transaction& transaction) const {
    Query query{kTable};
    query.where_equals(kState, Value::integer(static_cast<std::int64_t>(OutboxState::InFlight)))
        .order_by(kPosition);
    const auto rows = transaction.select(query);

    const std::int64_t now = clock_();
    for (const auto& row : rows) {
        OutboxEntry entry = from_row(row);
        // Sending it again may duplicate it on the wire, and that is fine:
        // the idempotency key means the server recognises the second copy.
        // Guessing that it probably arrived is what loses a sale.
        entry.due_at = now;
        entry.last_error = "interrupted before the result was known";
        store_state(transaction, entry, OutboxState::Pending);
    }
    return rows.size();
}

std::size_t Outbox::prune_applied(Transaction& transaction, std::int64_t applied_before) const {
    Query query{kTable};
    query.where_equals(kState, Value::integer(static_cast<std::int64_t>(OutboxState::Applied)))
        .where(kUpdatedAt, Comparison::Less, Value::integer(applied_before));
    const auto rows = transaction.select(query);
    for (const auto& row : rows) {
        transaction.remove(kTable, row.get(kKey).text_or(""));
    }
    return rows.size();
}

std::optional<OutboxEntry> Outbox::get(const Store& store, const std::string& key) const {
    const auto row = store.find(kTable, key);
    if (!row) {
        return std::nullopt;
    }
    return from_row(*row);
}

std::vector<OutboxEntry> Outbox::in_state(const Store& store, OutboxState state) const {
    Query query{kTable};
    query.where_equals(kState, Value::integer(static_cast<std::int64_t>(state)))
        .order_by(kPosition);
    std::vector<OutboxEntry> entries;
    for (const auto& row : store.select(query)) {
        entries.push_back(from_row(row));
    }
    return entries;
}

std::vector<OutboxEntry> Outbox::for_record(const Store& store,
                                            const std::string& record_id) const {
    Query query{kTable};
    query.where_equals(kRecord, Value::text(record_id)).order_by(kPosition);
    std::vector<OutboxEntry> entries;
    for (const auto& row : store.select(query)) {
        entries.push_back(from_row(row));
    }
    return entries;
}

OutboxCounts Outbox::counts(const Store& store) const {
    OutboxCounts totals;
    Query query{kTable};
    for (const auto& row : store.select(query)) {
        switch (static_cast<OutboxState>(row.get(kState).integer_or(0))) {
            case OutboxState::Pending:
                ++totals.pending;
                break;
            case OutboxState::InFlight:
                ++totals.in_flight;
                break;
            case OutboxState::Acknowledged:
                ++totals.acknowledged;
                break;
            case OutboxState::Applied:
                ++totals.applied;
                break;
            case OutboxState::Conflicted:
                ++totals.conflicted;
                break;
            case OutboxState::Failed:
                ++totals.failed;
                break;
        }
    }
    return totals;
}

bool Outbox::drained(const Store& store) const {
    return counts(store).unfinished() == 0;
}

}  // namespace squiflow::engine
