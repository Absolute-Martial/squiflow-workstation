#include "engine/storage/database.hpp"

#include <utility>

namespace squiflow::engine {

Database::Database(std::unique_ptr<Store> store, MigrationRunner runner)
    : store_(std::move(store)), runner_(std::move(runner)), writer_(*store_) {
    if (!store_) {
        throw StoreError("a database needs a store");
    }
}

Database::~Database() = default;

void Database::open() {
    if (ready_) {
        throw StoreError("the database is already open");
    }
    // Migrations run before the gate matters: nothing else exists yet to
    // compete for the writer. If this throws - an edited migration, a database
    // from a newer build - the database stays closed, which is the correct
    // outcome. Starting anyway would mean writing into a schema we do not
    // understand.
    const MigrationOutcome outcome = runner_.run(*store_);
    version_ = outcome.version;
    ready_ = true;
}

void Database::close() {
    ready_ = false;
}

bool Database::ready() const {
    return ready_;
}

int Database::version() const {
    return version_;
}

void Database::require_ready() const {
    if (!ready_) {
        throw StoreError("the database is not open");
    }
}

void Database::write(const Writer::Work& work) {
    require_ready();
    writer_.write(work);
}

bool Database::write_within(const Writer::Work& work, std::chrono::milliseconds patience) {
    require_ready();
    return writer_.write_within(work, patience);
}

void Database::read(const Writer::Read& reader) const {
    require_ready();
    writer_.read(reader);
}

std::uint64_t Database::waiting() const {
    return writer_.waiting();
}

Writer::Statistics Database::statistics() const {
    return writer_.statistics();
}

}  // namespace squiflow::engine
