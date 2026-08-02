#include "modules/quotations/domain/quotation.hpp"

#include "modules/context.hpp"

namespace squiflow::modules::quotations {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

// Reading a stored state falls to the most closed value rather than the most
// open one. A row damaged on disk must not read back as an editable draft that
// someone can then reprice.
QuotationState state_from(std::int64_t stored) noexcept {
    switch (stored) {
        case 0: return QuotationState::Draft;
        case 1: return QuotationState::Issued;
        case 2: return QuotationState::Accepted;
        case 3: return QuotationState::Expired;
        default: return QuotationState::Expired;
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

std::uint64_t number_from(const engine::Row& row) noexcept {
    const std::int64_t stored = row.get("number").integer_or(0);
    return stored > 0 ? static_cast<std::uint64_t>(stored) : 0;
}

}  // namespace

const char* to_string(QuotationState state) noexcept {
    switch (state) {
        case QuotationState::Draft: return "draft";
        case QuotationState::Issued: return "issued";
        case QuotationState::Accepted: return "accepted";
        case QuotationState::Expired: return "expired";
    }
    return "?";
}

bool transition_allowed(QuotationState from, QuotationState to) noexcept {
    switch (from) {
        case QuotationState::Draft:
            return to == QuotationState::Issued || to == QuotationState::Expired;
        case QuotationState::Issued:
            // Issued to Issued is not a no-op: it is a later revision being
            // issued while the offer is still live.
            return to == QuotationState::Issued ||
                   to == QuotationState::Accepted ||
                   to == QuotationState::Expired;
        case QuotationState::Accepted:
        case QuotationState::Expired:
            return false;
    }
    return false;
}

engine::MoneyResult line_amount(const QuotationLine& line) noexcept {
    return engine::money_multiply(engine::Money{line.unit_price_minor},
                                  engine::Quantity{line.quantity_scaled});
}

engine::MoneyResult revision_total(const std::vector<QuotationLine>& lines) noexcept {
    engine::MoneyResult running{true, engine::Money{0}};
    for (const QuotationLine& line : lines) {
        const engine::MoneyResult amount = line_amount(line);
        if (!amount.ok) {
            return {false, engine::Money{0}};
        }
        running = engine::money_add(running.value, amount.value);
        if (!running.ok) {
            return {false, engine::Money{0}};
        }
    }
    return running;
}

bool expired_at_moment(const QuotationRevision& revision, std::int64_t at) noexcept {
    return revision.valid_until > 0 && at > revision.valid_until;
}

void validate(const Quotation& quotation) {
    if (quotation.id.empty()) {
        throw RuleViolation("This quotation has no record to be saved under.");
    }
    if (quotation.created_at <= 0 || blank(quotation.created_by)) {
        throw RuleViolation("A quotation must record when and by whom it was created.");
    }
    if (!quotation.party_id.empty() && blank(quotation.party_id)) {
        throw RuleViolation("A quotation customer cannot be only whitespace.");
    }
    if (quotation.current_revision < 1) {
        throw RuleViolation("A quotation must have at least one revision.");
    }
    if (quotation.accepted_revision < 0 ||
        quotation.accepted_revision > quotation.current_revision) {
        throw RuleViolation("A quotation cannot accept a revision that does not exist.");
    }

    const bool has_accept_evidence =
        quotation.accepted_revision != 0 || quotation.accepted_at != 0 ||
        !blank(quotation.accepted_by);
    const bool has_expiry_evidence =
        quotation.expired_at != 0 || !blank(quotation.expired_by) ||
        !blank(quotation.expiry_reason);

    switch (quotation.state) {
        case QuotationState::Draft:
        case QuotationState::Issued:
            if (has_accept_evidence) {
                throw RuleViolation("A quotation that is still open cannot carry acceptance evidence.");
            }
            if (has_expiry_evidence) {
                throw RuleViolation("A quotation that is still open cannot carry expiry evidence.");
            }
            break;
        case QuotationState::Accepted:
            if (quotation.accepted_revision <= 0 || quotation.accepted_at <= 0 ||
                blank(quotation.accepted_by)) {
                throw RuleViolation(
                    "An accepted quotation must record which revision was accepted, when, and by whom.");
            }
            if (quotation.accepted_at < quotation.created_at) {
                throw RuleViolation("A quotation cannot be accepted before it existed.");
            }
            if (has_expiry_evidence) {
                throw RuleViolation("An accepted quotation cannot also be expired.");
            }
            break;
        case QuotationState::Expired:
            if (quotation.expired_at <= 0 || blank(quotation.expired_by)) {
                throw RuleViolation("An expired quotation must record when and by whom it was expired.");
            }
            if (quotation.expired_at < quotation.created_at) {
                throw RuleViolation("A quotation cannot expire before it existed.");
            }
            if (has_accept_evidence) {
                throw RuleViolation("An expired quotation cannot also carry acceptance evidence.");
            }
            break;
        default:
            throw RuleViolation("That quotation has a state this build does not understand.");
    }
}

void validate(const QuotationRevision& revision) {
    if (revision.id.empty() || revision.quotation_id.empty()) {
        throw RuleViolation("A quotation revision must belong to a quotation.");
    }
    if (revision.revision < 1) {
        throw RuleViolation("A quotation revision must be numbered from one.");
    }
    if (revision.created_at <= 0 || blank(revision.created_by)) {
        throw RuleViolation("A quotation revision must record when and by whom it was drafted.");
    }
    if (revision.total_minor < 0) {
        throw RuleViolation("A quotation revision cannot total a negative amount.");
    }
    if (revision.valid_until < 0) {
        throw RuleViolation("That validity date could not be understood.");
    }

    if (revision.issued) {
        if (blank(revision.series) || revision.number == 0) {
            throw RuleViolation("An issued quotation must carry its number.");
        }
        if (revision.issued_at <= 0 || blank(revision.issued_by)) {
            throw RuleViolation("An issued quotation must record when and by whom it was issued.");
        }
        if (revision.issued_at < revision.created_at) {
            throw RuleViolation("A quotation cannot be issued before it was drafted.");
        }
        if (revision.valid_until > 0 && revision.valid_until < revision.issued_at) {
            throw RuleViolation("A quotation cannot be issued after it has already expired.");
        }
    } else if (!blank(revision.series) || revision.number != 0 ||
               revision.issued_at != 0 || !blank(revision.issued_by)) {
        throw RuleViolation("A draft quotation revision cannot carry a number or issue evidence.");
    }
}

void validate(const QuotationLine& line) {
    if (line.id.empty() || line.revision_id.empty() || line.quotation_id.empty()) {
        throw RuleViolation("A quotation line must belong to a quotation revision.");
    }
    if (line.position < 0) {
        throw RuleViolation("A quotation line cannot sit at a negative position.");
    }
    if (blank(line.description)) {
        throw RuleViolation("A quotation line must say what is being offered.");
    }
    if (line.quantity_scaled <= 0) {
        throw RuleViolation("A quotation line must have a quantity greater than zero.");
    }
    if (line.unit_price_minor < 0 || line.amount_minor < 0) {
        throw RuleViolation("A quotation line cannot carry negative money.");
    }

    const engine::MoneyResult amount = line_amount(line);
    if (!amount.ok || amount.value.minor != line.amount_minor) {
        throw RuleViolation(
            "That quotation line amount does not match its quantity and unit price.");
    }

    switch (line.rate_origin) {
        case engine::RateOrigin::CatalogDefault:
        case engine::RateOrigin::PartySpecific:
        case engine::RateOrigin::Agreement:
        case engine::RateOrigin::ManualOverride:
        case engine::RateOrigin::OffCatalog:
            break;
        default:
            throw RuleViolation("That quotation line has a price origin this build does not understand.");
    }
    if ((line.rate_origin == engine::RateOrigin::Agreement ||
         line.rate_origin == engine::RateOrigin::ManualOverride) &&
        blank(line.rate_reason)) {
        throw RuleViolation("An agreed or manually changed quotation rate must keep its reason.");
    }

    // An off-catalog line and a catalog line are not interchangeable: reports
    // separate them, so the record has to be honest about which it is.
    if (line.product_id.empty() && line.rate_origin != engine::RateOrigin::OffCatalog) {
        throw RuleViolation("A line with no catalog product must be recorded as off-catalog.");
    }
    if (!line.product_id.empty() && line.rate_origin == engine::RateOrigin::OffCatalog) {
        throw RuleViolation("An off-catalog line cannot also name a catalog product.");
    }
}

engine::Row to_row(const Quotation& quotation) {
    engine::Row row;
    row.set("id", engine::Value::text(quotation.id));
    row.set("party_id", engine::Value::text(quotation.party_id));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(quotation.state)));
    row.set("current_revision", engine::Value::integer(quotation.current_revision));
    row.set("accepted_revision", engine::Value::integer(quotation.accepted_revision));
    row.set("customer_reference", engine::Value::text(quotation.customer_reference));
    row.set("note", engine::Value::text(quotation.note));
    row.set("created_at", engine::Value::integer(quotation.created_at));
    row.set("created_by", engine::Value::text(quotation.created_by));
    row.set("accepted_at", engine::Value::integer(quotation.accepted_at));
    row.set("accepted_by", engine::Value::text(quotation.accepted_by));
    row.set("expired_at", engine::Value::integer(quotation.expired_at));
    row.set("expired_by", engine::Value::text(quotation.expired_by));
    row.set("expiry_reason", engine::Value::text(quotation.expiry_reason));
    return row;
}

engine::Row to_row(const QuotationRevision& revision) {
    engine::Row row;
    row.set("id", engine::Value::text(revision.id));
    row.set("quotation_id", engine::Value::text(revision.quotation_id));
    row.set("revision", engine::Value::integer(revision.revision));
    row.set("issued", engine::Value::boolean(revision.issued));
    row.set("series", engine::Value::text(revision.series));
    row.set("number", engine::Value::integer(static_cast<std::int64_t>(revision.number)));
    row.set("valid_until", engine::Value::integer(revision.valid_until));
    row.set("terms", engine::Value::text(revision.terms));
    row.set("total_minor", engine::Value::integer(revision.total_minor));
    row.set("created_at", engine::Value::integer(revision.created_at));
    row.set("created_by", engine::Value::text(revision.created_by));
    row.set("issued_at", engine::Value::integer(revision.issued_at));
    row.set("issued_by", engine::Value::text(revision.issued_by));
    return row;
}

engine::Row to_row(const QuotationLine& line) {
    engine::Row row;
    row.set("id", engine::Value::text(line.id));
    row.set("revision_id", engine::Value::text(line.revision_id));
    row.set("quotation_id", engine::Value::text(line.quotation_id));
    row.set("position", engine::Value::integer(line.position));
    row.set("product_id", engine::Value::text(line.product_id));
    row.set("description", engine::Value::text(line.description));
    row.set("specifications", engine::Value::text(line.specifications));
    row.set("quantity_scaled", engine::Value::integer(line.quantity_scaled));
    row.set("unit_price_minor", engine::Value::integer(line.unit_price_minor));
    row.set("amount_minor", engine::Value::integer(line.amount_minor));
    row.set("rate_origin", engine::Value::integer(static_cast<std::int64_t>(line.rate_origin)));
    row.set("rate_reason", engine::Value::text(line.rate_reason));
    return row;
}

Quotation quotation_from_row(const engine::Row& row) {
    Quotation quotation;
    quotation.id = row.get("id").text_or({});
    quotation.party_id = row.get("party_id").text_or({});
    quotation.state = state_from(row.get("state").integer_or(3));
    quotation.current_revision = row.get("current_revision").integer_or(1);
    quotation.accepted_revision = row.get("accepted_revision").integer_or(0);
    quotation.customer_reference = row.get("customer_reference").text_or({});
    quotation.note = row.get("note").text_or({});
    quotation.created_at = row.get("created_at").integer_or(0);
    quotation.created_by = row.get("created_by").text_or({});
    quotation.accepted_at = row.get("accepted_at").integer_or(0);
    quotation.accepted_by = row.get("accepted_by").text_or({});
    quotation.expired_at = row.get("expired_at").integer_or(0);
    quotation.expired_by = row.get("expired_by").text_or({});
    quotation.expiry_reason = row.get("expiry_reason").text_or({});
    return quotation;
}

QuotationRevision revision_from_row(const engine::Row& row) {
    QuotationRevision revision;
    revision.id = row.get("id").text_or({});
    revision.quotation_id = row.get("quotation_id").text_or({});
    revision.revision = row.get("revision").integer_or(1);
    revision.issued = row.get("issued").boolean_or(false);
    revision.series = row.get("series").text_or({});
    revision.number = number_from(row);
    revision.valid_until = row.get("valid_until").integer_or(0);
    revision.terms = row.get("terms").text_or({});
    revision.total_minor = row.get("total_minor").integer_or(0);
    revision.created_at = row.get("created_at").integer_or(0);
    revision.created_by = row.get("created_by").text_or({});
    revision.issued_at = row.get("issued_at").integer_or(0);
    revision.issued_by = row.get("issued_by").text_or({});
    return revision;
}

QuotationLine line_from_row(const engine::Row& row) {
    QuotationLine line;
    line.id = row.get("id").text_or({});
    line.revision_id = row.get("revision_id").text_or({});
    line.quotation_id = row.get("quotation_id").text_or({});
    line.position = row.get("position").integer_or(0);
    line.product_id = row.get("product_id").text_or({});
    line.description = row.get("description").text_or({});
    line.specifications = row.get("specifications").text_or({});
    line.quantity_scaled = row.get("quantity_scaled").integer_or(0);
    line.unit_price_minor = row.get("unit_price_minor").integer_or(0);
    line.amount_minor = row.get("amount_minor").integer_or(0);
    line.rate_origin = origin_from(row.get("rate_origin").integer_or(3));
    line.rate_reason = row.get("rate_reason").text_or({});
    return line;
}

}  // namespace squiflow::modules::quotations
