#include "engine/storage/memory_store.hpp"

#include <algorithm>
#include <utility>

namespace squiflow::engine {
namespace {

using TableMap = std::map<std::string, Row>;

std::vector<Row> run_query(const TableMap& rows, const Query& query) {
    std::vector<Row> matched;
    for (const auto& entry : rows) {
        if (query.matches(entry.second)) {
            matched.push_back(entry.second);
        }
    }

    // Stable, and applied in reverse so that the first sort named is the
    // primary one. Sorting forwards would make the last sort win, which is the
    // opposite of what anyone writing the query expects.
    const auto& sorts = query.sorts();
    for (auto it = sorts.rbegin(); it != sorts.rend(); ++it) {
        const Sort& sort = *it;
        std::stable_sort(matched.begin(), matched.end(),
                         [&sort](const Row& left, const Row& right) {
                             const int order = left.get(sort.column).compare(right.get(sort.column));
                             return sort.order == SortOrder::Ascending ? order < 0 : order > 0;
                         });
    }

    const std::size_t offset = query.offset();
    if (offset >= matched.size()) {
        return {};
    }
    matched.erase(matched.begin(), matched.begin() + static_cast<std::ptrdiff_t>(offset));

    if (const auto limit = query.limit()) {
        if (*limit < matched.size()) {
            matched.resize(*limit);
        }
    }
    return matched;
}

}  // namespace

// A transaction works on its own copy of the tables and swaps it in on commit.
// That makes rollback exact rather than approximate, and it means a reader
// outside the transaction never sees uncommitted rows. The cost is copying the
// map, which is acceptable for a store that exists to make tests possible.
class MemoryTransaction final : public Transaction {
public:
    explicit MemoryTransaction(MemoryStore& store)
        : store_(store), working_(store.tables_) {}

    ~MemoryTransaction() override {
        if (open_) {
            rollback();
        }
    }

    void insert(const std::string& table, const Row& row) override {
        require_open();
        MemoryStore::Table& target = table_for(table);
        const std::string key = store_.key_of(target, row);
        if (target.rows.find(key) != target.rows.end()) {
            throw StoreError("duplicate key '" + key + "' in table '" + table + "'");
        }
        target.rows.emplace(key, row);
    }

    bool update(const std::string& table, const std::string& key, const Row& row) override {
        require_open();
        MemoryStore::Table& target = table_for(table);
        const auto found = target.rows.find(key);
        if (found == target.rows.end()) {
            return false;
        }
        Row updated = found->second;
        updated.merge(row);
        if (store_.key_of(target, updated) != key) {
            throw StoreError("an update may not change the key of a row in '" + table + "'");
        }
        found->second = std::move(updated);
        return true;
    }

    bool replace(const std::string& table, const std::string& key, const Row& row) override {
        require_open();
        MemoryStore::Table& target = table_for(table);
        const auto found = target.rows.find(key);
        if (found == target.rows.end()) {
            return false;
        }
        if (store_.key_of(target, row) != key) {
            throw StoreError("a replacement may not change the key of a row in '" + table + "'");
        }
        found->second = row;
        return true;
    }

    bool remove(const std::string& table, const std::string& key) override {
        require_open();
        MemoryStore::Table& target = table_for(table);
        return target.rows.erase(key) > 0;
    }

    std::vector<Row> select(const Query& query) const override {
        require_open();
        const auto found = working_.find(query.table());
        if (found == working_.end()) {
            throw StoreError("no such table '" + query.table() + "'");
        }
        return run_query(found->second.rows, query);
    }

    std::optional<Row> find(const std::string& table, const std::string& key) const override {
        require_open();
        const auto table_found = working_.find(table);
        if (table_found == working_.end()) {
            throw StoreError("no such table '" + table + "'");
        }
        const auto row_found = table_found->second.rows.find(key);
        if (row_found == table_found->second.rows.end()) {
            return std::nullopt;
        }
        return row_found->second;
    }

    void commit() override {
        require_open();
        store_.tables_ = std::move(working_);
        open_ = false;
        store_.note_commit();
        store_.release_writer();
    }

    void rollback() override {
        if (!open_) {
            return;
        }
        working_.clear();
        open_ = false;
        store_.note_rollback();
        store_.release_writer();
    }

    bool open() const override {
        return open_;
    }

private:
    void require_open() const {
        if (!open_) {
            throw StoreError("the transaction is no longer open");
        }
    }

    MemoryStore::Table& table_for(const std::string& table) {
        const auto found = working_.find(table);
        if (found == working_.end()) {
            throw StoreError("no such table '" + table + "'");
        }
        return found->second;
    }

    MemoryStore& store_;
    std::map<std::string, MemoryStore::Table> working_;
    bool open_{true};
};

// ---------------------------------------------------------- MemoryStore

MemoryStore::MemoryStore() = default;

MemoryStore::~MemoryStore() = default;

void MemoryStore::define_table(const std::string& table, std::string key_column) {
    if (writing_) {
        throw StoreError("a table may not be defined while a transaction is open");
    }
    if (key_column.empty()) {
        throw StoreError("table '" + table + "' needs a key column");
    }
    const auto existing = tables_.find(table);
    if (existing != tables_.end()) {
        if (existing->second.key_column != key_column) {
            throw StoreError("table '" + table + "' is already defined with a different key");
        }
        return;
    }
    Table created;
    created.key_column = std::move(key_column);
    tables_.emplace(table, std::move(created));
}

bool MemoryStore::has_table(const std::string& table) const {
    return tables_.find(table) != tables_.end();
}

std::vector<std::string> MemoryStore::tables() const {
    std::vector<std::string> names;
    names.reserve(tables_.size());
    for (const auto& entry : tables_) {
        names.push_back(entry.first);
    }
    return names;
}

std::unique_ptr<Transaction> MemoryStore::begin() {
    if (writing_) {
        throw StoreError("a write transaction is already open; there is exactly one writer");
    }
    writing_ = true;
    return std::make_unique<MemoryTransaction>(*this);
}

bool MemoryStore::writing() const {
    return writing_;
}

std::vector<Row> MemoryStore::select(const Query& query) const {
    return run_query(table_for(query.table()).rows, query);
}

std::optional<Row> MemoryStore::find(const std::string& table, const std::string& key) const {
    const Table& target = table_for(table);
    const auto found = target.rows.find(key);
    if (found == target.rows.end()) {
        return std::nullopt;
    }
    return found->second;
}

std::size_t MemoryStore::count(const std::string& table) const {
    return table_for(table).rows.size();
}

std::size_t MemoryStore::total_rows() const {
    std::size_t total = 0;
    for (const auto& entry : tables_) {
        total += entry.second.rows.size();
    }
    return total;
}

std::size_t MemoryStore::committed_transactions() const {
    return commits_;
}

std::size_t MemoryStore::rolled_back_transactions() const {
    return rollbacks_;
}

const MemoryStore::Table& MemoryStore::table_for(const std::string& table) const {
    const auto found = tables_.find(table);
    if (found == tables_.end()) {
        throw StoreError("no such table '" + table + "'");
    }
    return found->second;
}

MemoryStore::Table& MemoryStore::table_for(const std::string& table) {
    const auto found = tables_.find(table);
    if (found == tables_.end()) {
        throw StoreError("no such table '" + table + "'");
    }
    return found->second;
}

std::string MemoryStore::key_of(const Table& table, const Row& row) const {
    const Value& key = row.get(table.key_column);
    if (key.is_null()) {
        throw StoreError("a row is missing its key column '" + table.key_column + "'");
    }
    if (key.kind() != ValueKind::Text && key.kind() != ValueKind::Integer) {
        throw StoreError("the key column '" + table.key_column + "' must be text or an integer");
    }
    return key.describe();
}

void MemoryStore::release_writer() {
    writing_ = false;
}

void MemoryStore::note_commit() {
    ++commits_;
}

void MemoryStore::note_rollback() {
    ++rollbacks_;
}

}  // namespace squiflow::engine
