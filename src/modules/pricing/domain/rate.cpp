#include "modules/pricing/domain/rate.hpp"

#include "modules/context.hpp"

namespace squiflow::modules::pricing {
namespace {

// Text that is empty or only spaces is not a value a person typed on purpose.
bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

void check_amount(std::int64_t amount, const char* negative, const char* absurd) {
    if (amount < 0) {
        throw RuleViolation(negative);
    }
    if (amount > kMaxAmountMinor) {
        throw RuleViolation(absurd);
    }
}

// How specific a rate is. Higher wins. Zero means it does not apply at all.
int specificity(const Rate& rate, const std::string& party_id) noexcept {
    if (!rate.party_id.empty()) {
        // A rate naming a party applies to that party and nobody else. In
        // particular it must not leak to a walk-in customer with no party.
        return (!party_id.empty() && rate.party_id == party_id) ? 2 : 0;
    }
    return 1;
}

// Strict ordering between two applicable candidates of any specificity.
// Every field is compared, so the winner never depends on the order rows
// arrived in.
bool beats(const Rate& candidate, int candidate_rank, const Rate& best, int best_rank) noexcept {
    if (candidate_rank != best_rank) {
        return candidate_rank > best_rank;
    }
    if (candidate.valid_from != best.valid_from) {
        return candidate.valid_from > best.valid_from;
    }
    if (candidate.created_at != best.created_at) {
        return candidate.created_at > best.created_at;
    }
    return candidate.id > best.id;
}

}  // namespace

const char* to_string(RateSource source) noexcept {
    switch (source) {
        case RateSource::None:         return "none";
        case RateSource::PartyRate:    return "agreed rate";
        case RateSource::CatchAllRate: return "list rate";
        case RateSource::Default:      return "standard price";
    }
    // Reached only if a value from outside this build arrives here. Answering
    // with a marker beats reading past the end of a table.
    return "?";
}

bool covers(const Rate& rate, std::int64_t at) noexcept {
    // Half-open: the start is included, the end is not, so two consecutive
    // windows sharing a boundary cannot both match the same instant.
    if (rate.valid_from != 0 && at < rate.valid_from) {
        return false;
    }
    if (rate.valid_until != 0 && at >= rate.valid_until) {
        return false;
    }
    return true;
}

ResolvedRate choose_rate(const std::vector<Rate>& candidates,
                         const std::string& party_id,
                         std::int64_t at) {
    const Rate* best = nullptr;
    int best_rank = 0;

    for (const Rate& candidate : candidates) {
        const int rank = specificity(candidate, party_id);
        if (rank == 0) {
            continue;  // belongs to a different party
        }
        if (!covers(candidate, at)) {
            continue;  // outside its window
        }
        if (best == nullptr || beats(candidate, rank, *best, best_rank)) {
            best = &candidate;
            best_rank = rank;
        }
    }

    if (best == nullptr) {
        return {};
    }

    ResolvedRate resolved;
    resolved.source = (best_rank == 2) ? RateSource::PartyRate : RateSource::CatchAllRate;
    resolved.amount_minor = best->amount_minor;
    resolved.rate_id = best->id;
    return resolved;
}

ResolvedRate with_default(const ResolvedRate& chosen, const DefaultRate& fallback) {
    if (chosen.found()) {
        return chosen;
    }
    // An absent default arrives as a default-constructed DefaultRate. Treating
    // that as a price of zero would hand out free goods, so it stays None and
    // the caller is told nothing was found.
    if (fallback.product_id.empty()) {
        return {};
    }

    ResolvedRate resolved;
    resolved.source = RateSource::Default;
    resolved.amount_minor = fallback.amount_minor;
    return resolved;
}

void validate(const Rate& rate) {
    if (rate.id.empty()) {
        throw RuleViolation("This rate has no record to be saved under.");
    }
    if (rate.product_id.empty()) {
        throw RuleViolation("A rate must say which product it is for.");
    }
    check_amount(rate.amount_minor,
                 "A rate cannot be negative.",
                 "That rate is too large to be a real price. Check the decimal point.");
    if (rate.valid_from < 0 || rate.valid_until < 0) {
        throw RuleViolation("That rate's dates are not a time this shop has existed.");
    }
    // Both bounds are optional; zero means open-ended. If both are given the
    // window must be able to contain a moment, which an end at or before the
    // start cannot.
    if (rate.valid_from != 0 && rate.valid_until != 0 && rate.valid_until <= rate.valid_from) {
        throw RuleViolation("The rate's end date must be after its start date.");
    }
}

void validate(const DefaultRate& rate) {
    if (rate.product_id.empty()) {
        throw RuleViolation("A standard price must say which product it is for.");
    }
    check_amount(rate.amount_minor,
                 "A standard price cannot be negative.",
                 "That price is too large to be real. Check the decimal point.");
}

void validate(const RateOverride& override_) {
    if (override_.id.empty()) {
        throw RuleViolation("This price change has no record to be saved under.");
    }
    if (override_.line_id.empty()) {
        throw RuleViolation("A price change must say which line it applies to.");
    }
    check_amount(override_.overridden_minor,
                 "A changed price cannot be negative.",
                 "That price is too large to be real. Check the decimal point.");
    // The reason is required, not merely recommended. A deviation from the
    // normal price that cannot be explained cannot be audited, and an
    // unauditable deviation is how money leaves a shop unnoticed.
    if (blank(override_.reason)) {
        throw RuleViolation("A price change needs a reason so that it can be checked later.");
    }
}

engine::Row to_row(const Rate& rate) {
    engine::Row row;
    row.set("id", engine::Value::text(rate.id));
    row.set("product_id", engine::Value::text(rate.product_id));
    row.set("party_id", engine::Value::text(rate.party_id));
    row.set("amount_minor", engine::Value::integer(rate.amount_minor));
    row.set("valid_from", engine::Value::integer(rate.valid_from));
    row.set("valid_until", engine::Value::integer(rate.valid_until));
    row.set("created_at", engine::Value::integer(rate.created_at));
    row.set("created_by", engine::Value::text(rate.created_by));
    return row;
}

Rate rate_from_row(const engine::Row& row) {
    Rate rate;
    rate.id = row.get("id").text_or({});
    rate.product_id = row.get("product_id").text_or({});
    rate.party_id = row.get("party_id").text_or({});
    rate.amount_minor = row.get("amount_minor").integer_or(0);
    rate.valid_from = row.get("valid_from").integer_or(0);
    rate.valid_until = row.get("valid_until").integer_or(0);
    rate.created_at = row.get("created_at").integer_or(0);
    rate.created_by = row.get("created_by").text_or({});
    return rate;
}

engine::Row to_row(const DefaultRate& rate) {
    engine::Row row;
    row.set("product_id", engine::Value::text(rate.product_id));
    row.set("amount_minor", engine::Value::integer(rate.amount_minor));
    row.set("updated_at", engine::Value::integer(rate.updated_at));
    row.set("updated_by", engine::Value::text(rate.updated_by));
    return row;
}

DefaultRate default_rate_from_row(const engine::Row& row) {
    DefaultRate rate;
    rate.product_id = row.get("product_id").text_or({});
    rate.amount_minor = row.get("amount_minor").integer_or(0);
    rate.updated_at = row.get("updated_at").integer_or(0);
    rate.updated_by = row.get("updated_by").text_or({});
    return rate;
}

engine::Row to_row(const RateOverride& override_) {
    engine::Row row;
    row.set("id", engine::Value::text(override_.id));
    row.set("line_id", engine::Value::text(override_.line_id));
    row.set("overridden_minor", engine::Value::integer(override_.overridden_minor));
    row.set("reason", engine::Value::text(override_.reason));
    row.set("applied_at", engine::Value::integer(override_.applied_at));
    row.set("applied_by", engine::Value::text(override_.applied_by));
    return row;
}

RateOverride override_from_row(const engine::Row& row) {
    RateOverride override_;
    override_.id = row.get("id").text_or({});
    override_.line_id = row.get("line_id").text_or({});
    override_.overridden_minor = row.get("overridden_minor").integer_or(0);
    override_.reason = row.get("reason").text_or({});
    override_.applied_at = row.get("applied_at").integer_or(0);
    override_.applied_by = row.get("applied_by").text_or({});
    return override_;
}

}  // namespace squiflow::modules::pricing
