#pragma once

// Reads are templates over Store or Transaction so a workflow can see rows it
// has just written before commit, while lookup remains a plain local read.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/sourcing/data/tables.hpp"
#include "modules/sourcing/domain/sourcing.hpp"

namespace squiflow::modules::sourcing::data {

template <typename Reader>
std::optional<SupplierProfile> find_supplier(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kSupplier, id);
    return row ? std::optional<SupplierProfile>{supplier_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<Material> find_material(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kMaterial, id);
    return row ? std::optional<Material>{material_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<Purchase> find_purchase(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kPurchase, id);
    return row ? std::optional<Purchase>{purchase_from_row(*row)} : std::nullopt;
}

// Exact name lookup is used by the record-purchase workflow to reuse a named
// material rather than creating a second spelling-identical memory.
template <typename Reader>
std::optional<Material> find_material_by_name(const Reader& reader, const std::string& name) {
    engine::Query query{tables::kMaterial};
    query.where_equals("name", engine::Value::text(name));
    query.order_by("id");
    query.take(1);
    const std::vector<engine::Row> rows = reader.select(query);
    return rows.empty() ? std::nullopt
                        : std::optional<Material>{material_from_row(rows.front())};
}

template <typename Reader>
std::vector<SupplierProfile> all_suppliers(const Reader& reader) {
    engine::Query query{tables::kSupplier};
    query.order_by("id");
    std::vector<SupplierProfile> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(supplier_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<Material> all_materials(const Reader& reader) {
    engine::Query query{tables::kMaterial};
    query.order_by("name");
    query.order_by("id");
    std::vector<Material> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(material_from_row(row));
    }
    return result;
}

// The one lookup primitive behind material history, supplier history, and the
// outstanding screen. Empty filters mean "all". Purchase date and then id are
// descending so equal timestamps are deterministic on every device.
template <typename Reader>
std::vector<Purchase> lookup_purchases(const Reader& reader,
                                       const std::string& material_id,
                                       const std::string& supplier_id,
                                       bool outstanding_only,
                                       std::size_t limit) {
    engine::Query query{tables::kPurchase};
    if (!material_id.empty()) {
        query.where_equals("material_id", engine::Value::text(material_id));
    }
    if (!supplier_id.empty()) {
        query.where_equals("supplier_id", engine::Value::text(supplier_id));
    }
    if (outstanding_only) {
        query.where_equals("state", engine::Value::integer(
            static_cast<std::int64_t>(PurchaseState::Owed)));
    }
    query.order_by("purchased_at", engine::SortOrder::Descending);
    query.order_by("id", engine::SortOrder::Descending);
    query.take(limit);

    std::vector<Purchase> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(purchase_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<Purchase> purchases_for_material(const Reader& reader,
                                             const std::string& material_id,
                                             std::size_t limit) {
    return lookup_purchases(reader, material_id, {}, false, limit);
}

template <typename Reader>
std::vector<Purchase> purchases_for_supplier(const Reader& reader,
                                             const std::string& supplier_id,
                                             std::size_t limit) {
    return lookup_purchases(reader, {}, supplier_id, false, limit);
}

template <typename Reader>
std::vector<Purchase> outstanding_purchases(const Reader& reader, std::size_t limit) {
    return lookup_purchases(reader, {}, {}, true, limit);
}

void save_supplier(engine::Transaction& transaction, const SupplierProfile& supplier);
void save_material(engine::Transaction& transaction, const Material& material);
void save_purchase(engine::Transaction& transaction, const Purchase& purchase);

}  // namespace squiflow::modules::sourcing::data
