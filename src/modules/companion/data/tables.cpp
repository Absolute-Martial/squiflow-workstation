#include "modules/companion/data/tables.hpp"

namespace squiflow::modules::companion::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "companion tables";
    first.schema = [](engine::Store& store) {
        store.define_table(kTask, "id");
        store.define_table(kEvent, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::companion::tables
