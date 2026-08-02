#include "modules/pricing/data/repository.hpp"

namespace squiflow::modules::pricing::data {
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

void save_rate(engine::Transaction& transaction, const Rate& rate) {
    validate(rate);
    upsert(transaction, tables::kRate, rate.id, to_row(rate));
}

void save_default_rate(engine::Transaction& transaction, const DefaultRate& rate) {
    validate(rate);
    upsert(transaction, tables::kDefaultRate, rate.product_id, to_row(rate));
}

void save_override(engine::Transaction& transaction, const RateOverride& override_) {
    validate(override_);
    upsert(transaction, tables::kRateOverride, override_.id, to_row(override_));
}

bool remove_rate(engine::Transaction& transaction, const std::string& id) {
    return transaction.remove(tables::kRate, id);
}

}  // namespace squiflow::modules::pricing::data
