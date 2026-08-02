#include "engine/sync/cursor.hpp"

#include <string>
#include <utility>

namespace squiflow::engine {
namespace {

const std::string kTable = "sync_cursor";
const std::string kModule = "module";
const std::string kSequence = "sequence";
const std::string kLastAttempt = "last_attempt_at";
const std::string kLastSuccess = "last_success_at";
const std::string kFailures = "consecutive_failures";
const std::string kLastError = "last_error";

std::string key_for(protocol::ModuleId module) {
    return std::to_string(static_cast<int>(module));
}

Row to_row(const CursorPosition& position) {
    Row row;
    row.set(kModule, Value::text(key_for(position.module)));
    row.set(kSequence, Value::integer(position.sequence));
    row.set(kLastAttempt, Value::integer(position.last_attempt_at));
    row.set(kLastSuccess, Value::integer(position.last_success_at));
    row.set(kFailures, Value::integer(position.consecutive_failures));
    row.set(kLastError, Value::text(position.last_error));
    return row;
}

CursorPosition from_row(const Row& row) {
    CursorPosition position;
    position.module =
        static_cast<protocol::ModuleId>(std::stoi(row.get(kModule).text_or("0")));
    position.sequence = row.get(kSequence).integer_or(0);
    position.last_attempt_at = row.get(kLastAttempt).integer_or(0);
    position.last_success_at = row.get(kLastSuccess).integer_or(0);
    position.consecutive_failures =
        static_cast<std::int32_t>(row.get(kFailures).integer_or(0));
    position.last_error = row.get(kLastError).text_or("");
    return position;
}

void require_known(protocol::ModuleId module) {
    if (static_cast<std::size_t>(module) >= protocol::kModuleCount) {
        throw StoreError("no such module");
    }
}

}  // namespace

bool CursorPosition::never_pulled() const noexcept {
    return sequence == 0;
}

const std::string& Cursor::table_name() {
    return kTable;
}

void Cursor::define(Store& store) {
    store.define_table(kTable, kModule);
}

Cursor::Cursor(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) {
        throw StoreError("the cursor needs a clock");
    }
}

CursorPosition Cursor::position(const Store& store, protocol::ModuleId module) const {
    require_known(module);
    const auto row = store.find(kTable, key_for(module));
    if (!row) {
        // An absent cursor and a cursor at zero are the same thing: this
        // device has never pulled that module.
        CursorPosition position;
        position.module = module;
        return position;
    }
    return from_row(*row);
}

std::vector<CursorPosition> Cursor::all(const Store& store) const {
    std::vector<CursorPosition> positions;
    for (std::size_t index = 0; index < protocol::kModuleCount; ++index) {
        positions.push_back(position(store, static_cast<protocol::ModuleId>(index)));
    }
    return positions;
}

CursorPosition Cursor::load(const Transaction& transaction, protocol::ModuleId module) const {
    require_known(module);
    const auto row = transaction.find(kTable, key_for(module));
    if (!row) {
        CursorPosition position;
        position.module = module;
        return position;
    }
    return from_row(*row);
}

void Cursor::save(Transaction& transaction, const CursorPosition& position) const {
    const std::string key = key_for(position.module);
    if (transaction.find(kTable, key)) {
        transaction.replace(kTable, key, to_row(position));
    } else {
        transaction.insert(kTable, to_row(position));
    }
}

void Cursor::advance(Transaction& transaction, protocol::ModuleId module,
                     std::int64_t sequence) const {
    CursorPosition position = load(transaction, module);

    if (sequence < position.sequence) {
        // A lower sequence is a stale or replayed reply. Accepting it would
        // pull and apply the same batch again on every cycle from now on.
        throw StoreError("a cursor cannot move backwards: " +
                         std::to_string(position.sequence) + " to " +
                         std::to_string(sequence));
    }

    const std::int64_t now = clock_();
    position.sequence = sequence;
    position.last_attempt_at = now;
    position.last_success_at = now;
    position.consecutive_failures = 0;
    position.last_error.clear();
    save(transaction, position);
}

void Cursor::record_failure(Transaction& transaction, protocol::ModuleId module,
                            const std::string& error) const {
    CursorPosition position = load(transaction, module);
    position.last_attempt_at = clock_();
    position.consecutive_failures += 1;
    position.last_error = error;
    // The sequence is untouched. A failed pull changes nothing about what has
    // been read; only about when it was last tried.
    save(transaction, position);
}

void Cursor::reset(Transaction& transaction, protocol::ModuleId module) const {
    CursorPosition position = load(transaction, module);
    position.sequence = 0;
    position.consecutive_failures = 0;
    position.last_error = "reset for a full pull";
    position.last_attempt_at = clock_();
    save(transaction, position);
}

std::vector<protocol::ModuleId> Cursor::never_pulled(const Store& store) const {
    std::vector<protocol::ModuleId> modules;
    for (const auto& position : all(store)) {
        if (position.never_pulled()) {
            modules.push_back(position.module);
        }
    }
    return modules;
}

}  // namespace squiflow::engine
