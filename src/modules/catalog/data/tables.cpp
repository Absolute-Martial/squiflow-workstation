#include "modules/catalog/data/tables.hpp"
namespace squiflow::modules::catalog::tables {
std::vector<engine::Migration> migrations() {
    engine::Migration m;
    m.number = kFirstMigration;
    m.name   = "catalog tables";
    m.schema = [](engine::Store& store) { store.define_table(kProduct, "id"); };
    return {m};
}
}  // namespace squiflow::modules::catalog::tables
