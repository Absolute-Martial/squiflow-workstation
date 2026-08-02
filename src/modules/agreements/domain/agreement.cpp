#include "modules/agreements/domain/agreement.hpp"

#include <limits>

#include "modules/context.hpp"

namespace squiflow::modules::agreements {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

// Reading a stored state falls to the most closed value. A row damaged on disk
// must not read back as an open agreement that then prices somebody's work.
AgreementState state_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return AgreementState::Draft;
        case 1: return AgreementState::Open;
        case 2: return AgreementState::Closed;
        case 3: return AgreementState::Superseded;
        default: return AgreementState::Superseded;
    }
}

// Likewise the fallback: an unreadable rule refuses work outside the agreed
// list rather than quietly charging catalog price for it.
FallbackRule fallback_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return FallbackRule::CatalogPrice;
        case 1: return FallbackRule::RefuseOutsideScope;
        default: return FallbackRule::RefuseOutsideScope;
    }
}

CloseEffect close_effect_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return CloseEffect::KeepAgreedRate;
        case 1: return CloseEffect::RevertToCatalog;
        default: return CloseEffect::RevertToCatalog;
    }
}

// Nine tenths of the cap, computed so that it cannot overflow on the way. The
// division happens before the multiplication for exactly that reason.
std::int64_t nearing_threshold(std::int64_t cap) noexcept {
    const std::int64_t shortfall =
        (cap / kNearingDenominator) * (kNearingDenominator - kNearingNumerator);
    return cap - shortfall;
}

}  // namespace

const char* to_string(AgreementState state) noexcept {
    switch (state) {
        case AgreementState::Draft: return "draft";
        case AgreementState::Open: return "open";
        case AgreementState::Closed: return "closed";
        case AgreementState::Superseded: return "superseded";
    }
    return "?";
}

const char* to_string(FallbackRule rule) noexcept {
    switch (rule) {
        case FallbackRule::CatalogPrice: return "catalog price";
        case FallbackRule::RefuseOutsideScope: return "refuse outside scope";
    }
    return "?";
}

const char* to_string(CloseEffect effect) noexcept {
    switch (effect) {
        case CloseEffect::KeepAgreedRate: return "keep agreed rate";
        case CloseEffect::RevertToCatalog: return "revert to catalog";
    }
    return "?";
}

bool transition_allowed(AgreementState from, AgreementState to) noexcept {
    switch (from) {
        case AgreementState::Draft:
            // A draft is brought into force and nothing else. It cannot be
            // superseded, because nothing was ever in force to replace.
            return to == AgreementState::Open;
        case AgreementState::Open:
            return to == AgreementState::Closed || to == AgreementState::Superseded;
        case AgreementState::Closed:
            return to == AgreementState::Open;
        case AgreementState::Superseded:
            return false;
    }
    return false;
}

CapState cap_state(const AgreementLine& line) noexcept {
    CapState state;
    state.capped = line.cap_scaled > 0;
    state.agreed_scaled = line.cap_scaled;
    state.consumed_scaled = line.consumed_scaled;

    if (!state.capped) {
        return state;
    }

    state.exceeded = line.consumed_scaled > line.cap_scaled;
    state.remaining_scaled =
        state.exceeded ? 0 : line.cap_scaled - line.consumed_scaled;
    state.nearing =
        !state.exceeded && line.consumed_scaled >= nearing_threshold(line.cap_scaled);
    return state;
}

ConsumeResult consume_quantity(const AgreementLine& line,
                               std::int64_t quantity_scaled) noexcept {
    if (quantity_scaled <= 0) {
        return {};
    }
    if (line.consumed_scaled > std::numeric_limits<std::int64_t>::max() - quantity_scaled) {
        return {};
    }

    ConsumeResult result;
    result.ok = true;
    result.consumed_scaled = line.consumed_scaled + quantity_scaled;
    result.exceeds_cap = line.cap_scaled > 0 && result.consumed_scaled > line.cap_scaled;
    return result;
}

ConsumeResult release_quantity(const AgreementLine& line,
                               std::int64_t quantity_scaled) noexcept {
    if (quantity_scaled <= 0) {
        return {};
    }
    if (quantity_scaled > line.consumed_scaled) {
        return {};
    }

    ConsumeResult result;
    result.ok = true;
    result.consumed_scaled = line.consumed_scaled - quantity_scaled;
    result.exceeds_cap = line.cap_scaled > 0 && result.consumed_scaled > line.cap_scaled;
    return result;
}

bool has_expiry(const Agreement& agreement) noexcept {
    return agreement.valid_until > 0;
}

bool lapsed_at_moment(const Agreement& agreement, std::int64_t at) noexcept {
    return has_expiry(agreement) && at > agreement.valid_until;
}

bool expiring_at_moment(const Agreement& agreement, std::int64_t at,
                        std::int64_t window) noexcept {
    if (!has_expiry(agreement) || window < 0) {
        return false;
    }
    if (lapsed_at_moment(agreement, at)) {
        return false;
    }
    return agreement.valid_until - at <= window;
}

engine::MoneyResult capped_value(const AgreementLine& line) noexcept {
    if (line.cap_scaled <= 0) {
        return {true, engine::Money{0}};
    }
    return engine::money_multiply(engine::Money{line.rate_minor},
                                  engine::Quantity{line.cap_scaled});
}

void validate(const Agreement& agreement) {
    if (agreement.id.empty()) {
        throw RuleViolation("This agreement has no record to be saved under.");
    }
    if (blank(agreement.party_id)) {
        throw RuleViolation("An agreement must name the customer it is with.");
    }
    if (agreement.created_at <= 0 || blank(agreement.created_by)) {
        throw RuleViolation("An agreement must record when and by whom it was created.");
    }
    if (agreement.valid_from < 0 || agreement.valid_until < 0) {
        throw RuleViolation("An agreement cannot be valid at a negative moment.");
    }
    if (has_expiry(agreement) && agreement.valid_until < agreement.valid_from) {
        throw RuleViolation("An agreement cannot end before it begins.");
    }

    // The chain has to stay a chain. A record that supersedes itself is a loop
    // that any "read the history end to end" screen would hang on.
    if (!agreement.superseded_by.empty() && agreement.superseded_by == agreement.id) {
        throw RuleViolation("An agreement cannot supersede itself.");
    }
    if (!agreement.supersedes.empty() && agreement.supersedes == agreement.id) {
        throw RuleViolation("An agreement cannot supersede itself.");
    }
    if (!agreement.renewed_from.empty() && agreement.renewed_from == agreement.id) {
        throw RuleViolation("An agreement cannot be renewed from itself.");
    }

    switch (agreement.state) {
        case AgreementState::Draft:
            break;
        case AgreementState::Open:
            if (agreement.opened_at <= 0 || blank(agreement.opened_by)) {
                throw RuleViolation("An open agreement must record when it came into force.");
            }
            break;
        case AgreementState::Closed:
            if (agreement.closed_at <= 0 || blank(agreement.closed_by)) {
                throw RuleViolation("A closed agreement must record when it was closed.");
            }
            if (blank(agreement.close_reason)) {
                throw RuleViolation("Closing an agreement needs a reason.");
            }
            break;
        case AgreementState::Superseded:
            if (agreement.superseded_by.empty()) {
                throw RuleViolation(
                    "A superseded agreement must name the agreement that replaced it.");
            }
            break;
        default:
            throw RuleViolation("That agreement is in a state this build does not understand.");
    }

    // Signature evidence travels together or not at all. Half of it is worse
    // than none: it reads as signed without saying by whom or when.
    const bool has_signer = !blank(agreement.signed_by_name);
    const bool has_date = agreement.signed_on > 0;
    if (has_signer != has_date) {
        throw RuleViolation("A signed agreement must record both who signed it and when.");
    }
    if (!blank(agreement.signed_artifact) && !has_signer) {
        throw RuleViolation("A signed copy must belong to a recorded signature.");
    }
}

void validate(const AgreementLine& line) {
    if (line.id.empty() || line.agreement_id.empty()) {
        throw RuleViolation("An agreement line must belong to an agreement.");
    }
    if (line.position < 0) {
        throw RuleViolation("An agreement line cannot sit at a negative position.");
    }
    if (blank(line.product_id)) {
        throw RuleViolation("An agreement line must name the product it prices.");
    }
    if (blank(line.agreed_name)) {
        throw RuleViolation("An agreement line must carry the name the customer agreed to.");
    }
    if (line.rate_minor < 0) {
        throw RuleViolation("An agreed rate cannot be negative.");
    }
    if (line.cap_scaled < 0) {
        throw RuleViolation("A quantity cap cannot be negative.");
    }
    if (line.consumed_scaled < 0) {
        throw RuleViolation("Consumed quantity cannot be negative.");
    }

    // The cap is a warning line, not a wall: consumption may pass it with an
    // override. What it may not do is pass it without one recorded.
    if (line.cap_scaled > 0 && line.consumed_scaled > line.cap_scaled &&
        blank(line.rate_reason)) {
        throw RuleViolation("Going past a quantity cap needs a recorded reason.");
    }

    const engine::MoneyResult value = capped_value(line);
    if (!value.ok) {
        throw RuleViolation("That agreed rate and cap are too large to be held together.");
    }
}

engine::Row to_row(const Agreement& agreement) {
    engine::Row row;
    row.set("id", engine::Value::text(agreement.id));
    row.set("party_id", engine::Value::text(agreement.party_id));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(agreement.state)));
    row.set("source_quotation_id", engine::Value::text(agreement.source_quotation_id));
    row.set("supersedes", engine::Value::text(agreement.supersedes));
    row.set("superseded_by", engine::Value::text(agreement.superseded_by));
    row.set("renewed_from", engine::Value::text(agreement.renewed_from));
    row.set("valid_from", engine::Value::integer(agreement.valid_from));
    row.set("valid_until", engine::Value::integer(agreement.valid_until));
    row.set("fallback", engine::Value::integer(static_cast<std::int64_t>(agreement.fallback)));
    row.set("terms", engine::Value::text(agreement.terms));
    row.set("customer_reference", engine::Value::text(agreement.customer_reference));
    row.set("note", engine::Value::text(agreement.note));
    row.set("signed_by_name", engine::Value::text(agreement.signed_by_name));
    row.set("signed_on", engine::Value::integer(agreement.signed_on));
    row.set("signed_artifact", engine::Value::text(agreement.signed_artifact));
    row.set("created_at", engine::Value::integer(agreement.created_at));
    row.set("created_by", engine::Value::text(agreement.created_by));
    row.set("opened_at", engine::Value::integer(agreement.opened_at));
    row.set("opened_by", engine::Value::text(agreement.opened_by));
    row.set("closed_at", engine::Value::integer(agreement.closed_at));
    row.set("closed_by", engine::Value::text(agreement.closed_by));
    row.set("close_reason", engine::Value::text(agreement.close_reason));
    row.set("close_effect",
            engine::Value::integer(static_cast<std::int64_t>(agreement.close_effect)));
    row.set("reopened_at", engine::Value::integer(agreement.reopened_at));
    row.set("reopened_by", engine::Value::text(agreement.reopened_by));
    row.set("reopen_reason", engine::Value::text(agreement.reopen_reason));
    return row;
}

engine::Row to_row(const AgreementLine& line) {
    engine::Row row;
    row.set("id", engine::Value::text(line.id));
    row.set("agreement_id", engine::Value::text(line.agreement_id));
    row.set("position", engine::Value::integer(line.position));
    row.set("product_id", engine::Value::text(line.product_id));
    row.set("agreed_name", engine::Value::text(line.agreed_name));
    row.set("specifications", engine::Value::text(line.specifications));
    row.set("rate_minor", engine::Value::integer(line.rate_minor));
    row.set("cap_scaled", engine::Value::integer(line.cap_scaled));
    row.set("consumed_scaled", engine::Value::integer(line.consumed_scaled));
    row.set("rate_reason", engine::Value::text(line.rate_reason));
    return row;
}

Agreement agreement_from_row(const engine::Row& row) {
    Agreement agreement;
    agreement.id = row.get("id").text_or({});
    agreement.party_id = row.get("party_id").text_or({});
    agreement.state = state_from(row.get("state").integer_or(3));
    agreement.source_quotation_id = row.get("source_quotation_id").text_or({});
    agreement.supersedes = row.get("supersedes").text_or({});
    agreement.superseded_by = row.get("superseded_by").text_or({});
    agreement.renewed_from = row.get("renewed_from").text_or({});
    agreement.valid_from = row.get("valid_from").integer_or(0);
    agreement.valid_until = row.get("valid_until").integer_or(0);
    agreement.fallback = fallback_from(row.get("fallback").integer_or(1));
    agreement.terms = row.get("terms").text_or({});
    agreement.customer_reference = row.get("customer_reference").text_or({});
    agreement.note = row.get("note").text_or({});
    agreement.signed_by_name = row.get("signed_by_name").text_or({});
    agreement.signed_on = row.get("signed_on").integer_or(0);
    agreement.signed_artifact = row.get("signed_artifact").text_or({});
    agreement.created_at = row.get("created_at").integer_or(0);
    agreement.created_by = row.get("created_by").text_or({});
    agreement.opened_at = row.get("opened_at").integer_or(0);
    agreement.opened_by = row.get("opened_by").text_or({});
    agreement.closed_at = row.get("closed_at").integer_or(0);
    agreement.closed_by = row.get("closed_by").text_or({});
    agreement.close_reason = row.get("close_reason").text_or({});
    agreement.close_effect = close_effect_from(row.get("close_effect").integer_or(1));
    agreement.reopened_at = row.get("reopened_at").integer_or(0);
    agreement.reopened_by = row.get("reopened_by").text_or({});
    agreement.reopen_reason = row.get("reopen_reason").text_or({});
    return agreement;
}

AgreementLine line_from_row(const engine::Row& row) {
    AgreementLine line;
    line.id = row.get("id").text_or({});
    line.agreement_id = row.get("agreement_id").text_or({});
    line.position = row.get("position").integer_or(0);
    line.product_id = row.get("product_id").text_or({});
    line.agreed_name = row.get("agreed_name").text_or({});
    line.specifications = row.get("specifications").text_or({});
    line.rate_minor = row.get("rate_minor").integer_or(0);
    line.cap_scaled = row.get("cap_scaled").integer_or(0);
    line.consumed_scaled = row.get("consumed_scaled").integer_or(0);
    line.rate_reason = row.get("rate_reason").text_or({});
    return line;
}

}  // namespace squiflow::modules::agreements
