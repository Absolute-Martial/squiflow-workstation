#include "modules/sourcing/data/repository.hpp"

namespace squiflow::modules::sourcing::data {
namespace {

void upsert(engine::Transaction& transaction,
            const char* table,
            const std::string& key,
            const engine::Row& row) {
    if (!transaction.replace(table, key, row)) {
        transaction.insert(table, row);
    }
}

}  // namespace

void save_supplier(engine::Transaction& transaction, const SupplierProfile& supplier) {
    validate(supplier);
    upsert(transaction, tables::kSupplier, supplier.id, to_row(supplier));
}

void save_material(engine::Transaction& transaction, const Material& material) {
    validate(material);
    upsert(transaction, tables::kMaterial, material.id, to_row(material));
}

void save_purchase(engine::Transaction& transaction, const Purchase& purchase) {
    validate(purchase);
    upsert(transaction, tables::kPurchase, purchase.id, to_row(purchase));
}

}  // namespace squiflow::modules::sourcing::data
