#include "modules/orders/data/tables.hpp"

namespace squiflow::modules::orders::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "orders tables";
    // Schema only, no data half: this migration creates empty tables and has
    // nothing to backfill. define_table is idempotent, which the runner
    // requires because the schema half can run twice after a crash.
    first.schema = [](engine::Store& store) {
        store.define_table(kOrder, "id");
        store.define_table(kOrderLine, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::orders::tables
