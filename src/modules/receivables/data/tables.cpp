#include "modules/receivables/data/tables.hpp"

namespace squiflow::modules::receivables::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "receivables tables";
    first.schema = [](engine::Store& store) {
        store.define_table(kInvoice, "id");
        store.define_table(kInvoiceLine, "id");
        store.define_table(kPayment, "id");
        store.define_table(kAllocation, "id");
        store.define_table(kCreditAccount, "id");
        store.define_table(kCreditOverride, "id");
        store.define_table(kStatement, "id");
        store.define_table(kStatementEntry, "id");
        store.define_table(kStatementDelivery, "id");
    };

    engine::Migration number_blocks;
    number_blocks.number = kNumberBlockMigration;
    number_blocks.name = "reserved receivables number blocks";
    number_blocks.schema = [](engine::Store& store) {
        store.define_table(kNumberBlock, "id");
    };
    return {first, number_blocks};
}

}  // namespace squiflow::modules::receivables::tables
