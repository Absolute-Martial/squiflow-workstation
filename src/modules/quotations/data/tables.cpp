#include "modules/quotations/data/tables.hpp"

namespace squiflow::modules::quotations::tables {

std::vector<engine::Migration> migrations() {
    engine::Migration first;
    first.number = kFirstMigration;
    first.name = "quotations tables";
    first.schema = [](engine::Store& store) {
        // Three tables, because a revision is frozen when it is issued and its
        // lines have to freeze with it. One table keyed by quotation would
        // make revising a quotation rewrite paper the customer already holds.
        store.define_table(kQuotation, "id");
        store.define_table(kRevision, "id");
        store.define_table(kLine, "id");
    };
    return {first};
}

}  // namespace squiflow::modules::quotations::tables
