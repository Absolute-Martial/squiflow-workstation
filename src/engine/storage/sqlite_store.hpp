#pragma once

// The store the shop actually runs on.
//
// Everything above this file is written against `Store` and `Transaction` and
// has been executed against the in-memory implementation. This one translates
// the same operations into SQL. It is the only file in the engine that knows
// SQLite exists, and even here the header hides it: `sqlite3` is forward
// declared, so no header in the tree pulls in a SQLite dependency.
//
// Settings applied on open, and why:
//
//   journal_mode = WAL     A reader never blocks the writer. On a spinning
//                          disk that is the difference between a screen that
//                          responds and one that does not.
//   busy_timeout = 5000    A five second wait rather than an immediate
//                          failure, for the brief overlap a checkpoint can
//                          cause. Fairness between our own writers is the
//                          writer gate's job; this is only for the engine's
//                          own internal contention.
//   synchronous = FULL     A power cut in a shop is normal. NORMAL risks the
//                          last transactions on a crash, and those
//                          transactions are invoices.
//   foreign_keys = ON      Off by default in SQLite, which surprises people.

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"

struct sqlite3;

namespace squiflow::engine {

class SqliteStore final : public Store {
public:
    // A path on disk, or ":memory:" for a database that lives as long as the
    // object. The on-disk one is the shop's; the in-memory one is for tests
    // that need the real SQL engine rather than the fake.
    explicit SqliteStore(const std::string& path);
    ~SqliteStore() override;

    void define_table(const std::string& table, std::string key_column) override;
    bool has_table(const std::string& table) const override;
    std::vector<std::string> tables() const override;

    std::unique_ptr<Transaction> begin() override;
    bool writing() const override;

    std::vector<Row> select(const Query& query) const override;
    std::optional<Row> find(const std::string& table, const std::string& key) const override;
    std::size_t count(const std::string& table) const override;

    const std::string& path() const;

    // Reports what SQLite thinks of the file. Run at startup and after an
    // unclean shutdown; a corrupt database found at eight in the morning is a
    // restore, and one found at six in the evening is a lost day.
    bool integrity_ok(std::string& detail) const;

    // Reclaims space and rewrites the file. Only ever called when nobody is
    // waiting, because on a spinning disk it takes as long as it takes.
    void compact();

private:
    friend class SqliteTransaction;

    void execute(const std::string& sql) const;
    void load_schema();
    void ensure_columns(const std::string& table, const Row& row) const;
    const std::string& key_column(const std::string& table) const;
    bool has_column(const std::string& table, const std::string& column) const;
    void note_transaction_finished();

    sqlite3* handle_{nullptr};
    std::string path_;
    // Table name to key column, and the columns known to exist. Columns are
    // added as rows introduce them, so a repository never has to declare a
    // column twice - once in a migration and once in its code.
    mutable std::map<std::string, std::string> key_columns_{};
    mutable std::map<std::string, std::vector<std::string>> columns_{};
    bool writing_{false};
};

}  // namespace squiflow::engine
