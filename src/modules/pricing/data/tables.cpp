#include "modules/pricing/data/tables.hpp"

namespace squiflow::modules::pricing::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "pricing tables";
    // Schema only, no data half: this migration creates empty tables and has
    // nothing to backfill. define_table is idempotent, which the runner
    // requires because the schema half can run twice after a crash.
    first.schema = [](engine::Store& store) {
        store.define_table(kRate, "id");
        store.define_table(kDefaultRate, "product_id");
        store.define_table(kRateOverride, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::pricing::tables
