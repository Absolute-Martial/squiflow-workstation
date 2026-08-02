#include "engine/storage/migration_runner.hpp"

#include <utility>

namespace squiflow::engine {
namespace {

const std::string kTable = "schema_migration";
const std::string kNumber = "number";
const std::string kName = "name";
const std::string kAppliedAt = "applied_at";

}  // namespace

const std::string& MigrationRunner::table_name() {
    return kTable;
}

MigrationRunner::MigrationRunner(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) {
        throw StoreError("the migration runner needs a clock");
    }
}

void MigrationRunner::add(Migration migration) {
    if (migration.number <= 0) {
        throw StoreError("a migration number must be positive");
    }
    if (migration.name.empty()) {
        throw StoreError("migration " + std::to_string(migration.number) + " has no name");
    }
    if (!migration.schema && !migration.data) {
        throw StoreError("migration " + std::to_string(migration.number) + " does nothing");
    }
    const auto existing = migrations_.find(migration.number);
    if (existing != migrations_.end()) {
        // Two migrations with one number means two branches were merged
        // without anyone looking. Refusing here is cheaper than discovering it
        // on the shop's machine.
        throw StoreError("migration " + std::to_string(migration.number) +
                         " is already registered as '" + existing->second.name + "'");
    }
    migrations_.emplace(migration.number, std::move(migration));
}

std::size_t MigrationRunner::size() const {
    return migrations_.size();
}

int MigrationRunner::highest_known() const {
    if (migrations_.empty()) {
        return 0;
    }
    return migrations_.rbegin()->first;
}

void MigrationRunner::prepare(Store& store) const {
    store.define_table(kTable, kNumber);
}

int MigrationRunner::applied_version(const Store& store) const {
    if (!store.has_table(kTable)) {
        return 0;
    }
    Query query{kTable};
    query.order_by(kNumber, SortOrder::Descending).take(1);
    const auto rows = store.select(query);
    if (rows.empty()) {
        return 0;
    }
    return static_cast<int>(rows.front().get(kNumber).integer_or(0));
}

std::vector<Row> MigrationRunner::history(const Store& store) const {
    if (!store.has_table(kTable)) {
        return {};
    }
    Query query{kTable};
    query.order_by(kNumber);
    return store.select(query);
}

std::vector<int> MigrationRunner::pending(const Store& store) const {
    std::vector<int> numbers;
    const auto rows = history(store);
    for (const auto& entry : migrations_) {
        bool found = false;
        for (const auto& row : rows) {
            if (static_cast<int>(row.get(kNumber).integer_or(0)) == entry.first) {
                found = true;
                break;
            }
        }
        if (!found) {
            numbers.push_back(entry.first);
        }
    }
    return numbers;
}

MigrationOutcome MigrationRunner::run(Store& store) const {
    prepare(store);

    std::map<int, std::string> applied;
    for (const auto& row : history(store)) {
        applied.emplace(static_cast<int>(row.get(kNumber).integer_or(0)),
                        row.get(kName).text_or(""));
    }

    const int on_disk = applied.empty() ? 0 : applied.rbegin()->first;

    // The downgrade case. A newer build of the application opened this
    // database and changed it; this older build does not know what those
    // changes were. Refusing to start is the only safe move - carrying on
    // would write rows in a shape the newer version will not recognise.
    if (on_disk > highest_known()) {
        throw StoreError("this database is at version " + std::to_string(on_disk) +
                         " but this build only knows up to " + std::to_string(highest_known()) +
                         "; it was written by a newer version of the application");
    }

    MigrationOutcome outcome;
    outcome.version = on_disk;

    for (const auto& entry : migrations_) {
        const int number = entry.first;
        const Migration& migration = entry.second;

        const auto seen = applied.find(number);
        if (seen != applied.end()) {
            // The recorded name is checked, not just the number. A migration
            // edited after it shipped is a different migration wearing the
            // same number, and the two databases will quietly diverge.
            if (seen->second != migration.name) {
                throw StoreError("migration " + std::to_string(number) + " was applied as '" +
                                 seen->second + "' but this build calls it '" + migration.name +
                                 "'; a migration may never be edited after it has shipped");
            }
            continue;
        }

        // An unapplied migration numbered below one already applied means two
        // lines of work were merged out of order. Running it now would apply
        // it against a schema it was never written for.
        if (number < on_disk) {
            throw StoreError("migration " + std::to_string(number) + " ('" + migration.name +
                             "') has not been applied, but the database is already at version " +
                             std::to_string(on_disk) + "; migrations may not arrive out of order");
        }

        if (migration.schema) {
            migration.schema(store);
        }

        auto transaction = store.begin();
        if (migration.data) {
            migration.data(*transaction);
        }

        Row record;
        record.set(kNumber, Value::integer(number));
        record.set(kName, Value::text(migration.name));
        record.set(kAppliedAt, Value::integer(clock_()));
        transaction->insert(kTable, record);

        // The version record commits with the migration's own writes. Either
        // both land or neither does.
        transaction->commit();

        outcome.applied.push_back(number);
        outcome.version = number;
    }

    outcome.already_current = outcome.applied.empty();
    return outcome;
}

}  // namespace squiflow::engine
