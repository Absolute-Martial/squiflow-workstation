#include "modules/jobs/domain/job.hpp"

#include <limits>

#include "modules/context.hpp"

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

CommercialProgress commercial_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return CommercialProgress::Open;
        case 1: return CommercialProgress::Approved;
        case 2: return CommercialProgress::Closed;
        default: return CommercialProgress::Closed;
    }
}

DesignProgress design_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return DesignProgress::NotNeeded;
        case 1: return DesignProgress::Waiting;
        case 2: return DesignProgress::Approved;
        default: return DesignProgress::Approved;
    }
}

ProductionProgress production_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return ProductionProgress::Queued;
        case 1: return ProductionProgress::Running;
        case 2: return ProductionProgress::Produced;
        default: return ProductionProgress::Produced;
    }
}

FulfilmentProgress fulfilment_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return FulfilmentProgress::Waiting;
        case 1: return FulfilmentProgress::Ready;
        case 2: return FulfilmentProgress::Delivered;
        default: return FulfilmentProgress::Delivered;
    }
}

PaymentProgress payment_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return PaymentProgress::Unbilled;
        case 1: return PaymentProgress::Invoiced;
        case 2: return PaymentProgress::PartPaid;
        case 3: return PaymentProgress::Paid;
        default: return PaymentProgress::Paid;
    }
}

engine::RateOrigin origin_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return engine::RateOrigin::CatalogDefault;
        case 1: return engine::RateOrigin::PartySpecific;
        case 2: return engine::RateOrigin::Agreement;
        case 3: return engine::RateOrigin::ManualOverride;
        case 4: return engine::RateOrigin::OffCatalog;
        default: return engine::RateOrigin::ManualOverride;
    }
}

std::uint64_t ticket_number_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("ticket_number").integer_or(0);
    return stored > 0 ? static_cast<std::uint64_t>(stored) : 0;
}

bool axis_done(const Job& job) noexcept {
    return job.production == ProductionProgress::Produced &&
           job.fulfilment == FulfilmentProgress::Delivered;
}

}  // namespace

const char* to_string(JobState state) noexcept {
    switch (state) {
        case JobState::Draft: return "draft";
        case JobState::InProgress: return "in_progress";
        case JobState::Done: return "done";
        case JobState::Cancelled: return "cancelled";
    }
    return "?";
}

bool transition_allowed(JobState from, JobState to) noexcept {
    switch (from) {
        case JobState::Draft:
            return to == JobState::InProgress || to == JobState::Cancelled;
        case JobState::InProgress:
            return to == JobState::Done || to == JobState::Cancelled;
        case JobState::Done:
        case JobState::Cancelled:
            return false;
    }
    return false;
}

engine::MoneyResult job_total(const Job& job) noexcept {
    return engine::money_multiply(engine::Money{job.unit_price_minor},
                                  engine::Quantity{job.quantity_scaled});
}

void validate(const Job& job) {
    if (job.id.empty()) {
        throw RuleViolation("This job has no record to be saved under.");
    }
    if (job.created_at <= 0 || blank(job.created_by)) {
        throw RuleViolation("A job must record when and by whom it was created.");
    }
    if (job.quantity_scaled <= 0) {
        throw RuleViolation("A job must have a quantity greater than zero.");
    }
    if (job.unit_price_minor < 0 || job.total_price_minor < 0) {
        throw RuleViolation("A job cannot carry negative money.");
    }
    if (job.promised_at < 0 || job.deadline_at < 0) {
        throw RuleViolation("A job cannot promise time before this shop existed.");
    }
    if (job.deadline_at > 0 && job.promised_at > 0 && job.deadline_at < job.promised_at) {
        throw RuleViolation("A job deadline cannot be before the promised time.");
    }
    if (!job.party_id.empty() && blank(job.party_id)) {
        throw RuleViolation("A job customer cannot be only whitespace.");
    }
    if (!job.source_order_line_id.empty() && job.source_order_id.empty()) {
        throw RuleViolation("A source order line cannot exist without its source order.");
    }
    if (blank(job.title) && blank(job.description)) {
        throw RuleViolation("A job must say what is being made.");
    }
    if (job.thin && !blank(job.specifications)) {
        throw RuleViolation("A thin job cannot pretend to have complete specifications.");
    }
    if (!job.thin && blank(job.description)) {
        throw RuleViolation("A normal job must preserve its printable description.");
    }

    const engine::MoneyResult total = job_total(job);
    if (!total.ok || total.value.minor != job.total_price_minor) {
        throw RuleViolation("That job price snapshot does not match its quantity and unit price.");
    }

    switch (job.rate_origin) {
        case engine::RateOrigin::CatalogDefault:
        case engine::RateOrigin::PartySpecific:
        case engine::RateOrigin::Agreement:
        case engine::RateOrigin::ManualOverride:
        case engine::RateOrigin::OffCatalog:
            break;
        default:
            throw RuleViolation("That job has a price origin this build does not understand.");
    }
    if ((job.rate_origin == engine::RateOrigin::Agreement ||
         job.rate_origin == engine::RateOrigin::ManualOverride) &&
        blank(job.rate_reason)) {
        throw RuleViolation("An agreed or manually changed job rate must keep its reason.");
    }

    switch (job.state) {
        case JobState::Draft:
            if (!blank(job.ticket_series) || job.ticket_number != 0 ||
                job.started_at != 0 || !blank(job.started_by) ||
                job.done_at != 0 || !blank(job.done_by) ||
                job.cancelled_at != 0 || !blank(job.cancelled_by) ||
                !blank(job.cancel_reason)) {
                throw RuleViolation("A draft job cannot carry working, done, or cancellation evidence.");
            }
            break;
        case JobState::InProgress:
            if (blank(job.ticket_series) || job.ticket_number == 0 ||
                job.started_at <= 0 || blank(job.started_by) ||
                job.done_at != 0 || !blank(job.done_by) ||
                job.cancelled_at != 0 || !blank(job.cancelled_by) ||
                !blank(job.cancel_reason)) {
                throw RuleViolation("A working job must carry start evidence and no final evidence.");
            }
            break;
        case JobState::Done:
            if (blank(job.ticket_series) || job.ticket_number == 0 ||
                job.started_at <= 0 || blank(job.started_by) ||
                job.done_at <= 0 || blank(job.done_by) ||
                job.cancelled_at != 0 || !blank(job.cancelled_by) ||
                !blank(job.cancel_reason)) {
                throw RuleViolation("A done job must carry start and completion evidence only.");
            }
            if (job.done_at < job.started_at) {
                throw RuleViolation("A job cannot finish before it started.");
            }
            if (!axis_done(job)) {
                throw RuleViolation("A done job must be both produced and delivered.");
            }
            break;
        case JobState::Cancelled:
            if (blank(job.ticket_series) || job.ticket_number == 0 ||
                job.cancelled_at <= 0 || blank(job.cancelled_by) ||
                blank(job.cancel_reason) ||
                job.done_at != 0 || !blank(job.done_by)) {
                throw RuleViolation("A cancelled job must keep ticket and cancellation evidence only.");
            }
            if (job.started_at > 0 && job.cancelled_at < job.started_at) {
                throw RuleViolation("A job cannot be cancelled before it started.");
            }
            break;
        default:
            throw RuleViolation("That job has a state this build does not understand.");
    }

    if (job.fulfilment == FulfilmentProgress::Delivered) {
        if (job.delivered_at <= 0 || blank(job.delivered_by) ||
            blank(job.received_by) || blank(job.delivery_signature_ref)) {
            throw RuleViolation(
                "A delivered job must keep who delivered, who received, and the signature reference.");
        }
    } else if (job.delivered_at != 0 || !blank(job.delivered_by) ||
               !blank(job.received_by) || !blank(job.delivery_signature_ref)) {
        throw RuleViolation("Delivery evidence cannot exist before delivery.");
    }

    if (!job.reprint_of_job_id.empty() && blank(job.reprint_reason)) {
        throw RuleViolation("A reprint must preserve its reason.");
    }
    if (job.reprint_of_job_id.empty() && !blank(job.reprint_reason)) {
        throw RuleViolation("A reprint reason cannot exist without the original job link.");
    }
}

engine::Row to_row(const Job& job) {
    engine::Row row;
    row.set("id", engine::Value::text(job.id));
    row.set("party_id", engine::Value::text(job.party_id));
    row.set("source_order_id", engine::Value::text(job.source_order_id));
    row.set("source_order_line_id", engine::Value::text(job.source_order_line_id));
    row.set("source_quotation_id", engine::Value::text(job.source_quotation_id));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(job.state)));
    row.set("ticket_series", engine::Value::text(job.ticket_series));
    row.set("ticket_number", engine::Value::integer(static_cast<std::int64_t>(job.ticket_number)));
    row.set("thin", engine::Value::boolean(job.thin));
    row.set("title", engine::Value::text(job.title));
    row.set("description", engine::Value::text(job.description));
    row.set("specifications", engine::Value::text(job.specifications));
    row.set("quantity_scaled", engine::Value::integer(job.quantity_scaled));
    row.set("unit_price_minor", engine::Value::integer(job.unit_price_minor));
    row.set("total_price_minor", engine::Value::integer(job.total_price_minor));
    row.set("rate_origin", engine::Value::integer(static_cast<std::int64_t>(job.rate_origin)));
    row.set("rate_reason", engine::Value::text(job.rate_reason));
    row.set("promised_at", engine::Value::integer(job.promised_at));
    row.set("deadline_at", engine::Value::integer(job.deadline_at));
    row.set("note", engine::Value::text(job.note));
    row.set("commercial", engine::Value::integer(static_cast<std::int64_t>(job.commercial)));
    row.set("design", engine::Value::integer(static_cast<std::int64_t>(job.design)));
    row.set("production", engine::Value::integer(static_cast<std::int64_t>(job.production)));
    row.set("fulfilment", engine::Value::integer(static_cast<std::int64_t>(job.fulfilment)));
    row.set("payment", engine::Value::integer(static_cast<std::int64_t>(job.payment)));
    row.set("material_reference", engine::Value::text(job.material_reference));
    row.set("delivered_at", engine::Value::integer(job.delivered_at));
    row.set("delivered_by", engine::Value::text(job.delivered_by));
    row.set("received_by", engine::Value::text(job.received_by));
    row.set("delivery_signature_ref", engine::Value::text(job.delivery_signature_ref));
    row.set("proof_approval_ref", engine::Value::text(job.proof_approval_ref));
    row.set("reprint_of_job_id", engine::Value::text(job.reprint_of_job_id));
    row.set("reprint_reason", engine::Value::text(job.reprint_reason));
    row.set("created_at", engine::Value::integer(job.created_at));
    row.set("created_by", engine::Value::text(job.created_by));
    row.set("started_at", engine::Value::integer(job.started_at));
    row.set("started_by", engine::Value::text(job.started_by));
    row.set("done_at", engine::Value::integer(job.done_at));
    row.set("done_by", engine::Value::text(job.done_by));
    row.set("cancelled_at", engine::Value::integer(job.cancelled_at));
    row.set("cancelled_by", engine::Value::text(job.cancelled_by));
    row.set("cancel_reason", engine::Value::text(job.cancel_reason));
    return row;
}

Job job_from_row(const engine::Row& row) {
    Job job;
    job.id = row.get("id").text_or({});
    job.party_id = row.get("party_id").text_or({});
    job.source_order_id = row.get("source_order_id").text_or({});
    job.source_order_line_id = row.get("source_order_line_id").text_or({});
    job.source_quotation_id = row.get("source_quotation_id").text_or({});
    const std::int64_t state = row.get("state").integer_or(3);
    job.state = state == 0 ? JobState::Draft
              : state == 1 ? JobState::InProgress
              : state == 2 ? JobState::Done
                           : JobState::Cancelled;
    job.ticket_series = row.get("ticket_series").text_or({});
    job.ticket_number = ticket_number_from(row);
    job.thin = row.get("thin").boolean_or(false);
    job.title = row.get("title").text_or({});
    job.description = row.get("description").text_or({});
    job.specifications = row.get("specifications").text_or({});
    job.quantity_scaled = row.get("quantity_scaled").integer_or(0);
    job.unit_price_minor = row.get("unit_price_minor").integer_or(0);
    job.total_price_minor = row.get("total_price_minor").integer_or(0);
    job.rate_origin = origin_from(row.get("rate_origin").integer_or(3));
    job.rate_reason = row.get("rate_reason").text_or({});
    job.promised_at = row.get("promised_at").integer_or(0);
    job.deadline_at = row.get("deadline_at").integer_or(0);
    job.note = row.get("note").text_or({});
    job.commercial = commercial_from(row.get("commercial").integer_or(2));
    job.design = design_from(row.get("design").integer_or(2));
    job.production = production_from(row.get("production").integer_or(2));
    job.fulfilment = fulfilment_from(row.get("fulfilment").integer_or(2));
    job.payment = payment_from(row.get("payment").integer_or(3));
    job.material_reference = row.get("material_reference").text_or({});
    job.delivered_at = row.get("delivered_at").integer_or(0);
    job.delivered_by = row.get("delivered_by").text_or({});
    job.received_by = row.get("received_by").text_or({});
    job.delivery_signature_ref = row.get("delivery_signature_ref").text_or({});
    job.proof_approval_ref = row.get("proof_approval_ref").text_or({});
    job.reprint_of_job_id = row.get("reprint_of_job_id").text_or({});
    job.reprint_reason = row.get("reprint_reason").text_or({});
    job.created_at = row.get("created_at").integer_or(0);
    job.created_by = row.get("created_by").text_or({});
    job.started_at = row.get("started_at").integer_or(0);
    job.started_by = row.get("started_by").text_or({});
    job.done_at = row.get("done_at").integer_or(0);
    job.done_by = row.get("done_by").text_or({});
    job.cancelled_at = row.get("cancelled_at").integer_or(0);
    job.cancelled_by = row.get("cancelled_by").text_or({});
    job.cancel_reason = row.get("cancel_reason").text_or({});
    return job;
}

}  // namespace squiflow::modules::jobs
