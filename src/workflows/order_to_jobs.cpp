#include "workflows/order_to_jobs.hpp"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <limits>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "engine/records/identity.hpp"
#include "modules/jobs/data/repository.hpp"
#include "modules/jobs/domain/job.hpp"
#include "modules/orders/data/repository.hpp"
#include "modules/orders/domain/order.hpp"
#include "modules/orders/service/orders_service.hpp"
#include "modules/pricing/domain/rate.hpp"
#include "modules/registry.hpp"

namespace squiflow::workflows {
namespace {

bool blank(const std::string& text) noexcept {
    return std::all_of(text.begin(), text.end(), [](const char c) {
        return static_cast<unsigned char>(c) <= static_cast<unsigned char>(' ');
    });
}

int nibble(const char c) noexcept {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + c - 'a';
    if (c >= 'A' && c <= 'F') return 10 + c - 'A';
    return -1;
}

void require_record_id(const std::string& value, const char* complaint) {
    if (value.size() != 32U ||
        std::any_of(value.begin(), value.end(), [](const char c) { return nibble(c) < 0; }) ||
        !engine::record_id_from_string(value).is_valid()) {
        throw modules::RuleViolation(complaint);
    }
}

std::string required_text(const engine::Row& row, const char* key,
                          const char* complaint) {
    const std::string* value = row.get(key).as_text();
    if (value == nullptr || blank(*value)) throw modules::RuleViolation(complaint);
    return *value;
}

std::string optional_text(const engine::Row& row, const char* key,
                          const char* complaint) {
    if (!row.has(key)) return {};
    const std::string* value = row.get(key).as_text();
    if (value == nullptr) throw modules::RuleViolation(complaint);
    return *value;
}

std::int64_t optional_time(const engine::Row& row, const char* key,
                           const std::int64_t fallback, const char* complaint) {
    if (!row.has(key)) return fallback;
    const auto value = row.get(key).as_integer();
    if (!value || *value < 0) throw modules::RuleViolation(complaint);
    return *value;
}

void reject_unknown_fields(const engine::Row& row) {
    static const std::set<std::string> allowed{
        "order_id", "mode", "line_ids", "promised_at", "deadline_at",
        "note", "specifications", "title_prefix"};
    for (const auto& field : row.fields()) {
        if (!allowed.contains(field.first)) {
            throw modules::RuleViolation("The order-to-jobs request contains an unknown field: " +
                                         field.first + ".");
        }
    }
}

std::vector<std::string> parse_ids(const std::string& text) {
    std::vector<std::string> result;
    std::set<std::string> unique;
    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t comma = text.find(',', start);
        const std::size_t end = comma == std::string::npos ? text.size() : comma;
        const std::string id = text.substr(start, end - start);
        if (id.empty() || blank(id)) {
            throw modules::RuleViolation("Selected order-line ids cannot be empty.");
        }
        require_record_id(id, "A selected order-line id is invalid.");
        if (!unique.insert(id).second) {
            throw modules::RuleViolation("The same order line was selected more than once.");
        }
        result.push_back(id);
        if (comma == std::string::npos) break;
        start = comma + 1U;
    }
    if (result.empty()) {
        throw modules::RuleViolation("Selected-line mode needs at least one order line.");
    }
    return result;
}

engine::RateOrigin copied_origin(const modules::orders::OrderLine& line) {
    using modules::pricing::RateSource;
    switch (line.price_source) {
        case RateSource::PartyRate: return engine::RateOrigin::PartySpecific;
        case RateSource::CatchAllRate:
        case RateSource::Default: return engine::RateOrigin::CatalogDefault;
        case RateSource::None:
            if (!line.price_overridden) {
                throw modules::RuleViolation(
                    "An order line has no valid frozen price origin.");
            }
            if (line.price_reason.starts_with("Agreement:")) {
                return engine::RateOrigin::Agreement;
            }
            return line.product_id.empty() ? engine::RateOrigin::OffCatalog
                                           : engine::RateOrigin::ManualOverride;
    }
    throw modules::RuleViolation("An order line has a price origin this build does not understand.");
}

std::string printable_description(const modules::orders::OrderLine& line) {
    if (!blank(line.description)) return line.description;
    return "Product " + line.product_id;
}

std::vector<modules::orders::OrderLine> select_lines(
    const std::vector<modules::orders::OrderLine>& all,
    const engine::Row& fields) {
    const std::string mode = fields.has("mode")
        ? required_text(fields, "mode", "The job creation mode must be text.")
        : "all_lines";
    if (mode == "all_lines") {
        if (fields.has("line_ids")) {
            throw modules::RuleViolation("All-lines mode cannot also name selected lines.");
        }
        return all;
    }
    if (mode != "selected_lines") {
        throw modules::RuleViolation("The job creation mode must be all_lines or selected_lines.");
    }
    const std::vector<std::string> ids = parse_ids(required_text(
        fields, "line_ids", "Selected-line mode needs order-line ids."));
    std::vector<modules::orders::OrderLine> selected;
    selected.reserve(ids.size());
    for (const std::string& id : ids) {
        const auto found = std::find_if(all.begin(), all.end(), [&](const auto& line) {
            return line.id == id;
        });
        if (found == all.end()) {
            throw modules::RuleViolation("A selected line does not belong to that order.");
        }
        selected.push_back(*found);
    }
    return selected;
}

WorkflowResult convert(engine::Transaction& transaction, const modules::Call& call,
                       const OrderToJobsClock& clock) {
    const engine::Row fields = modules::orders::read_fields(call);
    reject_unknown_fields(fields);
    const engine::Session& who = modules::orders::actor(call);
    const std::string order_id = required_text(
        fields, "order_id", "The conversion must identify a source order.");
    require_record_id(order_id, "The source order id is invalid.");
    require_record_id(call.record_id, "The conversion identity is invalid.");

    const auto stored_order = modules::orders::data::find_order(transaction, order_id);
    if (!stored_order) throw modules::RuleViolation("That order is not on file.");
    const modules::orders::Order& order = *stored_order;
    modules::orders::validate(order);
    if (order.state != modules::orders::OrderState::Open) {
        throw modules::RuleViolation("Only an open order can create jobs.");
    }

    const std::vector<modules::orders::OrderLine> all =
        modules::orders::data::lines_for_order(transaction, order_id);
    if (all.empty()) throw modules::RuleViolation("An empty order cannot create jobs.");
    for (const auto& line : all) {
        modules::orders::validate(line);
        if (line.order_id != order_id) {
            throw modules::RuleViolation("An order line does not belong to its source order.");
        }
    }
    const std::vector<modules::orders::OrderLine> selected = select_lines(all, fields);

    const std::int64_t at = clock();
    if (at <= 0) throw modules::RuleViolation("The conversion time is invalid.");
    const std::string creator = engine::to_string(who.person);
    const std::int64_t promised = optional_time(
        fields, "promised_at", order.promised_at,
        "The promised time must be a non-negative integer.");
    const std::int64_t deadline = optional_time(
        fields, "deadline_at", 0,
        "The deadline must be a non-negative integer.");
    const std::string note = fields.has("note")
        ? optional_text(fields, "note", "The job note must be text.") : order.note;
    const std::string specifications = optional_text(
        fields, "specifications", "The job specifications must be text.");
    const std::string title_prefix = optional_text(
        fields, "title_prefix", "The job title prefix must be text.");

    std::vector<modules::jobs::Job> jobs;
    std::set<std::string> derived_ids;
    jobs.reserve(selected.size());
    for (const auto& line : selected) {
        if (modules::jobs::data::job_for_order_line(transaction, order_id, line.id)) {
            throw modules::RuleViolation("That order line already has a derived job.");
        }
        modules::jobs::Job job;
        job.id = order_job_id(call.record_id, line.id);
        if (!derived_ids.insert(job.id).second) {
            throw modules::RuleViolation("Two selected lines derive the same job identity.");
        }
        job.party_id = order.party_id;
        job.source_order_id = order.id;
        job.source_order_line_id = line.id;
        job.source_quotation_id = order.source_quotation_id;
        const std::string base = printable_description(line);
        job.title = blank(title_prefix) ? base : title_prefix + " " + base;
        job.description = base;
        job.specifications = specifications;
        job.quantity_scaled = line.quantity_scaled;
        job.unit_price_minor = line.unit_price_minor;
        const engine::MoneyResult total = modules::orders::line_amount(line);
        if (!total.ok) throw modules::RuleViolation("An order-line total cannot be copied safely.");
        job.total_price_minor = total.value.minor;
        job.rate_origin = copied_origin(line);
        job.rate_reason = line.price_reason;
        job.promised_at = promised;
        job.deadline_at = deadline;
        job.note = note;
        job.created_at = at;
        job.created_by = creator;
        modules::jobs::validate(job);
        jobs.push_back(std::move(job));
    }

    // Target collisions are deliberately checked immediately before each
    // insert. If a later target collides, earlier inserts are rolled back by
    // the workflow transaction; the permanent test locks that property down.
    for (const auto& job : jobs) {
        if (modules::jobs::data::find_job(transaction, job.id)) {
            throw modules::RuleViolation("A derived job identity already exists.");
        }
        modules::jobs::data::save_job(transaction, job);
    }

    const engine::RecordId subject = engine::record_id_from_string(jobs.front().id);
    return {{protocol::ModuleId::jobs, subject},
            "Created " + std::to_string(jobs.size()) + " job(s) from order " + order_id + "."};
}

}  // namespace

std::string order_job_id(const std::string& conversion_id,
                         const std::string& order_line_id) {
    require_record_id(conversion_id, "The conversion identity is invalid.");
    require_record_id(order_line_id, "The source order-line id is invalid.");
    static constexpr char hex[] = "0123456789abcdef";
    std::string result(32U, '0');
    bool nonzero = false;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const int value = nibble(conversion_id[i]) ^ nibble(order_line_id[i]);
        result[i] = hex[static_cast<std::size_t>(value)];
        nonzero = nonzero || value != 0;
    }
    if (!nonzero) result.back() = '1';
    return result;
}

WorkflowDefinition make_order_to_jobs(OrderToJobsClock clock) {
    if (!clock) throw modules::RegistryError("order_to_jobs needs a clock");
    WorkflowDefinition definition;
    definition.operation = protocol::OperationId::order_to_jobs;
    definition.requirements = {protocol::ModuleId::orders, protocol::ModuleId::jobs};
    definition.handler = [clock = std::move(clock)](
        engine::Transaction& transaction, const modules::Call& call) {
        return convert(transaction, call, clock);
    };
    return definition;
}

}  // namespace squiflow::workflows
