#pragma once

// The database as the rest of the application sees it.
//
// One object owns the store, the migration runner and the writer gate, and
// nothing above this layer may reach past it to the store directly. That is
// the whole point: if a module could open its own transaction, the single
// writer rule would hold only for as long as everybody remembered it.
//
// A database is closed until it is opened, and a closed database refuses
// everything by name rather than returning empty results. An empty list is
// indistinguishable from a shop with no customers, and that is exactly the
// kind of error that gets noticed a week later.

#include <chrono>
#include <memory>
#include <string>

#include "engine/storage/migration_runner.hpp"
#include "engine/storage/store.hpp"
#include "engine/storage/writer.hpp"

namespace squiflow::engine {

class Database {
public:
    Database(std::unique_ptr<Store> store, MigrationRunner runner);

    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    Database(Database&&) = delete;
    Database& operator=(Database&&) = delete;
    ~Database();

    // Runs pending migrations and makes the database usable. Called once, at
    // startup, before any background service exists - which is why it is the
    // one write that does not go through the gate.
    void open();
    void close();

    bool ready() const;
    int version() const;

    void write(const Writer::Work& work);
    bool write_within(const Writer::Work& work, std::chrono::milliseconds patience);
    void read(const Writer::Read& reader) const;

    std::uint64_t waiting() const;
    Writer::Statistics statistics() const;

private:
    void require_ready() const;

    std::unique_ptr<Store> store_;
    MigrationRunner runner_;
    Writer writer_;
    bool ready_{false};
    int version_{0};
};

}  // namespace squiflow::engine
