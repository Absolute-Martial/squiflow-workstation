#pragma once
#include <optional>
#include <string>
#include <vector>
#include "engine/storage/store.hpp"
#include "modules/catalog/data/tables.hpp"
#include "modules/catalog/domain/product.hpp"
namespace squiflow::modules::catalog::data {
template <typename Reader>
std::optional<Product> find_product(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kProduct, id);
    if (!row) return std::nullopt;
    return product_from_row(*row);
}
template <typename Reader>
std::vector<Product> all_products(const Reader& reader) {
    engine::Query q{tables::kProduct};
    q.order_by("name");
    std::vector<Product> result;
    for (const auto& row : reader.select(q)) result.push_back(product_from_row(row));
    return result;
}
void save_product(engine::Transaction& transaction, const Product& product);
}  // namespace squiflow::modules::catalog::data
