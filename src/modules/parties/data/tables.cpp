#include "modules/parties/data/tables.hpp"

namespace squiflow::modules::parties::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "parties tables";
    first.schema = [](engine::Store& store) {
        store.define_table(kParty,   "id");
        store.define_table(kContact, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::parties::tables
