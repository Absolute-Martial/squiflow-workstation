#include "engine/storage/sqlite_store.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <utility>

#include "engine/storage/sqlite_statement.hpp"

namespace squiflow::engine {
namespace {

// SQLite has no way to parameterise an identifier, so table and column names
// are quoted by hand. Every name in this program comes from a header rather
// than from a person typing, but a doubled quote costs nothing and removes
// the question entirely.
std::string quoted(const std::string& identifier) {
    std::string result = "\"";
    for (const char character : identifier) {
        if (character == '"') {
            result += "\"\"";
        } else {
            result += character;
        }
    }
    result += '"';
    return result;
}

// LIKE treats these as wildcards, so a customer named "100% cotton" would
// otherwise match rather more than intended.
std::string escaped_for_like(const std::string& text) {
    std::string result;
    for (const char character : text) {
        if (character == '%' || character == '_' || character == '\\') {
            result += '\\';
        }
        result += character;
    }
    return result;
}

struct Clause {
    std::string sql;
    std::vector<Value> parameters;
};

}  // namespace

// ---------------------------------------------------------------------------
// The transaction
// ---------------------------------------------------------------------------

class SqliteTransaction final : public Transaction {
public:
    explicit SqliteTransaction(SqliteStore& store) : store_(store) {
        // IMMEDIATE rather than DEFERRED: the write lock is taken now, at a
        // point where failing is harmless, instead of part way through when
        // half the work has been done.
        store_.execute("BEGIN IMMEDIATE");
        open_ = true;
    }

    ~SqliteTransaction() override {
        if (open_) {
            // An early return or a thrown exception must not leave half a
            // record behind.
            try {
                rollback();
            } catch (const StoreError&) {
                // Nothing useful can be done from a destructor, and throwing
                // from one ends the program.
            }
        }
        store_.note_transaction_finished();
    }

    void insert(const std::string& table, const Row& row) override {
        require_open();
        const std::string& key_column = store_.key_column(table);
        const Value& key = row.get(key_column);
        if (key.is_null()) {
            throw StoreError("a row inserted into '" + table + "' needs a " + key_column);
        }
        if (find(table, key.text_or("")).has_value()) {
            throw StoreError("'" + key.text_or("") + "' is already in '" + table + "'");
        }
        store_.ensure_columns(table, row);
        write_row(table, row);
    }

    bool update(const std::string& table, const std::string& key, const Row& row) override {
        require_open();
        const auto existing = find(table, key);
        if (!existing) {
            return false;
        }
        refuse_key_change(table, key, row);
        // Update merges: a repository that writes two columns must not silently
        // erase the other eight.
        Row merged = *existing;
        merged.merge(row);
        store_.ensure_columns(table, merged);
        write_row(table, merged);
        return true;
    }

    bool replace(const std::string& table, const std::string& key, const Row& row) override {
        require_open();
        if (!find(table, key)) {
            return false;
        }
        refuse_key_change(table, key, row);
        store_.ensure_columns(table, row);
        remove_row(table, key);
        write_row(table, row);
        return true;
    }

    bool remove(const std::string& table, const std::string& key) override {
        require_open();
        if (!find(table, key)) {
            return false;
        }
        remove_row(table, key);
        return true;
    }

    std::vector<Row> select(const Query& query) const override {
        require_open();
        // The same connection, so this sees what the transaction has written
        // and nobody else's uncommitted work.
        return store_.select(query);
    }

    std::optional<Row> find(const std::string& table, const std::string& key) const override {
        require_open();
        return store_.find(table, key);
    }

    void commit() override {
        require_open();
        store_.execute("COMMIT");
        open_ = false;
    }

    void rollback() override {
        if (!open_) {
            return;
        }
        store_.execute("ROLLBACK");
        open_ = false;
    }

    bool open() const override {
        return open_;
    }

private:
    void require_open() const {
        if (!open_) {
            throw StoreError("this transaction has already finished");
        }
    }

    void refuse_key_change(const std::string& table, const std::string& key,
                           const Row& row) const {
        const std::string& key_column = store_.key_column(table);
        const Value& given = row.get(key_column);
        if (!given.is_null() && given.text_or("") != key) {
            // Changing a key is not an edit, it is a delete and an insert, and
            // everything pointing at the old key would be left pointing at
            // nothing.
            throw StoreError("the " + key_column + " of a row in '" + table +
                             "' cannot be changed");
        }
    }

    void write_row(const std::string& table, const Row& row) {
        std::string columns;
        std::string placeholders;
        std::vector<Value> values;
        for (const auto& field : row.fields()) {
            if (!columns.empty()) {
                columns += ", ";
                placeholders += ", ";
            }
            columns += quoted(field.first);
            placeholders += '?';
            values.push_back(field.second);
        }

        Statement statement{store_.handle_, "INSERT OR REPLACE INTO " + quoted(table) + " (" +
                                               columns + ") VALUES (" + placeholders + ")"};
        for (std::size_t index = 0; index < values.size(); ++index) {
            statement.bind(static_cast<int>(index) + 1, values[index]);
        }
        statement.run();
    }

    void remove_row(const std::string& table, const std::string& key) {
        Statement statement{store_.handle_, "DELETE FROM " + quoted(table) + " WHERE " +
                                                quoted(store_.key_column(table)) + " = ?"};
        statement.bind_text(1, key);
        statement.run();
    }

    SqliteStore& store_;
    bool open_{false};
};

// ---------------------------------------------------------------------------
// The store
// ---------------------------------------------------------------------------

SqliteStore::SqliteStore(const std::string& path) : path_(path) {
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_FULLMUTEX;
    if (sqlite3_open_v2(path_.c_str(), &handle_, flags, nullptr) != SQLITE_OK) {
        StoreError error = sqlite_error(handle_, "could not open '" + path_ + "'");
        sqlite3_close(handle_);
        handle_ = nullptr;
        throw error;
    }

    // The order matters: the busy timeout must be in place before anything
    // else can be refused for being busy.
    sqlite3_busy_timeout(handle_, 5000);
    execute("PRAGMA journal_mode = WAL");
    execute("PRAGMA synchronous = FULL");
    execute("PRAGMA foreign_keys = ON");
    // A modest cache: this machine has other work to do, and the file is
    // small enough that the operating system's own cache does most of it.
    execute("PRAGMA cache_size = -8000");

    load_schema();
}

SqliteStore::~SqliteStore() {
    if (handle_ != nullptr) {
        // A checkpoint on the way out keeps the write-ahead log from growing
        // across restarts and makes a copy of the file a complete backup.
        sqlite3_wal_checkpoint_v2(handle_, nullptr, SQLITE_CHECKPOINT_TRUNCATE, nullptr,
                                  nullptr);
        sqlite3_close_v2(handle_);
        handle_ = nullptr;
    }
}

const std::string& SqliteStore::path() const {
    return path_;
}

void SqliteStore::execute(const std::string& sql) const {
    Statement statement{handle_, sql};
    statement.run();
}

void SqliteStore::load_schema() {
    Statement tables{handle_,
                     "SELECT name FROM sqlite_master WHERE type = 'table' "
                     "AND name NOT LIKE 'sqlite_%'"};
    std::vector<std::string> names;
    while (tables.step()) {
        names.push_back(tables.column_value(0).text_or(""));
    }

    for (const auto& name : names) {
        std::vector<std::string> columns;
        std::string key_column;
        Statement info{handle_, "PRAGMA table_info(" + quoted(name) + ")"};
        while (info.step()) {
            const Row row = info.row();
            const std::string column = row.get("name").text_or("");
            columns.push_back(column);
            if (row.get("pk").integer_or(0) == 1) {
                key_column = column;
            }
        }
        columns_[name] = columns;
        if (!key_column.empty()) {
            key_columns_[name] = key_column;
        }
    }
}

void SqliteStore::define_table(const std::string& table, std::string key_column) {
    if (key_column.empty()) {
        throw StoreError("a table needs a key column");
    }
    if (writing_) {
        throw StoreError("a table cannot be defined while a transaction is open");
    }

    const auto existing = key_columns_.find(table);
    if (existing != key_columns_.end()) {
        if (existing->second != key_column) {
            // Silently accepting a different key would mean the same table
            // being read two ways in one program.
            throw StoreError("'" + table + "' already has the key column '" + existing->second +
                             "'");
        }
        return;
    }

    execute("CREATE TABLE IF NOT EXISTS " + quoted(table) + " (" + quoted(key_column) +
            " TEXT PRIMARY KEY NOT NULL)");
    key_columns_[table] = key_column;
    columns_[table] = {key_column};
}

bool SqliteStore::has_table(const std::string& table) const {
    return key_columns_.find(table) != key_columns_.end();
}

std::vector<std::string> SqliteStore::tables() const {
    std::vector<std::string> names;
    names.reserve(key_columns_.size());
    for (const auto& entry : key_columns_) {
        names.push_back(entry.first);
    }
    return names;
}

const std::string& SqliteStore::key_column(const std::string& table) const {
    const auto found = key_columns_.find(table);
    if (found == key_columns_.end()) {
        throw StoreError("no table called '" + table + "'");
    }
    return found->second;
}

bool SqliteStore::has_column(const std::string& table, const std::string& column) const {
    const auto found = columns_.find(table);
    if (found == columns_.end()) {
        return false;
    }
    return std::find(found->second.begin(), found->second.end(), column) != found->second.end();
}

void SqliteStore::ensure_columns(const std::string& table, const Row& row) const {
    auto& known = columns_[table];
    for (const auto& field : row.fields()) {
        if (std::find(known.begin(), known.end(), field.first) != known.end()) {
            continue;
        }
        // No type is given deliberately. SQLite stores what it is handed, and
        // this way a column holding minor units cannot be quietly converted to
        // a floating point number by an affinity rule.
        execute("ALTER TABLE " + quoted(table) + " ADD COLUMN " + quoted(field.first));
        known.push_back(field.first);
    }
}

std::unique_ptr<Transaction> SqliteStore::begin() {
    if (writing_) {
        // One writer. A second is refused here rather than queued; queuing is
        // the writer gate's job, one layer up, where it can be fair.
        throw StoreError("a transaction is already open");
    }
    auto transaction = std::make_unique<SqliteTransaction>(*this);
    writing_ = true;
    return transaction;
}

bool SqliteStore::writing() const {
    return writing_;
}

void SqliteStore::note_transaction_finished() {
    writing_ = false;
}

std::vector<Row> SqliteStore::select(const Query& query) const {
    if (!has_table(query.table())) {
        throw StoreError("no table called '" + query.table() + "'");
    }

    Clause where;
    for (const auto& condition : query.conditions()) {
        const bool known = has_column(query.table(), condition.column);
        std::string fragment;

        if (!known) {
            // The column has never been written, so every row is null there.
            // Answering from that fact is better than adding a column as a
            // side effect of a read.
            const bool matches_null =
                (condition.comparison == Comparison::Equal && condition.value.is_null()) ||
                (condition.comparison == Comparison::NotEqual && !condition.value.is_null());
            fragment = matches_null ? "1 = 1" : "1 = 0";
        } else {
            const std::string column = quoted(condition.column);
            switch (condition.comparison) {
                case Comparison::Equal:
                    if (condition.value.is_null()) {
                        fragment = column + " IS NULL";
                    } else {
                        fragment = column + " = ?";
                        where.parameters.push_back(condition.value);
                    }
                    break;
                case Comparison::NotEqual:
                    if (condition.value.is_null()) {
                        fragment = column + " IS NOT NULL";
                    } else {
                        // IS NOT rather than <>, so a null in the column
                        // counts as "not equal" instead of dropping the row.
                        fragment = column + " IS NOT ?";
                        where.parameters.push_back(condition.value);
                    }
                    break;
                case Comparison::Less:
                case Comparison::LessOrEqual:
                case Comparison::Greater:
                case Comparison::GreaterOrEqual: {
                    static const char* const operators[] = {"<", "<=", ">", ">="};
                    const auto index =
                        static_cast<std::size_t>(condition.comparison) -
                        static_cast<std::size_t>(Comparison::Less);
                    // A null never satisfies an ordering comparison, which is
                    // what SQL already does, so nothing extra is needed.
                    fragment = column + " " + operators[index] + " ?";
                    where.parameters.push_back(condition.value);
                    break;
                }
                case Comparison::Contains:
                    fragment = column + " LIKE ? ESCAPE '\\'";
                    where.parameters.push_back(
                        Value::text("%" + escaped_for_like(condition.value.text_or("")) + "%"));
                    break;
                case Comparison::StartsWith:
                    fragment = column + " LIKE ? ESCAPE '\\'";
                    where.parameters.push_back(
                        Value::text(escaped_for_like(condition.value.text_or("")) + "%"));
                    break;
            }
        }

        where.sql += where.sql.empty() ? " WHERE " : " AND ";
        where.sql += fragment;
    }

    std::string order;
    for (const auto& sort : query.sorts()) {
        if (!has_column(query.table(), sort.column)) {
            continue;
        }
        order += order.empty() ? " ORDER BY " : ", ";
        order += quoted(sort.column);
        order += sort.order == SortOrder::Descending ? " DESC" : " ASC";
    }
    // SQLite orders nulls first, then numbers, then text, then blobs, which is
    // the same order the in-memory store uses. The two agree without help.

    std::string limits;
    if (query.limit()) {
        limits += " LIMIT " + std::to_string(*query.limit());
    } else if (query.offset() > 0) {
        // SQLite has no OFFSET without a LIMIT.
        limits += " LIMIT -1";
    }
    if (query.offset() > 0) {
        limits += " OFFSET " + std::to_string(query.offset());
    }

    Statement statement{handle_,
                        "SELECT * FROM " + quoted(query.table()) + where.sql + order + limits};
    for (std::size_t index = 0; index < where.parameters.size(); ++index) {
        statement.bind(static_cast<int>(index) + 1, where.parameters[index]);
    }

    std::vector<Row> rows;
    while (statement.step()) {
        rows.push_back(statement.row());
    }
    return rows;
}

std::optional<Row> SqliteStore::find(const std::string& table, const std::string& key) const {
    Statement statement{handle_, "SELECT * FROM " + quoted(table) + " WHERE " +
                                     quoted(key_column(table)) + " = ? LIMIT 1"};
    statement.bind_text(1, key);
    if (!statement.step()) {
        return std::nullopt;
    }
    return statement.row();
}

std::size_t SqliteStore::count(const std::string& table) const {
    if (!has_table(table)) {
        throw StoreError("no table called '" + table + "'");
    }
    Statement statement{handle_, "SELECT COUNT(*) FROM " + quoted(table)};
    if (!statement.step()) {
        return 0;
    }
    const std::int64_t total = statement.column_value(0).integer_or(0);
    return total > 0 ? static_cast<std::size_t>(total) : 0;
}

bool SqliteStore::integrity_ok(std::string& detail) const {
    Statement statement{handle_, "PRAGMA integrity_check"};
    detail.clear();
    while (statement.step()) {
        const std::string line = statement.column_value(0).text_or("");
        if (line == "ok") {
            continue;
        }
        if (!detail.empty()) {
            detail += "; ";
        }
        detail += line;
    }
    return detail.empty();
}

void SqliteStore::compact() {
    if (writing_) {
        throw StoreError("the database cannot be compacted while a transaction is open");
    }
    execute("VACUUM");
    execute("ANALYZE");
}

}  // namespace squiflow::engine
