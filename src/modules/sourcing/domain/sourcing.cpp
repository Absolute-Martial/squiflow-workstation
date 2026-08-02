#include "modules/sourcing/domain/sourcing.hpp"

#include "modules/context.hpp"

namespace squiflow::modules::sourcing {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

SupplierKind supplier_kind_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return SupplierKind::LocalDealer;
        case 1: return SupplierKind::Importer;
        default: return SupplierKind::LocalDealer;
    }
}

// Damaged payment state reads as owed. Losing a debt is the unsafe direction;
// showing a debt that needs checking is visible and recoverable.
PurchaseState purchase_state_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return PurchaseState::Owed;
        case 1: return PurchaseState::Paid;
        default: return PurchaseState::Owed;
    }
}

void validate_creation(std::int64_t created_at,
                       const std::string& created_by,
                       const char* complaint) {
    if (created_at <= 0 || blank(created_by)) {
        throw RuleViolation(complaint);
    }
}

void validate_update(std::int64_t created_at,
                     std::int64_t updated_at,
                     const std::string& updated_by,
                     const char* complaint) {
    if (updated_at < created_at || blank(updated_by)) {
        throw RuleViolation(complaint);
    }
}

}  // namespace

const char* to_string(SupplierKind kind) noexcept {
    switch (kind) {
        case SupplierKind::LocalDealer: return "local dealer";
        case SupplierKind::Importer: return "importer";
    }
    return "?";
}

const char* to_string(PurchaseState state) noexcept {
    switch (state) {
        case PurchaseState::Owed: return "owed";
        case PurchaseState::Paid: return "paid";
    }
    return "?";
}

void settle_purchase(Purchase& purchase,
                     std::int64_t settled_at,
                     const std::string& settled_by,
                     const std::string& note) {
    if (purchase.state != PurchaseState::Owed) {
        throw RuleViolation("That purchase is already settled.");
    }
    if (settled_at <= 0 || settled_at < purchase.purchased_at) {
        throw RuleViolation("A purchase cannot be settled before it was made.");
    }
    if (blank(settled_by)) {
        throw RuleViolation("Settling a purchase must record who cleared it.");
    }

    purchase.state = PurchaseState::Paid;
    purchase.settled_at = settled_at;
    purchase.settled_by = settled_by;
    purchase.settlement_note = note;
}

void validate(const SupplierProfile& supplier) {
    if (supplier.id.empty()) {
        throw RuleViolation("This supplier has no party record to extend.");
    }
    if (supplier.lead_time_days < 0) {
        throw RuleViolation("A supplier lead time cannot be negative.");
    }
    validate_creation(supplier.created_at, supplier.created_by,
                      "A supplier profile must record when and by whom it was created.");
    validate_update(supplier.created_at, supplier.updated_at, supplier.updated_by,
                    "A supplier profile update must record when and by whom it happened.");
}

void validate(const Material& material) {
    if (material.id.empty()) {
        throw RuleViolation("This material has no record to be saved under.");
    }
    if (blank(material.name)) {
        throw RuleViolation("A material must have a name.");
    }
    validate_creation(material.created_at, material.created_by,
                      "A material must record when and by whom it was created.");
    validate_update(material.created_at, material.updated_at, material.updated_by,
                    "A material update must record when and by whom it happened.");
}

void validate(const Purchase& purchase) {
    if (purchase.id.empty()) {
        throw RuleViolation("This purchase has no record to be saved under.");
    }
    if (blank(purchase.supplier_id)) {
        throw RuleViolation("A purchase must name its supplier.");
    }
    if (blank(purchase.material_id)) {
        throw RuleViolation("A purchase must name its material.");
    }
    if (purchase.purchased_at <= 0) {
        throw RuleViolation("A purchase must record when it was made.");
    }
    if (purchase.quantity_scaled <= 0) {
        throw RuleViolation("A purchase quantity must be greater than zero.");
    }
    if (purchase.total_cost_minor < 0) {
        throw RuleViolation("A purchase cost cannot be negative.");
    }
    validate_creation(purchase.created_at, purchase.created_by,
                      "A purchase must record when and by whom it was logged.");
    if (purchase.purchased_at > purchase.created_at) {
        throw RuleViolation("A purchase cannot be dated after it was logged.");
    }

    switch (purchase.state) {
        case PurchaseState::Owed:
            if (purchase.settled_at != 0 || !blank(purchase.settled_by) ||
                !blank(purchase.settlement_note)) {
                throw RuleViolation("A purchase still owed cannot carry settlement evidence.");
            }
            break;
        case PurchaseState::Paid:
            if (purchase.settled_at <= 0 || blank(purchase.settled_by)) {
                throw RuleViolation("A paid purchase must record when and by whom it was settled.");
            }
            if (purchase.settled_at < purchase.purchased_at) {
                throw RuleViolation("A purchase cannot be settled before it was made.");
            }
            break;
        default:
            throw RuleViolation("That purchase has a payment state this build does not understand.");
    }
}

engine::Row to_row(const SupplierProfile& supplier) {
    engine::Row row;
    row.set("id", engine::Value::text(supplier.id));
    row.set("kind", engine::Value::integer(static_cast<std::int64_t>(supplier.kind)));
    row.set("supplies", engine::Value::text(supplier.supplies));
    row.set("reliability_notes", engine::Value::text(supplier.reliability_notes));
    row.set("lead_time_days", engine::Value::integer(supplier.lead_time_days));
    row.set("sourcing_notes", engine::Value::text(supplier.sourcing_notes));
    row.set("created_at", engine::Value::integer(supplier.created_at));
    row.set("created_by", engine::Value::text(supplier.created_by));
    row.set("updated_at", engine::Value::integer(supplier.updated_at));
    row.set("updated_by", engine::Value::text(supplier.updated_by));
    return row;
}

engine::Row to_row(const Material& material) {
    engine::Row row;
    row.set("id", engine::Value::text(material.id));
    row.set("name", engine::Value::text(material.name));
    row.set("description", engine::Value::text(material.description));
    row.set("created_at", engine::Value::integer(material.created_at));
    row.set("created_by", engine::Value::text(material.created_by));
    row.set("updated_at", engine::Value::integer(material.updated_at));
    row.set("updated_by", engine::Value::text(material.updated_by));
    return row;
}

engine::Row to_row(const Purchase& purchase) {
    engine::Row row;
    row.set("id", engine::Value::text(purchase.id));
    row.set("supplier_id", engine::Value::text(purchase.supplier_id));
    row.set("material_id", engine::Value::text(purchase.material_id));
    row.set("purchased_at", engine::Value::integer(purchase.purchased_at));
    row.set("quantity_scaled", engine::Value::integer(purchase.quantity_scaled));
    row.set("total_cost_minor", engine::Value::integer(purchase.total_cost_minor));
    row.set("bill_file_ref", engine::Value::text(purchase.bill_file_ref));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(purchase.state)));
    row.set("settled_at", engine::Value::integer(purchase.settled_at));
    row.set("settled_by", engine::Value::text(purchase.settled_by));
    row.set("settlement_note", engine::Value::text(purchase.settlement_note));
    row.set("created_at", engine::Value::integer(purchase.created_at));
    row.set("created_by", engine::Value::text(purchase.created_by));
    return row;
}

SupplierProfile supplier_from_row(const engine::Row& row) {
    SupplierProfile supplier;
    supplier.id = row.get("id").text_or({});
    supplier.kind = supplier_kind_from(row.get("kind").integer_or(0));
    supplier.supplies = row.get("supplies").text_or({});
    supplier.reliability_notes = row.get("reliability_notes").text_or({});
    supplier.lead_time_days = row.get("lead_time_days").integer_or(0);
    supplier.sourcing_notes = row.get("sourcing_notes").text_or({});
    supplier.created_at = row.get("created_at").integer_or(0);
    supplier.created_by = row.get("created_by").text_or({});
    supplier.updated_at = row.get("updated_at").integer_or(0);
    supplier.updated_by = row.get("updated_by").text_or({});
    return supplier;
}

Material material_from_row(const engine::Row& row) {
    Material material;
    material.id = row.get("id").text_or({});
    material.name = row.get("name").text_or({});
    material.description = row.get("description").text_or({});
    material.created_at = row.get("created_at").integer_or(0);
    material.created_by = row.get("created_by").text_or({});
    material.updated_at = row.get("updated_at").integer_or(0);
    material.updated_by = row.get("updated_by").text_or({});
    return material;
}

Purchase purchase_from_row(const engine::Row& row) {
    Purchase purchase;
    purchase.id = row.get("id").text_or({});
    purchase.supplier_id = row.get("supplier_id").text_or({});
    purchase.material_id = row.get("material_id").text_or({});
    purchase.purchased_at = row.get("purchased_at").integer_or(0);
    purchase.quantity_scaled = row.get("quantity_scaled").integer_or(0);
    purchase.total_cost_minor = row.get("total_cost_minor").integer_or(0);
    purchase.bill_file_ref = row.get("bill_file_ref").text_or({});
    purchase.state = purchase_state_from(row.get("state").integer_or(0));
    purchase.settled_at = row.get("settled_at").integer_or(0);
    purchase.settled_by = row.get("settled_by").text_or({});
    purchase.settlement_note = row.get("settlement_note").text_or({});
    purchase.created_at = row.get("created_at").integer_or(0);
    purchase.created_by = row.get("created_by").text_or({});
    return purchase;
}

}  // namespace squiflow::modules::sourcing
