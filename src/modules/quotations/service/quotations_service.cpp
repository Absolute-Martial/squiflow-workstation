#include "modules/quotations/service/quotations_service.hpp"

#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/quotations/data/repository.hpp"

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

engine::Row fields(const Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw RuleViolation("This request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("quotations: a write arrived with no session");
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
                          const std::string& name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) {
        throw RuleViolation(complaint);
    }
    return *value;
}

std::string optional_text(const engine::Row& row,
                          const std::string& name,
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
                             const std::string& name,
                             const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value) {
        throw RuleViolation(complaint);
    }
    return *value;
}

std::int64_t optional_number(const engine::Row& row,
                             const std::string& name,
                             const char* complaint,
                             std::int64_t fallback = 0) {
    if (!row.has(name)) {
        return fallback;
    }
    return required_number(row, name, complaint);
}

engine::RateOrigin origin_from(std::int64_t value) {
    switch (value) {
        case 0: return engine::RateOrigin::CatalogDefault;
        case 1: return engine::RateOrigin::PartySpecific;
        case 2: return engine::RateOrigin::Agreement;
        case 3: return engine::RateOrigin::ManualOverride;
        case 4: return engine::RateOrigin::OffCatalog;
        default: throw RuleViolation("That quotation line price origin is not understood.");
    }
}

// Lines arrive as indexed fields on one flat payload: line_count, then
// line.0.id, line.0.description and so on. The whole set is read before
// anything is written, so a bad line at the end cannot leave the good ones
// behind it already saved.
std::vector<QuotationLine> read_lines(const engine::Row& row,
                                      const std::string& quotation_id,
                                      const std::string& revision_id) {
    const std::int64_t count = required_number(
        row, "line_count", "This quotation does not say how many lines it has.");
    if (count <= 0) {
        throw RuleViolation("A quotation must offer at least one line.");
    }
    if (count > kMaxLines) {
        throw RuleViolation("That quotation has more lines than this shop can record.");
    }

    std::vector<QuotationLine> lines;
    lines.reserve(static_cast<std::size_t>(count));
    std::set<std::string> seen;

    for (std::int64_t index = 0; index < count; ++index) {
        const std::string prefix = "line." + std::to_string(index) + ".";

        QuotationLine line;
        line.id = required_text(row, prefix + "id", "Every quotation line needs its own record.");
        if (!seen.insert(line.id).second) {
            throw RuleViolation("That quotation lists the same line twice.");
        }
        line.quotation_id = quotation_id;
        line.revision_id = revision_id;
        line.position = optional_number(
            row, prefix + "position", "That line position could not be read as a number.", index);
        line.product_id = optional_text(
            row, prefix + "product_id", "That line product could not be read as a product record.");
        line.description = optional_text(
            row, prefix + "description", "That line description could not be read as text.");
        line.specifications = optional_text(
            row, prefix + "specifications", "Those line specifications could not be read as text.");
        line.quantity_scaled = required_number(
            row, prefix + "quantity_scaled", "That line quantity could not be read as a number.");
        line.unit_price_minor = required_number(
            row, prefix + "unit_price_minor", "That line price could not be read as a number.");
        line.rate_reason = optional_text(
            row, prefix + "rate_reason", "That line price reason could not be read as text.");

        // An unstated origin is inferred from whether a catalog product was
        // named, rather than defaulting to "catalog default" and quietly
        // mislabelling a one-off line as a catalog sale.
        if (row.has(prefix + "rate_origin")) {
            line.rate_origin = origin_from(required_number(
                row, prefix + "rate_origin", "That line price origin could not be read as a number."));
        } else {
            line.rate_origin = line.product_id.empty() ? engine::RateOrigin::OffCatalog
                                                       : engine::RateOrigin::CatalogDefault;
        }

        const engine::MoneyResult amount = line_amount(line);
        if (!amount.ok) {
            throw RuleViolation("That quotation line amount is too large to store safely.");
        }
        line.amount_minor = amount.value.minor;

        // Checked here as well as on save, so that a bad line is refused
        // before a single row of this document has been written.
        validate(line);
        lines.push_back(std::move(line));
    }

    return lines;
}

std::int64_t total_of(const std::vector<QuotationLine>& lines) {
    const engine::MoneyResult total = revision_total(lines);
    if (!total.ok) {
        throw RuleViolation("That quotation total is too large to store safely.");
    }
    return total.value.minor;
}

Quotation existing_quotation(const engine::Transaction& transaction, const std::string& id) {
    const auto quotation = data::find_quotation(transaction, id);
    if (!quotation) {
        throw RuleViolation("That quotation is not on file.");
    }
    validate(*quotation);
    return *quotation;
}

QuotationRevision live_revision(const engine::Transaction& transaction,
                                const std::string& quotation_id) {
    const auto revision = data::latest_revision(transaction, quotation_id);
    if (!revision) {
        // A quotation always creates its first revision in the same
        // transaction, so a head with no stack is a broken database rather
        // than something the person did.
        throw std::logic_error("quotations: a quotation exists with no revision");
    }
    return *revision;
}

void write_lines(engine::Transaction& transaction, const std::vector<QuotationLine>& lines) {
    for (const QuotationLine& line : lines) {
        data::save_line(transaction, line);
    }
}

}  // namespace

void QuotationsService::create(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    if (data::find_quotation(transaction, id)) {
        throw RuleViolation("That quotation has already been recorded.");
    }

    const std::string revision_id = required_text(
        row, "revision_id", "This quotation does not identify its first revision.");
    if (revision_id == id) {
        throw RuleViolation("A quotation and its revision cannot share one record.");
    }
    if (data::find_revision(transaction, revision_id)) {
        throw RuleViolation("That quotation revision has already been recorded.");
    }

    const std::int64_t now = clock_();
    const std::string person = engine::to_string(who.person);

    Quotation quotation;
    quotation.id = id;
    quotation.party_id = optional_text(
        row, "party_id", "That customer could not be read as a customer record.");
    quotation.state = QuotationState::Draft;
    quotation.current_revision = 1;
    quotation.customer_reference = optional_text(
        row, "customer_reference", "That customer reference could not be read as text.");
    quotation.note = optional_text(row, "note", "That quotation note could not be read as text.");
    quotation.created_at = now;
    quotation.created_by = person;

    QuotationRevision revision;
    revision.id = revision_id;
    revision.quotation_id = id;
    revision.revision = 1;
    revision.issued = false;
    revision.valid_until = optional_number(
        row, "valid_until", "That validity date could not be read as a number.");
    revision.terms = optional_text(row, "terms", "Those terms could not be read as text.");
    revision.created_at = now;
    revision.created_by = person;

    const std::vector<QuotationLine> lines = read_lines(row, id, revision_id);
    revision.total_minor = total_of(lines);

    data::save_quotation(transaction, quotation);
    data::save_revision(transaction, revision);
    write_lines(transaction, lines);
}

void QuotationsService::revise(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Quotation quotation = existing_quotation(transaction, id);
    if (!can_revise(quotation.state)) {
        throw RuleViolation("An accepted or expired quotation cannot be revised.");
    }

    const QuotationRevision current = live_revision(transaction, id);
    const std::int64_t now = clock_();
    const std::string person = engine::to_string(who.person);

    QuotationRevision target;
    if (current.issued) {
        // The customer already holds this one on paper. It is left exactly as
        // it is and a new revision is stacked on top.
        const std::string next_id = required_text(
            row, "revision_id", "A new quotation revision needs its own record.");
        if (next_id == current.id || next_id == id) {
            throw RuleViolation("A new quotation revision cannot reuse an existing record.");
        }
        if (data::find_revision(transaction, next_id)) {
            throw RuleViolation("That quotation revision has already been recorded.");
        }
        target.id = next_id;
        target.quotation_id = id;
        target.revision = current.revision + 1;
        target.created_at = now;
        target.created_by = person;
        target.valid_until = current.valid_until;
        target.terms = current.terms;
        quotation.current_revision = target.revision;
    } else {
        // Still a draft: nothing has left the building, so it is edited in
        // place and its old lines go with it.
        target = current;
        data::remove_lines_for_revision(transaction, target.id);
    }

    target.issued = false;
    target.series.clear();
    target.number = 0;
    target.issued_at = 0;
    target.issued_by.clear();

    if (row.has("valid_until")) {
        target.valid_until = required_number(
            row, "valid_until", "That validity date could not be read as a number.");
    }
    if (row.has("terms")) {
        target.terms = optional_text(row, "terms", "Those terms could not be read as text.");
    }
    if (row.has("customer_reference")) {
        quotation.customer_reference = optional_text(
            row, "customer_reference", "That customer reference could not be read as text.");
    }
    if (row.has("note")) {
        quotation.note = optional_text(row, "note", "That quotation note could not be read as text.");
    }

    const std::vector<QuotationLine> lines = read_lines(row, id, target.id);
    target.total_minor = total_of(lines);

    // A revised quotation is an open offer again until it is issued afresh.
    quotation.state = QuotationState::Draft;

    data::save_quotation(transaction, quotation);
    data::save_revision(transaction, target);
    write_lines(transaction, lines);
}

void QuotationsService::issue(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Quotation quotation = existing_quotation(transaction, id);
    if (!can_revise(quotation.state)) {
        throw RuleViolation("An accepted or expired quotation cannot be issued again.");
    }

    QuotationRevision revision = live_revision(transaction, id);
    if (revision.issued) {
        throw RuleViolation("That quotation revision has already been issued.");
    }

    // An offer with nothing on it must never acquire a number.
    if (data::lines_for_revision(transaction, revision.id).empty()) {
        throw RuleViolation("A quotation with no lines cannot be issued.");
    }

    const std::string series = required_text(
        row, "series", "An issued quotation needs its number series.");
    const std::int64_t number = required_number(
        row, "number", "An issued quotation needs its number.");
    if (number <= 0) {
        throw RuleViolation("An issued quotation must have a positive number.");
    }
    if (data::number_taken(transaction, series, number, revision.id)) {
        throw RuleViolation("That quotation number has already been used.");
    }

    const std::int64_t now = clock_();

    if (row.has("valid_until")) {
        revision.valid_until = required_number(
            row, "valid_until", "That validity date could not be read as a number.");
    }
    if (revision.valid_until > 0 && revision.valid_until < now) {
        throw RuleViolation("A quotation cannot be issued with a validity date already past.");
    }

    revision.issued = true;
    revision.series = series;
    revision.number = static_cast<std::uint64_t>(number);
    revision.issued_at = now;
    revision.issued_by = engine::to_string(who.person);

    if (!transition_allowed(quotation.state, QuotationState::Issued)) {
        throw RuleViolation("That quotation cannot be issued from where it stands.");
    }
    quotation.state = QuotationState::Issued;

    data::save_revision(transaction, revision);
    data::save_quotation(transaction, quotation);
}

void QuotationsService::accept(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Quotation quotation = existing_quotation(transaction, id);
    if (quotation.state == QuotationState::Accepted) {
        throw RuleViolation("That quotation has already been accepted.");
    }
    if (quotation.state == QuotationState::Expired) {
        throw RuleViolation("An expired quotation cannot be accepted.");
    }
    if (!transition_allowed(quotation.state, QuotationState::Accepted)) {
        throw RuleViolation("A quotation must be issued before it can be accepted.");
    }

    // Which revision, exactly. Acceptance is never inferred: the customer
    // agreed to one specific piece of paper and the record has to say which.
    const std::int64_t number = required_number(
        row, "revision", "This acceptance does not say which revision was accepted.");
    const auto revision = data::revision_numbered(transaction, id, number);
    if (!revision) {
        throw RuleViolation("That quotation revision is not on file.");
    }
    if (!revision->issued) {
        throw RuleViolation("A quotation revision that was never issued cannot be accepted.");
    }

    const std::int64_t now = clock_();
    if (expired_at_moment(*revision, now)) {
        throw RuleViolation("That quotation has passed its validity date and cannot be accepted.");
    }

    quotation.state = QuotationState::Accepted;
    quotation.accepted_revision = revision->revision;
    quotation.accepted_at = now;
    quotation.accepted_by = engine::to_string(who.person);

    data::save_quotation(transaction, quotation);
}

void QuotationsService::expire(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Quotation quotation = existing_quotation(transaction, id);
    if (quotation.state == QuotationState::Expired) {
        throw RuleViolation("That quotation has already expired.");
    }
    if (quotation.state == QuotationState::Accepted) {
        throw RuleViolation("An accepted quotation cannot be expired.");
    }
    if (!transition_allowed(quotation.state, QuotationState::Expired)) {
        throw RuleViolation("That quotation cannot be expired from where it stands.");
    }

    quotation.state = QuotationState::Expired;
    quotation.expired_at = clock_();
    quotation.expired_by = engine::to_string(who.person);
    quotation.expiry_reason = optional_text(
        row, "reason", "That expiry reason could not be read as text.");

    data::save_quotation(transaction, quotation);
}

}  // namespace squiflow::modules::quotations
