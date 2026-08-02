#pragma once

// Schema migrations.
//
// Numbering is global across every module, not per module. Two modules that
// each number their own migrations produce two migration 4s, and the day one
// of them touches a table the other created, the order they ran in decides
// whether the shop's data survives. A single ascending sequence removes the
// question.
//
// A migration has two halves, and the split is deliberate:
//
//   schema  runs outside a transaction and defines tables. It must be
//           idempotent, because a crash before the data half commits leaves
//           the schema half already done and the migration will run again.
//   data    runs inside a transaction, together with the record that says
//           this migration was applied. Either both land or neither does, so
//           a power cut mid-migration cannot produce a database that claims
//           to be at a version it never reached.

#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"

namespace squiflow::engine {

struct Migration {
    int number{0};
    std::string name{};
    std::function<void(Store&)> schema{};
    std::function<void(Transaction&)> data{};
};

struct MigrationOutcome {
    std::vector<int> applied{};
    int version{0};
    bool already_current{false};
};

class MigrationRunner {
public:
    // The clock is injected so a test can assert on the recorded time instead
    // of hoping the wall clock cooperates.
    using Clock = std::function<std::int64_t()>;

    explicit MigrationRunner(Clock clock);

    void add(Migration migration);

    std::size_t size() const;
    int highest_known() const;

    // Prepares the bookkeeping table. Safe to call repeatedly; called by run.
    void prepare(Store& store) const;

    int applied_version(const Store& store) const;
    std::vector<int> pending(const Store& store) const;
    std::vector<Row> history(const Store& store) const;

    MigrationOutcome run(Store& store) const;

    static const std::string& table_name();

private:
    std::map<int, Migration> migrations_{};
    Clock clock_;
};

}  // namespace squiflow::engine
