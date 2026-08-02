#pragma once

// The in-memory implementation of the storage seam.
//
// This is not a mock. It enforces the same rules the SQLite implementation
// must enforce: a declared table with a declared key column, a refusal to
// insert a duplicate key, one writer at a time, and a rollback that really
// restores the previous state. Every layer above storage is tested against
// this, which is why those layers can be tested at all on a machine with no
// database installed.

#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"

namespace squiflow::engine {

class MemoryStore final : public Store {
public:
    MemoryStore();
    ~MemoryStore() override;

    void define_table(const std::string& table, std::string key_column) override;
    bool has_table(const std::string& table) const override;
    std::vector<std::string> tables() const override;

    std::unique_ptr<Transaction> begin() override;
    bool writing() const override;

    std::vector<Row> select(const Query& query) const override;
    std::optional<Row> find(const std::string& table, const std::string& key) const override;
    std::size_t count(const std::string& table) const override;

    // Diagnostics used by tests and by the maintenance service.
    std::size_t total_rows() const;
    std::size_t committed_transactions() const;
    std::size_t rolled_back_transactions() const;

private:
    friend class MemoryTransaction;

    struct Table {
        std::string key_column{};
        std::map<std::string, Row> rows{};
    };

    const Table& table_for(const std::string& table) const;
    Table& table_for(const std::string& table);
    std::string key_of(const Table& table, const Row& row) const;

    void release_writer();
    void note_commit();
    void note_rollback();

    std::map<std::string, Table> tables_{};
    bool writing_{false};
    std::size_t commits_{0};
    std::size_t rollbacks_{0};
};

}  // namespace squiflow::engine
