#include "modules/jobs/data/tables.hpp"

namespace squiflow::modules::jobs::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "jobs tables";
    first.schema = [](engine::Store& store) {
        store.define_table(kJob, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::jobs::tables
