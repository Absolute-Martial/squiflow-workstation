#include "modules/sourcing/data/tables.hpp"

namespace squiflow::modules::sourcing::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "sourcing tables";
    first.schema = [](engine::Store& store) {
        // Separate tables keep three different facts separate: who supplies,
        // what the material is called, and what one receipt actually cost.
        // None of them is an inventory balance.
        store.define_table(kSupplier, "id");
        store.define_table(kMaterial, "id");
        store.define_table(kPurchase, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::sourcing::tables
