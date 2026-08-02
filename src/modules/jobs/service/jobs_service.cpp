#include "modules/jobs/service/jobs_service.hpp"

#include <stdexcept>
#include <string>
#include <utility>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/jobs/data/repository.hpp"

namespace squiflow::modules::jobs {
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
        throw std::logic_error("jobs: a write arrived with no session");
    }
    return *call.actor;
}

std::string subject(const Call& call) {
    if (blank(call.record_id)) {
        throw RuleViolation("This request does not identify its record.");
    }
    return call.record_id;
}

std::string required_text(const engine::Row& row,
                          const char* name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) {
        throw RuleViolation(complaint);
    }
    return *value;
}

std::string optional_text(const engine::Row& row,
                          const char* name,
                          const char* complaint) {
    if (!row.has(name)) {
        return {};
    }
    const std::string* value = row.get(name).as_text();
    if (value == nullptr) {
        throw RuleViolation(complaint);
    }
    return *value;
}

std::int64_t required_number(const engine::Row& row,
                             const char* name,
                             const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value) {
        throw RuleViolation(complaint);
    }
    return *value;
}

std::int64_t optional_number(const engine::Row& row,
                             const char* name,
                             const char* complaint) {
    if (!row.has(name)) {
        return 0;
    }
    return required_number(row, name, complaint);
}

bool optional_bool(const engine::Row& row,
                   const char* name,
                   const char* complaint,
                   bool fallback = false) {
    if (!row.has(name)) {
        return fallback;
    }
    const engine::Value& value = row.get(name);
    if (value.kind() != engine::ValueKind::Integer) {
        throw RuleViolation(complaint);
    }
    return value.boolean_or(fallback);
}

CommercialProgress commercial_from(std::int64_t value) {
    switch (value) {
        case 0: return CommercialProgress::Open;
        case 1: return CommercialProgress::Approved;
        case 2: return CommercialProgress::Closed;
        default: throw RuleViolation("That commercial progress state is not understood.");
    }
}

DesignProgress design_from(std::int64_t value) {
    switch (value) {
        case 0: return DesignProgress::NotNeeded;
        case 1: return DesignProgress::Waiting;
        case 2: return DesignProgress::Approved;
        default: throw RuleViolation("That design progress state is not understood.");
    }
}

ProductionProgress production_from(std::int64_t value) {
    switch (value) {
        case 0: return ProductionProgress::Queued;
        case 1: return ProductionProgress::Running;
        case 2: return ProductionProgress::Produced;
        default: throw RuleViolation("That production progress state is not understood.");
    }
}

FulfilmentProgress fulfilment_from(std::int64_t value) {
    switch (value) {
        case 0: return FulfilmentProgress::Waiting;
        case 1: return FulfilmentProgress::Ready;
        case 2: return FulfilmentProgress::Delivered;
        default: throw RuleViolation("That fulfilment progress state is not understood.");
    }
}

PaymentProgress payment_from(std::int64_t value) {
    switch (value) {
        case 0: return PaymentProgress::Unbilled;
        case 1: return PaymentProgress::Invoiced;
        case 2: return PaymentProgress::PartPaid;
        case 3: return PaymentProgress::Paid;
        default: throw RuleViolation("That payment progress state is not understood.");
    }
}

engine::RateOrigin rate_origin_from(std::int64_t value) {
    switch (value) {
        case 0: return engine::RateOrigin::CatalogDefault;
        case 1: return engine::RateOrigin::PartySpecific;
        case 2: return engine::RateOrigin::Agreement;
        case 3: return engine::RateOrigin::ManualOverride;
        case 4: return engine::RateOrigin::OffCatalog;
        default: throw RuleViolation("That job price origin is not understood.");
    }
}

template <typename Reader>
Job existing_job(const Reader& reader, const std::string& id) {
    const auto job = data::find_job(reader, id);
    if (!job) {
        throw RuleViolation("That job is not on file.");
    }
    validate(*job);
    return *job;
}

void apply_common_edits(Job& job, const engine::Row& row) {
    if (row.has("party_id")) {
        job.party_id = optional_text(row, "party_id", "That customer could not be read as a customer record.");
    }
    if (row.has("source_order_id")) {
        job.source_order_id = optional_text(row, "source_order_id", "That source order link could not be read.");
    }
    if (row.has("source_quotation_id")) {
        job.source_quotation_id = optional_text(row, "source_quotation_id", "That source quotation link could not be read.");
    }
    if (row.has("thin")) {
        job.thin = optional_bool(row, "thin", "That thin-job flag could not be read.");
    }
    if (row.has("title")) {
        job.title = optional_text(row, "title", "That job title could not be read as text.");
    }
    if (row.has("description")) {
        job.description = optional_text(row, "description", "That job description could not be read as text.");
    }
    if (row.has("specifications")) {
        job.specifications = optional_text(row, "specifications", "Those specifications could not be read as text.");
    }
    if (row.has("quantity_scaled")) {
        job.quantity_scaled = required_number(row, "quantity_scaled", "That quantity could not be read as a number.");
    }
    if (row.has("unit_price_minor")) {
        job.unit_price_minor = required_number(row, "unit_price_minor", "That unit price could not be read as a number.");
    }
    if (row.has("rate_origin")) {
        job.rate_origin = rate_origin_from(required_number(row, "rate_origin", "That price origin could not be read as a number."));
    }
    if (row.has("rate_reason")) {
        job.rate_reason = optional_text(row, "rate_reason", "That price reason could not be read as text.");
    }
    if (row.has("promised_at")) {
        job.promised_at = required_number(row, "promised_at", "That promised time could not be read as a number.");
    }
    if (row.has("deadline_at")) {
        job.deadline_at = required_number(row, "deadline_at", "That deadline could not be read as a number.");
    }
    if (row.has("note")) {
        job.note = optional_text(row, "note", "That job note could not be read as text.");
    }
    if (row.has("commercial")) {
        job.commercial = commercial_from(required_number(row, "commercial", "That commercial progress value could not be read as a number."));
    }
    if (row.has("design")) {
        job.design = design_from(required_number(row, "design", "That design progress value could not be read as a number."));
    }
    if (row.has("production")) {
        job.production = production_from(required_number(row, "production", "That production progress value could not be read as a number."));
    }
    if (row.has("fulfilment")) {
        job.fulfilment = fulfilment_from(required_number(row, "fulfilment", "That fulfilment progress value could not be read as a number."));
    }
    if (row.has("payment")) {
        job.payment = payment_from(required_number(row, "payment", "That payment progress value could not be read as a number."));
    }
    if (row.has("material_reference")) {
        job.material_reference = optional_text(row, "material_reference", "That material reference could not be read as text.");
    }
    if (row.has("proof_approval_ref")) {
        job.proof_approval_ref = optional_text(row, "proof_approval_ref", "That proof approval reference could not be read as text.");
    }
    if (row.has("reprint_of_job_id")) {
        job.reprint_of_job_id = optional_text(row, "reprint_of_job_id", "That reprint source job could not be read as text.");
    }
    if (row.has("reprint_reason")) {
        job.reprint_reason = optional_text(row, "reprint_reason", "That reprint reason could not be read as text.");
    }

    const engine::MoneyResult total = job_total(job);
    if (!total.ok) {
        throw RuleViolation("That job total is too large to store safely.");
    }
    job.total_price_minor = total.value.minor;
}

}  // namespace

void JobsService::create(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    if (data::find_job(transaction, id)) {
        throw RuleViolation("That job has already been recorded.");
    }

    Job job;
    job.id = id;
    job.created_at = clock_();
    job.created_by = engine::to_string(who.person);
    apply_common_edits(job, row);
    data::save_job(transaction, job);
}

void JobsService::update(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    static_cast<void>(actor(call));

    Job job = existing_job(transaction, id);
    if (!can_change(job.state)) {
        throw RuleViolation("A finished or cancelled job cannot be changed.");
    }

    apply_common_edits(job, row);
    data::save_job(transaction, job);
}

void JobsService::state_change(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Job job = existing_job(transaction, id);
    const JobState next = static_cast<JobState>(required_number(
        row, "state", "That job state change could not be read as a number."));
    if (!transition_allowed(job.state, next)) {
        throw RuleViolation("That job state change is not allowed.");
    }

    if (row.has("commercial")) {
        job.commercial = commercial_from(required_number(row, "commercial", "That commercial progress value could not be read as a number."));
    }
    if (row.has("design")) {
        job.design = design_from(required_number(row, "design", "That design progress value could not be read as a number."));
    }
    if (row.has("production")) {
        job.production = production_from(required_number(row, "production", "That production progress value could not be read as a number."));
    }
    if (row.has("fulfilment")) {
        job.fulfilment = fulfilment_from(required_number(row, "fulfilment", "That fulfilment progress value could not be read as a number."));
    }
    if (row.has("payment")) {
        job.payment = payment_from(required_number(row, "payment", "That payment progress value could not be read as a number."));
    }

    if (next == JobState::InProgress) {
        job.ticket_series = required_text(row, "ticket_series", "A started job needs its ticket series.");
        const std::int64_t ticket_number = required_number(
            row, "ticket_number", "A started job needs its ticket number.");
        if (ticket_number <= 0) {
            throw RuleViolation("A started job must have a positive ticket number.");
        }
        job.ticket_number = static_cast<std::uint64_t>(ticket_number);
        job.started_at = clock_();
        job.started_by = engine::to_string(who.person);
        if (job.production == ProductionProgress::Queued) {
            job.production = ProductionProgress::Running;
        }
    } else if (next == JobState::Done) {
        job.done_at = clock_();
        job.done_by = engine::to_string(who.person);
        job.production = ProductionProgress::Produced;
        job.fulfilment = FulfilmentProgress::Delivered;
        job.delivered_at = optional_number(row, "delivered_at", "That delivery time could not be read as a number.");
        if (job.delivered_at == 0) {
            job.delivered_at = job.done_at;
        }
        job.delivered_by = required_text(row, "delivered_by", "A delivered job must say who delivered it.");
        job.received_by = required_text(row, "received_by", "A delivered job must say who received it.");
        job.delivery_signature_ref = required_text(
            row, "delivery_signature_ref", "A delivered job must keep the signature reference.");
    }

    job.state = next;
    data::save_job(transaction, job);
}

void JobsService::cancel(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Job job = existing_job(transaction, id);
    if (job.state == JobState::Cancelled) {
        throw RuleViolation("That job has already been cancelled.");
    }
    if (job.state == JobState::Done) {
        throw RuleViolation("A completed job cannot be cancelled.");
    }

    if (job.ticket_number == 0 || blank(job.ticket_series)) {
        job.ticket_series = required_text(row, "ticket_series", "A cancelled job must keep its ticket series.");
        const std::int64_t ticket_number = required_number(
            row, "ticket_number", "A cancelled job must keep its ticket number.");
        if (ticket_number <= 0) {
            throw RuleViolation("A cancelled job must have a positive ticket number.");
        }
        job.ticket_number = static_cast<std::uint64_t>(ticket_number);
    }
    job.state = JobState::Cancelled;
    job.cancelled_at = clock_();
    job.cancelled_by = engine::to_string(who.person);
    job.cancel_reason = required_text(row, "reason", "A cancelled job must say why it was cancelled.");
    data::save_job(transaction, job);
}

}  // namespace squiflow::modules::jobs
