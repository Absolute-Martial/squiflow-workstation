#include "modules/orders/data/repository.hpp"

namespace squiflow::modules::orders::data {
namespace {

// Transaction::replace overwrites an existing row and reports whether one was
// there; it does not insert. Every save in this module is either a correction
// of an existing row or a first write, and the caller does not want to know
// which, so the two calls are paired here once instead of in each handler.
void upsert(engine::Transaction& transaction,
            const char* table,
            const std::string& key,
            const engine::Row& row) {
    if (!transaction.replace(table, key, row)) {
        transaction.insert(table, row);
    }
}

}  // namespace

void save_order(engine::Transaction& transaction, const Order& order) {
    validate(order);
    upsert(transaction, tables::kOrder, order.id, to_row(order));
}

void save_line(engine::Transaction& transaction, const OrderLine& line) {
    validate(line);
    upsert(transaction, tables::kOrderLine, line.id, to_row(line));
}

}  // namespace squiflow::modules::orders::data
