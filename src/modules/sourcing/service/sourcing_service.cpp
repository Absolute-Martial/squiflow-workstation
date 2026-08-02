#include "modules/sourcing/service/sourcing_service.hpp"

#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/sourcing/data/repository.hpp"

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

engine::Row fields(const Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw RuleViolation("This request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("sourcing: a write arrived with no session");
    }
    return *call.actor;
}

std::string subject(const Call& call) {
    if (blank(call.record_id)) {
        throw RuleViolation("This request does not identify its record.");
    }
    return call.record_id;
}

std::string optional_text(const engine::Row& row,
                          const std::string& name,
                          const char* complaint,
                          const std::string& fallback = {}) {
    if (!row.has(name)) {
        return fallback;
    }
    const std::string* value = row.get(name).as_text();
    if (value == nullptr) {
        throw RuleViolation(complaint);
    }
    return *value;
}

std::int64_t optional_number(const engine::Row& row,
                             const std::string& name,
                             const char* complaint,
                             std::int64_t fallback) {
    if (!row.has(name)) {
        return fallback;
    }
    const auto value = row.get(name).as_integer();
    if (!value) {
        throw RuleViolation(complaint);
    }
    return *value;
}

bool optional_bool(const engine::Row& row,
                   const std::string& name,
                   const char* complaint,
                   bool fallback) {
    if (!row.has(name)) {
        return fallback;
    }
    const auto value = row.get(name).as_integer();
    if (!value || (*value != 0 && *value != 1)) {
        throw RuleViolation(complaint);
    }
    return *value == 1;
}

SupplierKind supplier_kind_from(const std::string& value) {
    if (value == "local") {
        return SupplierKind::LocalDealer;
    }
    if (value == "importer") {
        return SupplierKind::Importer;
    }
    throw RuleViolation("That supplier kind is not understood.");
}

std::string actor_id(const Call& call) {
    return engine::to_string(actor(call).person);
}

SupplierProfile existing_supplier(const engine::Transaction& transaction,
                                  const std::string& id) {
    const auto supplier = data::find_supplier(transaction, id);
    if (!supplier) {
        throw RuleViolation("That supplier is not on file.");
    }
    validate(*supplier);
    return *supplier;
}

Purchase existing_purchase(const engine::Transaction& transaction, const std::string& id) {
    const auto purchase = data::find_purchase(transaction, id);
    if (!purchase) {
        throw RuleViolation("That purchase is not on file.");
    }
    validate(*purchase);
    return *purchase;
}

}  // namespace

void SourcingService::create_supplier(engine::Transaction& transaction,
                                      const Call& call) const {
    const std::string id = subject(call);
    const engine::Row row = fields(call);
    if (data::find_supplier(transaction, id)) {
        throw RuleViolation("That supplier profile is already on file.");
    }

    const std::int64_t at = clock_();
    SupplierProfile supplier;
    supplier.id = id;
    supplier.kind = supplier_kind_from(optional_text(
        row, "kind", "That supplier kind could not be read as text.", "local"));
    supplier.supplies = optional_text(
        row, "supplies", "What that supplier provides could not be read as text.");
    supplier.reliability_notes = optional_text(
        row, "reliability_notes", "Those reliability notes could not be read as text.");
    supplier.lead_time_days = optional_number(
        row, "lead_time_days", "That lead time could not be read as a number.", 0);
    supplier.sourcing_notes = optional_text(
        row, "sourcing_notes", "Those sourcing notes could not be read as text.");
    supplier.created_at = at;
    supplier.created_by = actor_id(call);
    supplier.updated_at = at;
    supplier.updated_by = supplier.created_by;

    data::save_supplier(transaction, supplier);
}

void SourcingService::update_supplier(engine::Transaction& transaction,
                                      const Call& call) const {
    const std::string id = subject(call);
    const engine::Row row = fields(call);
    SupplierProfile supplier = existing_supplier(transaction, id);

    if (row.has("kind")) {
        supplier.kind = supplier_kind_from(optional_text(
            row, "kind", "That supplier kind could not be read as text."));
    }
    supplier.supplies = optional_text(
        row, "supplies", "What that supplier provides could not be read as text.",
        supplier.supplies);
    supplier.reliability_notes = optional_text(
        row, "reliability_notes", "Those reliability notes could not be read as text.",
        supplier.reliability_notes);
    supplier.lead_time_days = optional_number(
        row, "lead_time_days", "That lead time could not be read as a number.",
        supplier.lead_time_days);
    supplier.sourcing_notes = optional_text(
        row, "sourcing_notes", "Those sourcing notes could not be read as text.",
        supplier.sourcing_notes);
    supplier.updated_at = clock_();
    supplier.updated_by = actor_id(call);

    data::save_supplier(transaction, supplier);
}

void SourcingService::settle(engine::Transaction& transaction, const Call& call) const {
    const std::string id = subject(call);
    const engine::Row row = fields(call);
    Purchase purchase = existing_purchase(transaction, id);
    const std::string note = optional_text(
        row, "note", "That settlement note could not be read as text.");

    settle_purchase(purchase, clock_(), actor_id(call), note);
    data::save_purchase(transaction, purchase);
}

std::vector<engine::Row> SourcingService::lookup(const engine::Store& store,
                                                 const Call& call) const {
    const engine::Row row = fields(call);
    const std::string material_id = optional_text(
        row, "material_id", "That material filter could not be read as text.");
    const std::string supplier_id = optional_text(
        row, "supplier_id", "That supplier filter could not be read as text.");
    const bool outstanding_only = optional_bool(
        row, "outstanding_only", "That outstanding filter could not be read as yes or no.",
        false);
    const std::int64_t requested = optional_number(
        row, "limit", "That lookup limit could not be read as a number.",
        static_cast<std::int64_t>(kDefaultLookupLimit));
    if (requested <= 0 || requested > static_cast<std::int64_t>(kMaxLookupLimit)) {
        throw RuleViolation("A sourcing lookup must ask for between 1 and 500 purchases.");
    }

    const std::vector<Purchase> purchases = data::lookup_purchases(
        store, material_id, supplier_id, outstanding_only,
        static_cast<std::size_t>(requested));

    std::vector<engine::Row> result;
    result.reserve(purchases.size());
    for (const Purchase& purchase : purchases) {
        engine::Row output = to_row(purchase);
        const auto material = data::find_material(store, purchase.material_id);
        output.set("material_name",
                   engine::Value::text(material ? material->name : std::string{}));
        output.set("material_description",
                   engine::Value::text(material ? material->description : std::string{}));
        result.push_back(std::move(output));
    }
    return result;
}

void SourcingService::record_purchase(engine::Transaction& transaction,
                                      const Material& material,
                                      const Purchase& purchase) const {
    // Validate the complete proposed write before the first save. Throwing
    // would roll the transaction back anyway, but this ordering also makes the
    // rule safe for transaction implementations whose diagnostics inspect the
    // uncommitted rows.
    validate(material);
    validate(purchase);
    if (purchase.material_id != material.id) {
        throw RuleViolation("That purchase and material do not identify the same record.");
    }
    if (!data::find_supplier(transaction, purchase.supplier_id)) {
        throw RuleViolation("That purchase names a supplier that is not on file.");
    }
    if (data::find_purchase(transaction, purchase.id)) {
        throw RuleViolation("That purchase is already on file.");
    }

    const auto stored_material = data::find_material(transaction, material.id);
    if (stored_material) {
        if (stored_material->name != material.name) {
            throw RuleViolation("That material id is already used by a different name.");
        }
    } else {
        const auto same_name = data::find_material_by_name(transaction, material.name);
        if (same_name && same_name->id != material.id) {
            throw RuleViolation("That material name is already on file.");
        }
    }

    if (!stored_material) {
        data::save_material(transaction, material);
    }
    data::save_purchase(transaction, purchase);
}

}  // namespace squiflow::modules::sourcing
