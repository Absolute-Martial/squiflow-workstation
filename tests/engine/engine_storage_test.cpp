// Phase 3.1 and 3.2: the storage seam and its in-memory implementation.
//
// These are behaviour tests, not shape tests. Every rule the SQLite
// implementation will have to obey is executed here against the in-memory one,
// so that when the SQLite version arrives there is something to compare it to.

#include <string>
#include <vector>

#include "engine/storage/memory_store.hpp"
#include "engine/storage/store.hpp"
#include "support/check.hpp"

namespace {

using squiflow::engine::Blob;
using squiflow::engine::Comparison;
using squiflow::engine::MemoryStore;
using squiflow::engine::Query;
using squiflow::engine::Row;
using squiflow::engine::SortOrder;
using squiflow::engine::StoreError;
using squiflow::engine::Value;
using squiflow::engine::ValueKind;
using squiflow::testing::check;
using squiflow::testing::report;
using squiflow::testing::section;

const std::string kParties = "parties";

// A store is deliberately neither copyable nor movable: two handles to one
// store, or a store that moves out from under an open transaction, are both
// bugs waiting to happen. So the helper prepares a store in place rather than
// returning one.
void prepare(MemoryStore& store) {
    store.define_table(kParties, "id");
}

Row party(const std::string& id, const std::string& name, std::int64_t owed) {
    Row row;
    row.set("id", Value::text(id));
    row.set("name", Value::text(name));
    row.set("owed_minor", Value::integer(owed));
    return row;
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

void test_values() {
    section("values");

    check(Value{}.is_null(), "a default value is null");
    check(Value::null().kind() == ValueKind::Null, "an explicit null is null");
    check(Value::integer(42).as_integer() == 42, "an integer round-trips");
    check(Value::text("flex").text_or("") == "flex", "text round-trips");
    check(Value::boolean(true).integer_or(-1) == 1, "true is stored as one");
    check(Value::boolean(false).boolean_or(true) == false, "false reads back as false");

    // SQLite has no boolean type. Inventing one here would mean the two
    // implementations disagree about what was stored.
    check(Value::boolean(true).kind() == ValueKind::Integer,
          "a boolean is an integer, because the database has no boolean");

    check(!Value::text("7").as_integer().has_value(), "text is not silently read as a number");
    check(Value::integer(7).as_real().value_or(0.0) == 7.0, "an integer is readable as a real");

    check(Value::integer(1).compare(Value::integer(2)) < 0, "integers order");
    check(Value::text("a").compare(Value::text("b")) < 0, "text orders");
    check(Value::null().compare(Value::integer(0)) < 0, "null sorts before every number");
    check(Value::integer(5).compare(Value::text("5")) < 0, "a number sorts before text");
    check(Value::integer(5) != Value::text("5"), "a number is never equal to its text form");

    // Two values beyond the exact range of a double must not compare equal.
    // Comparing through double is the ordinary way this goes wrong.
    const std::int64_t big = 9007199254740993;
    check(Value::integer(big).compare(Value::integer(big + 2)) < 0,
          "large integers compare exactly, not through a double");

    const Blob bytes{1, 2, 3};
    check(Value::binary(bytes).as_binary() != nullptr && *Value::binary(bytes).as_binary() == bytes,
          "binary round-trips");
}

void test_rows() {
    section("rows");

    Row row;
    row.set("id", Value::text("p1")).set("name", Value::text("Ramesh"));
    check(row.size() == 2, "two fields were set");
    check(row.has("name"), "a set field is present");
    check(!row.has("phone"), "an unset field is absent");

    // An absent column reads as null rather than throwing. An absent value and
    // a stored null mean the same thing to a reader.
    check(row.get("phone").is_null(), "an absent column reads as null");
    check(!row.lookup("phone").has_value(), "lookup distinguishes absent from null");

    row.set("name", Value::text("Ramesh Shrestha"));
    check(row.size() == 2, "setting an existing column replaces rather than appends");
    check(row.get("name").text_or("") == "Ramesh Shrestha", "the replacement took effect");

    check(row.columns().front() == "id", "field order is preserved");

    Row patch;
    patch.set("phone", Value::text("98..."));
    row.merge(patch);
    check(row.size() == 3 && row.has("phone"), "merge adds new fields");

    check(row.erase("phone") && !row.has("phone"), "erase removes a field");
    check(!row.erase("phone"), "erasing an absent field reports that it was absent");
}

void test_table_definition() {
    section("table definition");

    MemoryStore store;
    check(!store.has_table(kParties), "a table does not exist until it is defined");
    store.define_table(kParties, "id");
    check(store.has_table(kParties), "a defined table exists");

    store.define_table(kParties, "id");
    check(store.tables().size() == 1, "defining the same table twice is not an error");

    check(refuses([&store] { store.define_table(kParties, "party_id"); }),
          "redefining a table with a different key is refused");
    check(refuses([&store] { store.define_table("broken", ""); }),
          "a table with no key column is refused");
    check(refuses([&store] { store.count("missing"); }),
          "reading an undefined table is refused rather than returning nothing");
}

void test_writes() {
    section("writes");

    MemoryStore store;
    prepare(store);

    auto transaction = store.begin();
    transaction->insert(kParties, party("p1", "Ramesh", 0));
    transaction->insert(kParties, party("p2", "Sita", 25000));
    check(transaction->select(Query{kParties}).size() == 2, "a transaction sees its own writes");
    check(store.count(kParties) == 0, "nothing is visible outside until commit");
    transaction->commit();

    check(store.count(kParties) == 2, "committed rows are visible");
    check(store.committed_transactions() == 1, "the commit was counted");

    auto second = store.begin();
    check(refuses([&second] { second->insert(kParties, party("p1", "Duplicate", 0)); }),
          "a duplicate key is refused");

    Row missing;
    missing.set("name", Value::text("No key"));
    check(refuses([&second, &missing] { second->insert(kParties, missing); }),
          "a row with no key column is refused");

    Row patch;
    patch.set("owed_minor", Value::integer(50000));
    check(second->update(kParties, "p1", patch), "an update reports that it found the row");
    check(!second->update(kParties, "absent", patch), "an update reports a missing row");

    Row rekey;
    rekey.set("id", Value::text("p9"));
    check(refuses([&second, &rekey] { second->update(kParties, "p1", rekey); }),
          "an update may not change the key");

    check(second->remove(kParties, "p2"), "remove reports that it found the row");
    check(!second->remove(kParties, "p2"), "removing twice reports the second as absent");
    second->commit();

    const auto found = store.find(kParties, "p1");
    check(found.has_value(), "the surviving row is found");
    check(found->get("owed_minor").integer_or(0) == 50000, "the update was applied");
    check(found->get("name").text_or("") == "Ramesh", "an update merges rather than replaces");
    check(store.count(kParties) == 1, "the removal was applied");
}

void test_replace() {
    section("replace");

    MemoryStore store;
    prepare(store);
    auto transaction = store.begin();
    transaction->insert(kParties, party("p1", "Ramesh", 100));
    transaction->commit();

    Row whole;
    whole.set("id", Value::text("p1"));
    whole.set("name", Value::text("Ramesh"));

    auto second = store.begin();
    check(second->replace(kParties, "p1", whole), "replace found the row");
    second->commit();

    const auto found = store.find(kParties, "p1");
    check(found.has_value() && !found->has("owed_minor"),
          "replace drops fields that update would have kept");
}

void test_rollback() {
    section("rollback");

    MemoryStore store;
    prepare(store);
    auto seed = store.begin();
    seed->insert(kParties, party("p1", "Ramesh", 100));
    seed->commit();

    {
        auto transaction = store.begin();
        transaction->insert(kParties, party("p2", "Sita", 200));
        Row patch;
        patch.set("owed_minor", Value::integer(999));
        transaction->update(kParties, "p1", patch);
        transaction->rollback();
    }

    check(store.count(kParties) == 1, "the inserted row was undone");
    check(store.find(kParties, "p1")->get("owed_minor").integer_or(0) == 100,
          "the updated row was restored exactly");
    check(store.rolled_back_transactions() == 1, "the rollback was counted");

    // The case that matters: an early return or a thrown exception must not
    // leave half a record behind.
    {
        auto abandoned = store.begin();
        abandoned->insert(kParties, party("p3", "Abandoned", 1));
    }
    check(store.count(kParties) == 1, "a transaction destroyed without commit rolls back");
    check(!store.writing(), "an abandoned transaction releases the writer");

    auto after = store.begin();
    check(after->open(), "a new transaction can be opened afterwards");
    after->rollback();
    check(refuses([&after] { after->insert(kParties, party("p4", "Closed", 0)); }),
          "writing through a closed transaction is refused");
}

void test_single_writer() {
    section("single writer");

    MemoryStore store;
    prepare(store);
    auto first = store.begin();
    check(store.writing(), "the store reports that a write is in progress");
    check(refuses([&store] { auto second = store.begin(); }),
          "a second writer is refused; there is exactly one");

    // Reads do not need the writer, so the interface stays usable while a
    // write is open.
    check(store.count(kParties) == 0, "reading is allowed while a write is open");

    check(refuses([&store] { store.define_table("other", "id"); }),
          "a table may not be defined mid-transaction");

    first->commit();
    check(!store.writing(), "committing releases the writer");
    auto again = store.begin();
    check(again->open(), "the next writer may start");
}

void test_queries() {
    section("queries");

    MemoryStore store;
    prepare(store);
    auto transaction = store.begin();
    transaction->insert(kParties, party("p1", "Ramesh Printers", 500));
    transaction->insert(kParties, party("p2", "Sita Traders", 0));
    transaction->insert(kParties, party("p3", "Ramesh Stationery", 1500));
    Row unpriced;
    unpriced.set("id", Value::text("p4"));
    unpriced.set("name", Value::text("Unknown"));
    transaction->insert(kParties, unpriced);
    transaction->commit();

    check(store.select(Query{kParties}).size() == 4, "an unfiltered query returns everything");

    Query owing{kParties};
    owing.where("owed_minor", Comparison::Greater, Value::integer(0));
    check(store.select(owing).size() == 2, "a greater-than filter works");

    // The rule that protects money: a missing value is not zero. Treating it
    // as zero is how an unpriced line silently becomes a free one.
    Query not_owing{kParties};
    not_owing.where("owed_minor", Comparison::LessOrEqual, Value::integer(0));
    check(store.select(not_owing).size() == 1,
          "a null never satisfies an ordering comparison, so the unpriced row is excluded");

    Query named{kParties};
    named.where("name", Comparison::StartsWith, Value::text("Ramesh"));
    check(store.select(named).size() == 2, "starts-with works");

    Query contains{kParties};
    contains.where("name", Comparison::Contains, Value::text("Trader"));
    check(store.select(contains).size() == 1, "contains works");

    Query both{kParties};
    both.where("name", Comparison::StartsWith, Value::text("Ramesh"))
        .where("owed_minor", Comparison::Greater, Value::integer(1000));
    check(store.select(both).size() == 1, "conditions combine with and");

    Query descending{kParties};
    descending.order_by("owed_minor", SortOrder::Descending);
    const auto sorted = store.select(descending);
    check(sorted.size() == 4 && sorted.front().get("id").text_or("") == "p3",
          "descending order puts the largest first");
    check(sorted.back().get("id").text_or("") == "p4",
          "a null sorts last when the order is descending");

    Query paged{kParties};
    paged.order_by("id").skip(1).take(2);
    const auto page = store.select(paged);
    check(page.size() == 2, "a page is the requested size");
    check(page.front().get("id").text_or("") == "p2", "the offset was applied before the limit");

    Query beyond{kParties};
    beyond.skip(99);
    check(store.select(beyond).empty(), "an offset past the end returns nothing, not an error");

    // First sort named is the primary one. Getting this backwards is a bug
    // nobody notices until a list is sorted by the wrong column.
    Query multi{kParties};
    multi.order_by("name").order_by("id", SortOrder::Descending);
    const auto ordered = store.select(multi);
    check(ordered.front().get("name").text_or("") == "Ramesh Printers",
          "the first sort named is the primary sort");

    check(refuses([&store] { store.select(Query{"missing"}); }),
          "querying an undefined table is refused");
}

}  // namespace

int main() {
    test_values();
    test_rows();
    test_table_definition();
    test_writes();
    test_replace();
    test_rollback();
    test_single_writer();
    test_queries();
    return report();
}
