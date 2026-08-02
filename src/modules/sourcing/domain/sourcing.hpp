#pragma once

// Sourcing is the shop's memory of where material came from. It is not stock,
// purchasing, or a supplier ledger: quantities describe one historical
// purchase and are never accumulated into an inventory balance.
//
// A supplier is already a party. The profile below is keyed by that same party
// id and stores only sourcing-specific knowledge, so names, addresses, and
// contacts never acquire a second owner here.

#include <cstdint>
#include <string>

#include "engine/storage/store.hpp"

namespace squiflow::modules::sourcing {

enum class SupplierKind : std::uint8_t {
    LocalDealer = 0,
    Importer = 1,
};

enum class PurchaseState : std::uint8_t {
    Owed = 0,
    Paid = 1,
};

const char* to_string(SupplierKind kind) noexcept;
const char* to_string(PurchaseState state) noexcept;

struct SupplierProfile {
    // The id of the party this profile extends. There is deliberately no
    // second supplier id and no copied display name.
    std::string id{};
    SupplierKind kind{SupplierKind::LocalDealer};
    std::string supplies{};
    std::string reliability_notes{};
    std::int64_t lead_time_days{0};
    std::string sourcing_notes{};
    std::int64_t created_at{0};
    std::string created_by{};
    std::int64_t updated_at{0};
    std::string updated_by{};
};

struct Material {
    std::string id{};
    std::string name{};
    std::string description{};
    std::int64_t created_at{0};
    std::string created_by{};
    std::int64_t updated_at{0};
    std::string updated_by{};
};

struct Purchase {
    std::string id{};
    std::string supplier_id{};
    std::string material_id{};
    std::int64_t purchased_at{0};

    // Quantity uses the engine's thousandths scale. It describes this receipt
    // only; nothing in this module adds it to or removes it from stock.
    std::int64_t quantity_scaled{0};

    // Total actually paid or owed for this purchase, in minor currency units.
    // The selling-price resolver never reads this field.
    std::int64_t total_cost_minor{0};
    std::string bill_file_ref{};

    PurchaseState state{PurchaseState::Owed};
    std::int64_t settled_at{0};
    std::string settled_by{};
    std::string settlement_note{};

    std::int64_t created_at{0};
    std::string created_by{};
};

constexpr bool is_outstanding(const Purchase& purchase) noexcept {
    return purchase.state == PurchaseState::Owed;
}

// Moves one owed purchase to paid and records the evidence. Refuses to rewrite
// an already-paid purchase: correction must be explicit rather than silently
// changing when or by whom a debt was cleared.
void settle_purchase(Purchase& purchase,
                     std::int64_t settled_at,
                     const std::string& settled_by,
                     const std::string& note);

void validate(const SupplierProfile& supplier);
void validate(const Material& material);
void validate(const Purchase& purchase);

engine::Row to_row(const SupplierProfile& supplier);
engine::Row to_row(const Material& material);
engine::Row to_row(const Purchase& purchase);

SupplierProfile supplier_from_row(const engine::Row& row);
Material material_from_row(const engine::Row& row);
Purchase purchase_from_row(const engine::Row& row);

}  // namespace squiflow::modules::sourcing
