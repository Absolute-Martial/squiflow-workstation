#include "modules/catalog/data/repository.hpp"
namespace squiflow::modules::catalog::data {
namespace { void upsert(engine::Transaction& tx, const char* table, const std::string& key, const engine::Row& row) { if (!tx.replace(table, key, row)) tx.insert(table, row); } }
void save_product(engine::Transaction& tx, const Product& p) { upsert(tx, tables::kProduct, p.id, to_row(p)); }
}  // namespace squiflow::modules::catalog::data
