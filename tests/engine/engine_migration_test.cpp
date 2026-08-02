// Phase 3.3: the migration runner.
//
// Migrations are the one piece of code that can destroy a shop's data while
// behaving exactly as written. So the tests here are mostly about refusals:
// the cases where the runner must stop rather than do its best.

#include <cstdint>
#include <string>
#include <vector>

#include "engine/storage/memory_store.hpp"
#include "engine/storage/migration_runner.hpp"
#include "support/check.hpp"

namespace {

using squiflow::engine::Migration;
using squiflow::engine::MigrationRunner;
using squiflow::engine::MemoryStore;
using squiflow::engine::Query;
using squiflow::engine::Row;
using squiflow::engine::Store;
using squiflow::engine::StoreError;
using squiflow::engine::Transaction;
using squiflow::engine::Value;
using squiflow::testing::check;
using squiflow::testing::report;
using squiflow::testing::section;

std::int64_t g_now = 1000;

std::int64_t clock_source() {
    g_now += 10;
    return g_now;
}

MigrationRunner make_runner() {
    return MigrationRunner{clock_source};
}

Migration create_parties(int number, std::string name) {
    Migration migration;
    migration.number = number;
    migration.name = std::move(name);
    migration.schema = [](Store& store) { store.define_table("parties", "id"); };
    return migration;
}

Migration seed_party(int number, std::string name, std::string id) {
    Migration migration;
    migration.number = number;
    migration.name = std::move(name);
    migration.data = [id](Transaction& transaction) {
        Row row;
        row.set("id", Value::text(id));
        row.set("name", Value::text("seeded"));
        transaction.insert("parties", row);
    };
    return migration;
}

template <typename Callable>
bool refuses(Callable&& callable) {
    try {
        callable();
    } catch (const StoreError&) {
        return true;
    }
    return false;
}

void test_registration() {
    section("registration");

    MigrationRunner runner = make_runner();
    runner.add(create_parties(1, "create parties"));
    check(runner.size() == 1, "a migration was registered");
    check(runner.highest_known() == 1, "the highest known version is reported");

    // Two migrations with one number means two branches were merged without
    // anyone looking.
    check(refuses([&runner] { runner.add(create_parties(1, "create parties again")); }),
          "a duplicate migration number is refused");

    check(refuses([&runner] { runner.add(create_parties(0, "zero")); }),
          "a migration number must be positive");
    check(refuses([&runner] { runner.add(create_parties(2, "")); }),
          "a migration must be named");

    Migration empty;
    empty.number = 3;
    empty.name = "does nothing";
    check(refuses([&runner, &empty] { runner.add(empty); }),
          "a migration that does nothing is refused");

    check(refuses([] { MigrationRunner broken{nullptr}; }),
          "the runner needs a clock");
}

void test_first_run() {
    section("first run");

    MemoryStore store;
    MigrationRunner runner = make_runner();
    runner.add(create_parties(1, "create parties"));
    runner.add(seed_party(2, "seed the walk-in customer", "walk-in"));

    check(runner.applied_version(store) == 0, "an untouched database is at version zero");
    check(runner.pending(store).size() == 2, "both migrations are pending");

    const auto outcome = runner.run(store);
    check(outcome.applied.size() == 2, "both migrations ran");
    check(outcome.applied.front() == 1 && outcome.applied.back() == 2,
          "they ran in ascending order");
    check(outcome.version == 2, "the database reports the new version");
    check(!outcome.already_current, "the run was not a no-op");

    check(store.has_table("parties"), "the schema half ran");
    check(store.count("parties") == 1, "the data half ran");
    check(runner.applied_version(store) == 2, "the version is readable afterwards");
    check(runner.pending(store).empty(), "nothing is pending afterwards");

    const auto history = runner.history(store);
    check(history.size() == 2, "the history has one row per migration");
    check(history.front().get("name").text_or("") == "create parties",
          "the history records the name, not only the number");
    check(history.front().get("applied_at").integer_or(0) > 0,
          "the history records when it was applied");
    check(history.front().get("applied_at").integer_or(0) <
              history.back().get("applied_at").integer_or(0),
          "the recorded times are in the order the migrations ran");
}

void test_second_run_is_a_no_op() {
    section("running twice");

    MemoryStore store;
    MigrationRunner runner = make_runner();
    runner.add(create_parties(1, "create parties"));
    runner.add(seed_party(2, "seed the walk-in customer", "walk-in"));
    runner.run(store);

    const auto second = runner.run(store);
    check(second.applied.empty(), "nothing ran the second time");
    check(second.already_current, "the run reports the database was already current");
    check(second.version == 2, "the version is unchanged");
    check(store.count("parties") == 1, "the seed was not inserted twice");
    check(runner.history(store).size() == 2, "the history did not grow");
}

void test_partial_upgrade() {
    section("upgrading an existing database");

    MemoryStore store;
    MigrationRunner first = make_runner();
    first.add(create_parties(1, "create parties"));
    first.run(store);

    // The next release knows one more migration than the one that created the
    // database. Only the new one may run.
    MigrationRunner next = make_runner();
    next.add(create_parties(1, "create parties"));
    next.add(seed_party(2, "seed the walk-in customer", "walk-in"));

    check(next.pending(store).size() == 1, "only the unapplied migration is pending");
    const auto outcome = next.run(store);
    check(outcome.applied.size() == 1 && outcome.applied.front() == 2,
          "only the new migration ran");
    check(outcome.version == 2, "the version moved forward by one");
}

void test_edited_migration_is_refused() {
    section("an edited migration");

    MemoryStore store;
    MigrationRunner shipped = make_runner();
    shipped.add(create_parties(1, "create parties"));
    shipped.run(store);

    // Same number, different name. This is a migration that was edited after
    // it shipped, which means two shops are running different schemas that
    // both claim to be version 1.
    MigrationRunner edited = make_runner();
    edited.add(create_parties(1, "create parties and contacts"));
    check(refuses([&edited, &store] { edited.run(store); }),
          "a migration edited after shipping is refused");
    check(store.count(squiflow::engine::MigrationRunner::table_name()) == 1,
          "the refusal changed nothing");
}

void test_out_of_order_is_refused() {
    section("an out-of-order migration");

    MemoryStore store;
    MigrationRunner first = make_runner();
    first.add(create_parties(1, "create parties"));
    first.add(seed_party(3, "seed the walk-in customer", "walk-in"));
    first.run(store);

    // Migration 2 arrives after 3 has already been applied - two lines of work
    // merged out of order. Running it now would apply it against a schema it
    // was never written for.
    MigrationRunner merged = make_runner();
    merged.add(create_parties(1, "create parties"));
    merged.add(seed_party(2, "seed a second party", "late"));
    merged.add(seed_party(3, "seed the walk-in customer", "walk-in"));
    check(refuses([&merged, &store] { merged.run(store); }),
          "a migration that arrives out of order is refused");
    check(store.count("parties") == 1, "the refusal did not insert the late row");
}

void test_downgrade_is_refused() {
    section("a database from a newer build");

    MemoryStore store;
    MigrationRunner newer = make_runner();
    newer.add(create_parties(1, "create parties"));
    newer.add(seed_party(2, "seed the walk-in customer", "walk-in"));
    newer.run(store);

    // An older build opens a database a newer build has already changed. It
    // cannot know what those changes were, so it must refuse to start rather
    // than write rows in a shape the newer version will not recognise.
    MigrationRunner older = make_runner();
    older.add(create_parties(1, "create parties"));
    check(refuses([&older, &store] { older.run(store); }),
          "an older build refuses a database written by a newer one");
    check(older.applied_version(store) == 2, "the database is left at its own version");
}

void test_failure_leaves_nothing_behind() {
    section("a migration that fails");

    MemoryStore store;
    MigrationRunner runner = make_runner();
    runner.add(create_parties(1, "create parties"));
    runner.add(seed_party(2, "seed the walk-in customer", "walk-in"));

    Migration broken;
    broken.number = 3;
    broken.name = "a migration that throws halfway";
    broken.data = [](Transaction& transaction) {
        Row row;
        row.set("id", Value::text("half-written"));
        transaction.insert("parties", row);
        throw StoreError("something went wrong halfway through");
    };
    runner.add(broken);

    check(refuses([&runner, &store] { runner.run(store); }),
          "the failure is reported rather than swallowed");

    // The half-written row and the version record must both be gone. A
    // database that claims a version it never reached is worse than one that
    // is simply out of date.
    check(store.count("parties") == 1, "the half-written row was rolled back");
    check(runner.applied_version(store) == 2, "the failed migration was not recorded");
    check(!store.writing(), "the writer was released after the failure");

    // And the run is repeatable once the migration is fixed.
    MigrationRunner fixed = make_runner();
    fixed.add(create_parties(1, "create parties"));
    fixed.add(seed_party(2, "seed the walk-in customer", "walk-in"));
    fixed.add(seed_party(3, "a migration that throws halfway", "repaired"));
    const auto outcome = fixed.run(store);
    check(outcome.applied.size() == 1 && outcome.version == 3,
          "the repaired migration runs on the next attempt");
    check(store.count("parties") == 2, "and its write lands this time");
}

void test_schema_steps_are_idempotent() {
    section("idempotent schema steps");

    // The schema half runs outside the transaction, so a crash before the
    // data half commits means it will run a second time. Defining a table
    // twice must therefore be harmless, and it is.
    MemoryStore store;
    MigrationRunner runner = make_runner();
    runner.add(create_parties(1, "create parties"));
    runner.run(store);

    MigrationRunner again = make_runner();
    Migration repeat = create_parties(2, "create parties once more");
    again.add(create_parties(1, "create parties"));
    again.add(repeat);
    const auto outcome = again.run(store);
    check(outcome.applied.size() == 1, "the repeated schema step ran without complaint");
    check(store.has_table("parties"), "the table is still there");
}

}  // namespace

int main() {
    test_registration();
    test_first_run();
    test_second_run_is_a_no_op();
    test_partial_upgrade();
    test_edited_migration_is_refused();
    test_out_of_order_is_refused();
    test_downgrade_is_refused();
    test_failure_leaves_nothing_behind();
    test_schema_steps_are_idempotent();
    return report();
}
