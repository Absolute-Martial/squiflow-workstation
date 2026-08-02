#include "modules/administration/data/tables.hpp"

namespace squiflow::modules::administration::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "administration tables";
    first.schema = [](engine::Store& store) {
        store.define_table(kPerson, "id");

        // One row per granted right, keyed by person and right name together.
        // Granting twice is then the same row rather than a duplicate, and a
        // revoke is a delete rather than a flag that can disagree with itself.
        store.define_table(kPersonRight, "id");

        store.define_table(kDevice, "id");
        store.define_table(kSetting, "key");

        // Only switched-off modules appear here. An empty table means every
        // module is on, which is the state a new shop should start in without
        // anyone having to write twelve rows to say so.
        store.define_table(kModuleState, "module");

        store.define_table(kAudit, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::administration::tables
