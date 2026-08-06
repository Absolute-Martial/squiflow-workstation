#include "workflows/record_purchase.hpp"

#include <algorithm>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/registry.hpp"
#include "modules/sourcing/domain/sourcing.hpp"
#include "modules/sourcing/service/sourcing_service.hpp"

namespace squiflow::workflows {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](char character) {
        return static_cast<unsigned char>(character) <=
               static_cast<unsigned char>(' ');
    });
}

engine::Row fields(const modules::Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw modules::RuleViolation(
            "This purchase request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const modules::Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("record_purchase: request has no session");
    }
    return *call.actor;
}

void reject_unknown_fields(const engine::Row& row) {
    static const std::set<std::string> allowed{
        "supplier_id", "material_id", "material_name", "material_description",
        "purchased_at", "quantity_scaled", "total_cost_minor", "bill_file_ref",
        "paid", "settled_at", "settlement_note"};
    for (const auto& field : row.fields()) {
        if (!allowed.contains(field.first)) {
            throw modules::RuleViolation(
                "The purchase request contains an unknown field: " +
                field.first + ".");
        }
    }
}

std::string required_text(const engine::Row& row, const char* name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) {
        throw modules::RuleViolation(complaint);
    }
    return *value;
}

std::string optional_text(const engine::Row& row, const char* name) {
    const engine::Value& value = row.get(name);
    if (value.is_null()) {
        return {};
    }
    const std::string* text = value.as_text();
    if (text == nullptr) {
        throw modules::RuleViolation(std::string("Purchase field '") + name +
                                     "' must be text when supplied.");
    }
    return *text;
}

std::int64_t required_positive(const engine::Row& row, const char* name,
                               const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value || *value <= 0) {
        throw modules::RuleViolation(complaint);
    }
    return *value;
}

std::int64_t required_nonnegative(const engine::Row& row, const char* name,
                                  const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value || *value < 0) {
        throw modules::RuleViolation(complaint);
    }
    return *value;
}

bool optional_paid(const engine::Row& row) {
    const engine::Value& value = row.get("paid");
    if (value.is_null()) {
        return false;
    }
    const auto paid = value.as_integer();
    if (!paid || (*paid != 0 && *paid != 1)) {
        throw modules::RuleViolation("Purchase field 'paid' must be yes or no.");
    }
    return *paid == 1;
}

WorkflowResult record(engine::Transaction& transaction,
                      const modules::Call& call,
                      const RecordPurchaseClock& clock) {
    const engine::Row request = fields(call);
    reject_unknown_fields(request);
    const engine::RecordId purchase_record =
        engine::record_id_from_string(call.record_id);
    if (!purchase_record.is_valid()) {
        throw modules::RuleViolation("The purchase identity is invalid.");
    }

    const std::int64_t recorded_at = clock();
    if (recorded_at <= 0) {
        throw modules::RuleViolation("The purchase recording time is invalid.");
    }
    const std::string person = engine::to_string(actor(call).person);

    modules::sourcing::Material material;
    material.id = required_text(
        request, "material_id", "A purchase must identify its material.");
    material.name = required_text(
        request, "material_name", "A purchase must name its material.");
    material.description = optional_text(request, "material_description");
    material.created_at = recorded_at;
    material.created_by = person;
    material.updated_at = recorded_at;
    material.updated_by = person;

    modules::sourcing::Purchase purchase;
    purchase.id = call.record_id;
    purchase.supplier_id = required_text(
        request, "supplier_id", "A purchase must identify its supplier.");
    purchase.material_id = material.id;
    purchase.purchased_at = required_positive(
        request, "purchased_at", "A purchase must record when it was made.");
    purchase.quantity_scaled = required_positive(
        request, "quantity_scaled", "A purchase quantity must be greater than zero.");
    purchase.total_cost_minor = required_nonnegative(
        request, "total_cost_minor", "A purchase cost cannot be negative.");
    purchase.bill_file_ref = optional_text(request, "bill_file_ref");
    purchase.created_at = recorded_at;
    purchase.created_by = person;
    if (optional_paid(request)) {
        purchase.state = modules::sourcing::PurchaseState::Paid;
        purchase.settled_at = required_positive(
            request, "settled_at", "A paid purchase must record when it was settled.");
        purchase.settled_by = person;
        purchase.settlement_note = optional_text(request, "settlement_note");
    } else if (request.has("settled_at") || request.has("settlement_note")) {
        throw modules::RuleViolation(
            "A purchase still owed cannot carry settlement evidence.");
    }

    modules::sourcing::SourcingService service{clock};
    service.record_purchase(transaction, material, purchase);
    return {{protocol::ModuleId::sourcing, purchase_record},
            "Recorded purchase of " + material.name + " from supplier " +
                purchase.supplier_id + "."};
}

}  // namespace

WorkflowDefinition make_record_purchase(RecordPurchaseClock clock) {
    if (!clock) {
        throw modules::RegistryError("record_purchase needs a clock");
    }
    WorkflowDefinition definition;
    definition.operation = protocol::OperationId::record_purchase;
    definition.requirements = {protocol::ModuleId::sourcing};
    definition.handler = [clock = std::move(clock)](
        engine::Transaction& transaction, const modules::Call& call) {
        return record(transaction, call, clock);
    };
    return definition;
}

}  // namespace squiflow::workflows
