#include "modules/agreements/data/tables.hpp"

namespace squiflow::modules::agreements::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "agreements tables";
    first.schema = [](engine::Store& store) {
        // Two tables. A cap is consumed against a line rather than against the
        // agreement, and the same product may be listed twice under two agreed
        // names at two rates, so lines cannot collapse into the head record.
        store.define_table(kAgreement, "id");
        store.define_table(kLine, "id");
    };
    engine::Migration consumption;
    consumption.number = kConsumptionMigration;
    consumption.name = "agreement consumption evidence";
    consumption.schema = [](engine::Store& store) {
        store.define_table(kConsumption, "id");
    };
    return {first, consumption};
}

}  // namespace squiflow::modules::agreements::tables
