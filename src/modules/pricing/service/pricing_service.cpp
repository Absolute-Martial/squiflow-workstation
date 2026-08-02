#include "modules/pricing/service/pricing_service.hpp"

#include <optional>
#include <stdexcept>
#include <utility>

#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"

namespace squiflow::modules::pricing {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

std::string required_text(const engine::Row& fields, const char* field, const char* complaint) {
    const std::string value = fields.get(field).text_or({});
    if (blank(value)) {
        throw RuleViolation(complaint);
    }
    return value;
}

// A price must arrive as a number. integer_or would quietly turn a price sent
// as text into the fallback, and a fallback of zero is a free item on somebody's
// invoice, so a non-integer is refused instead of defaulted.
std::int64_t required_amount(const engine::Row& fields, const char* field, const char* complaint) {
    const auto value = fields.get(field).as_integer();
    if (!value) {
        throw RuleViolation(complaint);
    }
    return *value;
}

// An absent time bound is not an error: zero means open-ended.
std::int64_t optional_moment(const engine::Row& fields, const char* field) {
    return fields.get(field).integer_or(0);
}

std::string subject(const Call& call) {
    if (call.record_id.empty()) {
        throw RuleViolation("This request does not say which record it is about.");
    }
    return call.record_id;
}

}  // namespace

engine::Row read_fields(const Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        // The bytes did not decode. The person cannot fix a wire format, but
        // they can be told the request did not survive the trip rather than
        // being shown a storage-layer exception.
        throw RuleViolation("This request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) {
        // The gate above refuses unauthenticated writes, so reaching here means
        // this module was wired up wrong. That is not a rule the user broke.
        throw std::logic_error("pricing: a write arrived with no session");
    }
    return *call.actor;
}

void PricingService::set_rate(engine::Transaction& transaction, const Call& call) const {
    const engine::Row fields = read_fields(call);
    const engine::Session& session = actor(call);

    Rate rate;
    rate.id = subject(call);
    rate.product_id = required_text(fields, "product_id", "A rate must say which product it is for.");
    rate.party_id = fields.get("party_id").text_or({});
    rate.amount_minor =
        required_amount(fields, "amount_minor", "A rate must have an amount.");
    rate.valid_from = optional_moment(fields, "valid_from");
    rate.valid_until = optional_moment(fields, "valid_until");

    const auto existing = data::find_rate(transaction, rate.id);
    if (existing) {
        // Correcting a rate is allowed; moving it to a different product is
        // not. The rate's history and its product are the same fact, and a rate
        // that changes product silently re-prices whatever already referred to
        // it.
        if (existing->product_id != rate.product_id) {
            throw RuleViolation("A rate cannot be moved to a different product. Record a new rate instead.");
        }
        rate.created_at = existing->created_at;
        rate.created_by = existing->created_by;
    } else {
        rate.created_at = clock_();
        rate.created_by = engine::to_string(session.person);
    }

    data::save_rate(transaction, rate);
}

void PricingService::remove_rate(engine::Transaction& transaction, const Call& call) const {
    (void)actor(call);
    const std::string id = subject(call);

    // Already gone is success, not a complaint. The same removal can arrive
    // twice after a retry, and the caller's intent is satisfied either way.
    data::remove_rate(transaction, id);
}

void PricingService::override_rate(engine::Transaction& transaction, const Call& call) const {
    const engine::Row fields = read_fields(call);
    const engine::Session& session = actor(call);

    RateOverride override_;
    override_.id = subject(call);
    override_.line_id =
        required_text(fields, "line_id", "A price change must say which line it applies to.");
    override_.overridden_minor =
        required_amount(fields, "overridden_minor", "A price change must have an amount.");
    override_.reason = fields.get("reason").text_or({});
    override_.applied_at = clock_();
    override_.applied_by = engine::to_string(session.person);

    // Overrides are permanent history, never edited. Reusing an id would
    // rewrite a recorded deviation, which is exactly the thing the reason field
    // exists to preserve.
    if (data::find_override(transaction, override_.id)) {
        throw RuleViolation("That price change has already been recorded.");
    }

    data::save_override(transaction, override_);
}

void PricingService::set_default_rate(engine::Transaction& transaction, const Call& call) const {
    const engine::Row fields = read_fields(call);
    const engine::Session& session = actor(call);

    DefaultRate rate;
    // The standard price is keyed by product, so the product is the record.
    rate.product_id = subject(call);
    rate.amount_minor =
        required_amount(fields, "amount_minor", "A standard price must have an amount.");
    rate.updated_at = clock_();
    rate.updated_by = engine::to_string(session.person);

    data::save_default_rate(transaction, rate);
}

}  // namespace squiflow::modules::pricing
