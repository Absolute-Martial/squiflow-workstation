// Phase 3.4: the database gate and the writer queue.
//
// This is the first code in the project that is genuinely concurrent, so the
// tests actually start threads. A single-threaded test of a queue proves
// nothing: the failures being defended against here - a lost update, a
// starved writer, a queue stopped by an exception - only appear when two
// threads want the same thing at the same moment.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/storage/migration_runner.hpp"
#include "engine/storage/store.hpp"
#include "engine/storage/writer.hpp"
#include "support/check.hpp"

namespace {

using namespace std::chrono_literals;

using squiflow::engine::Database;
using squiflow::engine::MemoryStore;
using squiflow::engine::Migration;
using squiflow::engine::MigrationRunner;
using squiflow::engine::Query;
using squiflow::engine::Row;
using squiflow::engine::Store;
using squiflow::engine::StoreError;
using squiflow::engine::Transaction;
using squiflow::engine::Value;
using squiflow::testing::check;
using squiflow::testing::report;
using squiflow::testing::section;

const std::string kCounter = "counter";
const std::string kEntries = "entries";

std::int64_t steady_clock_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

MigrationRunner shop_migrations() {
    MigrationRunner runner{steady_clock_ms};

    Migration first;
    first.number = 1;
    first.name = "create the counter and the entries";
    first.schema = [](Store& store) {
        store.define_table(kCounter, "id");
        store.define_table(kEntries, "id");
    };
    first.data = [](Transaction& transaction) {
        Row row;
        row.set("id", Value::text("total"));
        row.set("value", Value::integer(0));
        transaction.insert(kCounter, row);
    };
    runner.add(first);

    return runner;
}

std::unique_ptr<Database> open_database() {
    auto database = std::make_unique<Database>(std::make_unique<MemoryStore>(), shop_migrations());
    database->open();
    return database;
}

// Read, add one, write back - inside one transaction. Without the gate this
// is the textbook lost update, and with it the answer must be exact.
void increment(Transaction& transaction) {
    const auto current = transaction.find(kCounter, "total");
    if (!current) {
        throw StoreError("the counter is missing");
    }
    Row patch;
    patch.set("value", Value::integer(current->get("value").integer_or(0) + 1));
    transaction.update(kCounter, "total", patch);
}

std::int64_t counter_value(const Database& database) {
    std::int64_t value = 0;
    database.read([&value](const Store& store) {
        const auto row = store.find(kCounter, "total");
        value = row ? row->get("value").integer_or(-1) : -1;
    });
    return value;
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

void test_the_gate_is_shut_until_opened() {
    section("a closed database");

    Database database{std::make_unique<MemoryStore>(), shop_migrations()};
    check(!database.ready(), "a new database is closed");
    check(database.version() == 0, "a closed database reports no version");

    // Refusing by name rather than returning nothing. An empty list is
    // indistinguishable from a shop with no customers.
    check(refuses([&database] { database.write(increment); }),
          "a closed database refuses writes");
    check(refuses([&database] { database.read([](const Store&) {}); }),
          "a closed database refuses reads");

    database.open();
    check(database.ready(), "opening makes it usable");
    check(database.version() == 1, "opening ran the migrations");
    check(refuses([&database] { database.open(); }), "opening twice is refused");

    database.close();
    check(refuses([&database] { database.write(increment); }),
          "closing shuts the gate again");
}

void test_a_failed_open_leaves_it_closed() {
    section("a database that will not open");

    MigrationRunner broken{steady_clock_ms};
    Migration migration;
    migration.number = 1;
    migration.name = "a migration that fails";
    migration.data = [](Transaction&) { throw StoreError("the migration failed"); };
    broken.add(migration);

    Database database{std::make_unique<MemoryStore>(), std::move(broken)};
    check(refuses([&database] { database.open(); }), "the failure is reported");

    // Starting anyway would mean writing into a schema nobody understands.
    check(!database.ready(), "a database whose migrations failed stays closed");
    check(refuses([&database] { database.write(increment); }),
          "and it refuses work rather than half-working");
}

void test_no_lost_updates() {
    section("no lost updates under contention");

    auto database = open_database();

    const int threads = 8;
    const int each = 25;
    std::vector<std::thread> workers;
    workers.reserve(static_cast<std::size_t>(threads));

    for (int index = 0; index < threads; ++index) {
        workers.emplace_back([&database] {
            for (int step = 0; step < each; ++step) {
                database->write(increment);
            }
        });
    }
    for (auto& worker : workers) {
        worker.join();
    }

    // Two hundred read-modify-write cycles across eight threads. Any number
    // below two hundred is a lost update.
    check(counter_value(*database) == threads * each,
          "every increment landed; nothing was lost");

    const auto statistics = database->statistics();
    check(statistics.completed == static_cast<std::uint64_t>(threads * each),
          "every write is counted as completed");
    check(statistics.failed == 0, "none failed");
    check(statistics.peak_waiting > 1, "the queue really was contended");
    check(database->waiting() == 0, "nobody is left waiting afterwards");
}

void test_no_starvation() {
    section("no starvation");

    auto database = open_database();

    // One thread hammers the gate while another makes a handful of writes.
    // With retry-on-busy instead of a queue, the quiet thread is the one that
    // waits forever; with a ticket it cannot.
    std::atomic<bool> stop{false};
    std::atomic<int> greedy_writes{0};

    std::thread greedy([&database, &stop, &greedy_writes] {
        while (!stop.load()) {
            database->write(increment);
            greedy_writes.fetch_add(1);
        }
    });

    std::atomic<int> polite_writes{0};
    std::thread polite([&database, &polite_writes] {
        for (int step = 0; step < 20; ++step) {
            database->write(increment);
            polite_writes.fetch_add(1);
        }
    });

    polite.join();
    stop.store(true);
    greedy.join();

    check(polite_writes.load() == 20, "the quiet writer got all of its turns");
    check(greedy_writes.load() > 0, "the busy writer also made progress");
    check(counter_value(*database) == greedy_writes.load() + polite_writes.load(),
          "the total matches exactly what both threads did");
}

void test_a_failed_write_does_not_stop_the_queue() {
    section("a write that throws");

    auto database = open_database();
    database->write(increment);

    check(refuses([&database] {
              database->write([](Transaction& transaction) {
                  increment(transaction);
                  throw StoreError("the work failed after writing");
              });
          }),
          "the exception reaches the caller unchanged");

    check(counter_value(*database) == 1, "the failed write was rolled back");

    // The important part: the queue must still be moving. A gate that stops
    // on the first error freezes the whole application.
    database->write(increment);
    check(counter_value(*database) == 2, "the next write went through");

    const auto statistics = database->statistics();
    check(statistics.failed == 1, "the failure was counted");
    check(statistics.completed == 2, "and was not counted as a success");
    check(database->waiting() == 0, "the failed writer released its turn");
}

void test_giving_up_rather_than_waiting() {
    section("giving up rather than waiting");

    auto database = open_database();

    std::promise<void> started;
    std::future<void> has_started = started.get_future();

    // A long write holds the gate. Background work should rather be skipped
    // than delay a person, so it asks with a short patience.
    std::thread slow([&database, &started] {
        database->write([&started](Transaction& transaction) {
            started.set_value();
            std::this_thread::sleep_for(250ms);
            increment(transaction);
        });
    });

    has_started.wait();
    const auto begin = std::chrono::steady_clock::now();
    const bool wrote = database->write_within(increment, 30ms);
    const auto waited = std::chrono::steady_clock::now() - begin;

    check(!wrote, "the impatient writer gave up");
    check(waited < 200ms, "it gave up near its own deadline, not the other writer's");

    slow.join();

    check(counter_value(*database) == 1, "the long write still landed");
    check(database->statistics().abandoned == 1, "the abandonment was counted");

    // An abandoned ticket must not hold the queue for everyone behind it.
    check(database->write_within(increment, 1s), "the queue moved on afterwards");
    check(counter_value(*database) == 2, "and that write landed");
    check(database->waiting() == 0, "nobody is left waiting");
}

void test_reads_are_not_blocked_by_each_other() {
    section("concurrent reads");

    auto database = open_database();
    for (int step = 0; step < 5; ++step) {
        database->write(increment);
    }

    std::atomic<int> readers_done{0};
    std::vector<std::thread> readers;
    readers.reserve(6);
    for (int index = 0; index < 6; ++index) {
        readers.emplace_back([&database, &readers_done] {
            for (int step = 0; step < 50; ++step) {
                if (counter_value(*database) >= 5) {
                    readers_done.fetch_add(1);
                }
            }
        });
    }

    // Writes continue while the readers run. A reader must never see a
    // half-applied transaction.
    std::thread writer([&database] {
        for (int step = 0; step < 20; ++step) {
            database->write(increment);
        }
    });

    for (auto& reader : readers) {
        reader.join();
    }
    writer.join();

    check(readers_done.load() == 300, "every read saw a consistent value");
    check(counter_value(*database) == 25, "and the writes all landed");
}

void test_the_work_cannot_forget_to_commit() {
    section("commit is not the caller's job");

    auto database = open_database();

    // The work receives a transaction that is already open and never commits
    // it. The gate commits on return and rolls back on a throw, so there is no
    // way for a caller to forget either one.
    database->write([](Transaction& transaction) {
        Row row;
        row.set("id", Value::text("e1"));
        row.set("note", Value::text("written without an explicit commit"));
        transaction.insert(kEntries, row);
    });

    std::size_t entries = 0;
    database->read([&entries](const Store& store) { entries = store.count(kEntries); });
    check(entries == 1, "the write was committed by the gate");

    check(refuses([&database] { database->write(nullptr); }),
          "a write with nothing to do is refused");
    check(refuses([&database] { database->read(nullptr); }),
          "a read with nothing to do is refused");
}

}  // namespace

int main() {
    test_the_gate_is_shut_until_opened();
    test_a_failed_open_leaves_it_closed();
    test_no_lost_updates();
    test_no_starvation();
    test_a_failed_write_does_not_stop_the_queue();
    test_giving_up_rather_than_waiting();
    test_reads_are_not_blocked_by_each_other();
    test_the_work_cannot_forget_to_commit();
    return report();
}
