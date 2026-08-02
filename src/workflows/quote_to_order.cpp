#include "workflows/quote_to_order.hpp"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/domain/order.hpp"
#include "modules/orders/service/orders_service.hpp"
#include "modules/quotations/data/repository.hpp"
#include "modules/registry.hpp"
#include "modules/quotations/domain/quotation.hpp"

namespace squiflow::workflows {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > static_cast<unsigned char>(' ')) {
            return false;
        }
    }
    return true;
}

std::string required_text(const engine::Row& row, const char* key,
                          const char* message) {
    const std::string invalid{"\x01"};
    const std::string value = row.get(key).text_or(invalid);
    if (value == invalid || blank(value)) {
        throw modules::RuleViolation(message);
    }
    return value;
}

std::string optional_text(const engine::Row& row, const char* key,
                          const char* message) {
    if (!row.has(key)) {
        return {};
    }
    const std::string invalid{"\x01"};
    const std::string value = row.get(key).text_or(invalid);
    if (value == invalid) {
        throw modules::RuleViolation(message);
    }
    return value;
}

std::int64_t required_positive(const engine::Row& row, const char* key,
                               const char* message) {
    constexpr std::int64_t invalid = std::numeric_limits<std::int64_t>::min();
    const std::int64_t value = row.get(key).integer_or(invalid);
    if (value <= 0) {
        throw modules::RuleViolation(message);
    }
    return value;
}

std::int64_t optional_nonnegative(const engine::Row& row, const char* key,
                                  const char* message) {
    if (!row.has(key)) {
        return 0;
    }
    const std::int64_t value = row.get(key).integer_or(-1);
    if (value < 0) {
        throw modules::RuleViolation(message);
    }
    return value;
}

void copy_price_origin(const modules::quotations::QuotationLine& source,
                       modules::orders::OrderLine& target) {
    using engine::RateOrigin;
    using modules::pricing::RateSource;
    switch (source.rate_origin) {
        case RateOrigin::CatalogDefault:
            target.price_source = RateSource::Default;
            return;
        case RateOrigin::PartySpecific:
            target.price_source = RateSource::PartyRate;
            return;
        case RateOrigin::Agreement:
            target.price_source = RateSource::None;
            target.price_overridden = true;
            target.price_reason = "Agreement: " + source.rate_reason;
            return;
        case RateOrigin::ManualOverride:
            target.price_source = RateSource::None;
            target.price_overridden = true;
            target.price_reason = source.rate_reason;
            return;
        case RateOrigin::OffCatalog:
            target.price_source = RateSource::None;
            target.price_overridden = true;
            target.price_reason = source.rate_reason.empty()
                ? "Off-catalog quotation price" : source.rate_reason;
            return;
    }
    throw modules::RuleViolation(
        "That quotation line has a price origin this build does not understand.");
}

WorkflowResult convert(engine::Transaction& transaction, const modules::Call& call,
                       const WorkflowClock& clock) {
    const engine::Row fields = modules::orders::read_fields(call);
    const engine::Session& who = modules::orders::actor(call);
    const std::string quotation_id = required_text(
        fields, "quotation_id", "The conversion must identify a quotation.");
    const std::int64_t revision_number = required_positive(
        fields, "revision", "The conversion must identify a positive accepted revision.");

    const auto quotation = modules::quotations::data::find_quotation(
        transaction, quotation_id);
    if (!quotation) {
        throw modules::RuleViolation("That quotation is not on file.");
    }
    if (quotation->state != modules::quotations::QuotationState::Accepted) {
        throw modules::RuleViolation("Only an accepted quotation can become an order.");
    }
    if (quotation->accepted_revision != revision_number) {
        throw modules::RuleViolation(
            "That is not the exact quotation revision the customer accepted.");
    }

    const auto revision = modules::quotations::data::revision_numbered(
        transaction, quotation_id, revision_number);
    if (!revision || !revision->issued || revision->quotation_id != quotation_id) {
        throw modules::RuleViolation("The accepted quotation revision is not available.");
    }

    const std::vector<modules::quotations::QuotationLine> source_lines =
        modules::quotations::data::lines_for_revision(transaction, revision->id);
    if (source_lines.empty()) {
        throw modules::RuleViolation("An empty quotation cannot become an order.");
    }
    for (const auto& line : source_lines) {
        modules::quotations::validate(line);
        if (line.quotation_id != quotation_id || line.revision_id != revision->id) {
            throw modules::RuleViolation(
                "A quotation line does not belong to the accepted revision.");
        }
    }
    const engine::MoneyResult quoted_total =
        modules::quotations::revision_total(source_lines);
    if (!quoted_total.ok || quoted_total.value.minor != revision->total_minor) {
        throw modules::RuleViolation(
            "The accepted quotation total does not match its frozen lines.");
    }

    if (modules::orders::data::find_order(transaction, call.record_id)) {
        throw modules::RuleViolation("That target order already exists.");
    }
    if (modules::orders::data::order_for_revision(
            transaction, quotation_id, revision->id)) {
        throw modules::RuleViolation(
            "That accepted quotation revision already has an order.");
    }

    const std::int64_t at = clock();
    if (at <= 0) {
        throw modules::RuleViolation("The conversion time is invalid.");
    }
    modules::orders::Order order;
    order.id = call.record_id;
    order.party_id = quotation->party_id;
    order.source_quotation_id = quotation_id;
    order.source_revision_id = revision->id;
    order.source_revision = revision_number;
    order.promised_at = optional_nonnegative(
        fields, "promised_at", "That promised date could not be read as a date.");
    order.note = optional_text(
        fields, "note", "That order note could not be read as text.");
    order.created_at = at;
    order.created_by = engine::to_string(who.person);
    modules::orders::data::save_order(transaction, order);

    for (const auto& source : source_lines) {
        if (modules::orders::data::find_line(transaction, source.id)) {
            throw modules::RuleViolation(
                "A copied quotation line conflicts with an existing order line.");
        }
        modules::orders::OrderLine target;
        target.id = source.id;
        target.order_id = order.id;
        target.position = source.position;
        target.product_id = source.product_id;
        target.description = source.description;
        target.quantity_scaled = source.quantity_scaled;
        target.unit_price_minor = source.unit_price_minor;
        target.added_at = at;
        target.added_by = order.created_by;
        copy_price_origin(source, target);
        modules::orders::data::save_line(transaction, target);
    }

    const engine::MoneyResult order_total =
        modules::orders::data::total_for_order(transaction, order.id);
    if (!order_total.ok || order_total.value.minor != revision->total_minor) {
        throw modules::RuleViolation(
            "The order snapshot does not match the accepted quotation total.");
    }

    const engine::RecordId order_id = engine::record_id_from_string(order.id);
    if (!order_id.is_valid()) {
        throw modules::RuleViolation("The target order id is not a valid record id.");
    }
    return {{protocol::ModuleId::orders, order_id},
            "Converted accepted quotation " + quotation_id + " revision " +
                std::to_string(revision_number) + " to an order."};
}

}  // namespace

WorkflowDefinition make_quote_to_order(WorkflowClock clock) {
    if (!clock) {
        throw modules::RegistryError("quote_to_order needs a clock");
    }
    WorkflowDefinition definition;
    definition.operation = protocol::OperationId::quote_to_order;
    definition.requirements = {protocol::ModuleId::quotations,
                               protocol::ModuleId::orders,
                               protocol::ModuleId::pricing};
    definition.handler = [clock = std::move(clock)](
        engine::Transaction& transaction, const modules::Call& call) {
        return convert(transaction, call, clock);
    };
    return definition;
}

}  // namespace squiflow::workflows
