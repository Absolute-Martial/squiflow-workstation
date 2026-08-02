#include "modules/agreements/service/agreements_service.hpp"

#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/agreements/data/repository.hpp"

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

engine::Row fields(const Call& call) {
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError&) {
        throw RuleViolation("This request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) {
        throw std::logic_error("agreements: a write arrived with no session");
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

FallbackRule fallback_from(std::int64_t value) {
    switch (value) {
        case 0: return FallbackRule::CatalogPrice;
        case 1: return FallbackRule::RefuseOutsideScope;
        default: throw RuleViolation("That fallback rule is not understood.");
    }
}

// Closing asks explicitly what happens to jobs still running under the
// agreement, so the answer is read with no default. The two answers bill
// differently, and guessing on the shopkeeper's behalf is how a customer gets
// a surprise at the end of the month.
CloseEffect close_effect_from(const std::string& value) {
    if (value == "keep") {
        return CloseEffect::KeepAgreedRate;
    }
    if (value == "catalog") {
        return CloseEffect::RevertToCatalog;
    }
    throw RuleViolation(
        "Closing an agreement must say what happens to jobs still running under it.");
}

Agreement existing_agreement(const engine::Transaction& transaction, const std::string& id) {
    const auto agreement = data::find_agreement(transaction, id);
    if (!agreement) {
        throw RuleViolation("That agreement is not on file.");
    }
    validate(*agreement);
    return *agreement;
}

// Lines arrive as indexed fields on one flat payload: line_count, then
// line.0.id, line.0.product_id and so on. The whole set is read before
// anything is written, so a bad line at the end cannot leave the good ones
// before it already saved.
//
// Consumption is never read from the payload. A cap counter that could be
// typed in is a cap counter that means nothing; it moves only when work is
// created against it, so an existing line's consumed quantity is carried over
// here rather than restated by the caller.
std::vector<AgreementLine> read_lines(const engine::Row& row,
                                      const std::string& agreement_id,
                                      const std::map<std::string, std::int64_t>& consumed) {
    const std::int64_t count = required_number(
        row, "line_count", "This agreement does not say how many rates it sets.");
    if (count <= 0) {
        throw RuleViolation("An agreement must set at least one agreed rate.");
    }
    if (count > kMaxLines) {
        throw RuleViolation("That agreement has more lines than this shop can record.");
    }

    std::vector<AgreementLine> lines;
    lines.reserve(static_cast<std::size_t>(count));
    std::set<std::string> seen_ids;
    std::set<std::pair<std::string, std::string>> seen_names;

    for (std::int64_t index = 0; index < count; ++index) {
        const std::string prefix = "line." + std::to_string(index) + ".";

        AgreementLine line;
        line.id = required_text(row, prefix + "id", "Every agreed rate needs its own record.");
        if (!seen_ids.insert(line.id).second) {
            throw RuleViolation("That agreement lists the same line twice.");
        }
        line.agreement_id = agreement_id;
        line.position = optional_number(
            row, prefix + "position", "That line position could not be read as a number.", index);
        line.product_id = required_text(
            row, prefix + "product_id", "Every agreed rate must name the product it prices.");
        line.agreed_name = required_text(
            row, prefix + "agreed_name",
            "Every agreed rate must carry the name the customer agreed to.");
        line.specifications = optional_text(
            row, prefix + "specifications", "Those line specifications could not be read as text.");
        line.rate_minor = required_number(
            row, prefix + "rate_minor", "That agreed rate could not be read as a number.");
        line.cap_scaled = optional_number(
            row, prefix + "cap_scaled", "That quantity cap could not be read as a number.");
        line.rate_reason = optional_text(
            row, prefix + "rate_reason", "That rate reason could not be read as text.");

        // The same product may be listed twice under two different agreed
        // names at two different rates - that is a deliberate feature, and
        // nothing here may merge them. The same name twice for the same
        // product is not that; it is a mistake with no readable meaning.
        if (!seen_names.insert({line.product_id, line.agreed_name}).second) {
            throw RuleViolation("That agreement prices the same name twice for one product.");
        }

        const auto carried = consumed.find(line.id);
        line.consumed_scaled = carried == consumed.end() ? 0 : carried->second;

        // Checked here as well as on save, so that a bad line is refused
        // before a single row of this document has been written.
        validate(line);
        lines.push_back(std::move(line));
    }

    return lines;
}

std::map<std::string, std::int64_t> consumed_so_far(const engine::Transaction& transaction,
                                                    const std::string& agreement_id) {
    std::map<std::string, std::int64_t> consumed;
    for (const AgreementLine& line : data::lines_for_agreement(transaction, agreement_id)) {
        consumed.emplace(line.id, line.consumed_scaled);
    }
    return consumed;
}

void write_lines(engine::Transaction& transaction, const std::vector<AgreementLine>& lines) {
    for (const AgreementLine& line : lines) {
        data::save_line(transaction, line);
    }
}

// Following supersession forward from the successor must never arrive back at
// the record being superseded. A cycle here would hang any screen that reads
// the chain end to end, and a bad sync merge is entirely capable of writing
// one.
bool completes_a_cycle(const engine::Transaction& transaction,
                       const std::string& origin,
                       const std::string& successor) {
    std::set<std::string> seen;
    std::string current = successor;

    while (!current.empty() && seen.insert(current).second) {
        if (current == origin) {
            return true;
        }
        const auto next = data::find_agreement(transaction, current);
        if (!next) {
            return false;
        }
        current = next->superseded_by;
    }
    return false;
}

void apply_period(const engine::Row& row, Agreement& agreement) {
    if (row.has("valid_from")) {
        agreement.valid_from = required_number(
            row, "valid_from", "That start date could not be read as a number.");
    }
    if (row.has("valid_until")) {
        agreement.valid_until = required_number(
            row, "valid_until", "That end date could not be read as a number.");
    }
    if (agreement.valid_until > 0 && agreement.valid_until < agreement.valid_from) {
        throw RuleViolation("An agreement cannot end before it begins.");
    }
}

void apply_terms(const engine::Row& row, Agreement& agreement) {
    if (row.has("fallback")) {
        agreement.fallback = fallback_from(required_number(
            row, "fallback", "That fallback rule could not be read as a number."));
    }
    if (row.has("terms")) {
        agreement.terms = optional_text(row, "terms", "Those terms could not be read as text.");
    }
    if (row.has("customer_reference")) {
        agreement.customer_reference = optional_text(
            row, "customer_reference", "That customer reference could not be read as text.");
    }
    if (row.has("note")) {
        agreement.note = optional_text(row, "note", "That note could not be read as text.");
    }
    if (row.has("signed_by_name")) {
        agreement.signed_by_name = optional_text(
            row, "signed_by_name", "That signatory could not be read as text.");
    }
    if (row.has("signed_on")) {
        agreement.signed_on = required_number(
            row, "signed_on", "That signature date could not be read as a number.");
    }
    if (row.has("signed_artifact")) {
        agreement.signed_artifact = optional_text(
            row, "signed_artifact", "That signed copy could not be read as a file reference.");
    }
}

}  // namespace

void AgreementsService::create(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    if (data::find_agreement(transaction, id)) {
        throw RuleViolation("That agreement has already been recorded.");
    }

    const std::int64_t now = clock_();
    const std::string person = engine::to_string(who.person);

    Agreement agreement;
    agreement.id = id;
    agreement.party_id = required_text(
        row, "party_id", "An agreement must name the customer it is with.");

    // Always a draft. Striking a bargain and bringing it into force are two
    // acts, and only the second one changes what a job may be charged.
    agreement.state = AgreementState::Draft;
    agreement.valid_from = optional_number(
        row, "valid_from", "That start date could not be read as a number.", now);
    agreement.created_at = now;
    agreement.created_by = person;

    apply_period(row, agreement);
    apply_terms(row, agreement);

    agreement.source_quotation_id = optional_text(
        row, "source_quotation_id", "That quotation could not be read as a quotation record.");

    // The link to the predecessor is recorded now, but the predecessor is not
    // touched until this agreement actually comes into force. Superseding it
    // at draft time would leave a gap with nothing in force at all.
    agreement.supersedes = optional_text(
        row, "supersedes", "That earlier agreement could not be read as an agreement record.");
    if (!agreement.supersedes.empty()) {
        if (agreement.supersedes == id) {
            throw RuleViolation("An agreement cannot supersede itself.");
        }
        if (!data::find_agreement(transaction, agreement.supersedes)) {
            throw RuleViolation("That earlier agreement is not on file.");
        }
    }

    agreement.renewed_from = optional_text(
        row, "renewed_from", "That renewed agreement could not be read as an agreement record.");
    if (!agreement.renewed_from.empty() &&
        !data::find_agreement(transaction, agreement.renewed_from)) {
        throw RuleViolation("That renewed agreement is not on file.");
    }

    const std::vector<AgreementLine> lines = read_lines(row, id, {});

    data::save_agreement(transaction, agreement);
    write_lines(transaction, lines);
}

void AgreementsService::update(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Agreement agreement = existing_agreement(transaction, id);
    if (!can_amend(agreement.state)) {
        throw RuleViolation("A closed or superseded agreement cannot be amended.");
    }

    // The customer is the one thing that cannot change. An agreement is a
    // bargain with a named party, and moving it to another party would rewrite
    // who agreed to what.
    if (row.has("party_id")) {
        const std::string party = required_text(
            row, "party_id", "That customer could not be read as a customer record.");
        if (party != agreement.party_id) {
            throw RuleViolation("An agreement cannot be moved to a different customer.");
        }
    }

    const std::int64_t now = clock_();
    const std::string person = engine::to_string(who.person);
    const std::string action = optional_text(
        row, "action", "That action could not be read as text.");

    // Amending an agreement already in force changes what every later invoice
    // may charge, so it is not allowed to happen silently.
    const bool restating_rates = row.has("line_count");
    if (agreement.state == AgreementState::Open && restating_rates &&
        blank(optional_text(row, "reason", "That reason could not be read as text."))) {
        throw RuleViolation("Changing the rates of an agreement in force needs a reason.");
    }

    apply_period(row, agreement);
    apply_terms(row, agreement);

    if (action.empty() || action == "amend") {
        // Nothing more to do: the fields above are the amendment.
    } else if (action == "open") {
        if (!transition_allowed(agreement.state, AgreementState::Open)) {
            throw RuleViolation("That agreement is already in force.");
        }
        if (data::lines_for_agreement(transaction, id).empty() && !restating_rates) {
            throw RuleViolation("An agreement with no agreed rates cannot come into force.");
        }
        if (lapsed_at_moment(agreement, now)) {
            throw RuleViolation("An agreement cannot come into force after its end date.");
        }

        agreement.state = AgreementState::Open;
        agreement.opened_at = now;
        agreement.opened_by = person;

        // Now, and only now, the predecessor stops. The successor is in force
        // in the same transaction, so there is never a moment with neither.
        if (!agreement.supersedes.empty()) {
            Agreement earlier = existing_agreement(transaction, agreement.supersedes);
            if (!earlier.superseded_by.empty() && earlier.superseded_by != id) {
                throw RuleViolation("That earlier agreement has already been superseded.");
            }
            if (!transition_allowed(earlier.state, AgreementState::Superseded)) {
                throw RuleViolation("Only an agreement in force can be superseded.");
            }
            earlier.state = AgreementState::Superseded;
            earlier.superseded_by = id;
            data::save_agreement(transaction, earlier);
        }
    } else if (action == "supersede") {
        const std::string successor = required_text(
            row, "superseded_by",
            "Superseding an agreement must name the agreement that replaces it.");
        if (successor == id) {
            throw RuleViolation("An agreement cannot supersede itself.");
        }
        if (!transition_allowed(agreement.state, AgreementState::Superseded)) {
            throw RuleViolation("Only an agreement in force can be superseded.");
        }
        if (!data::find_agreement(transaction, successor)) {
            throw RuleViolation("That replacement agreement is not on file.");
        }
        if (completes_a_cycle(transaction, id, successor)) {
            throw RuleViolation("That would make the chain of agreements loop back on itself.");
        }

        agreement.state = AgreementState::Superseded;
        agreement.superseded_by = successor;
    } else {
        throw RuleViolation("That is not something that can be done to an agreement.");
    }

    if (restating_rates) {
        const std::map<std::string, std::int64_t> carried = consumed_so_far(transaction, id);
        const std::vector<AgreementLine> lines = read_lines(row, id, carried);
        data::remove_lines_for_agreement(transaction, id);
        write_lines(transaction, lines);
    }

    data::save_agreement(transaction, agreement);
}

void AgreementsService::close(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Agreement agreement = existing_agreement(transaction, id);
    if (!transition_allowed(agreement.state, AgreementState::Closed)) {
        throw RuleViolation("Only an agreement in force can be closed.");
    }

    agreement.close_reason = required_text(row, "reason", "Closing an agreement needs a reason.");
    agreement.close_effect = close_effect_from(required_text(
        row, "close_effect",
        "Closing an agreement must say what happens to jobs still running under it."));
    agreement.state = AgreementState::Closed;
    agreement.closed_at = clock_();
    agreement.closed_by = engine::to_string(who.person);

    data::save_agreement(transaction, agreement);
}

void AgreementsService::reopen(engine::Transaction& transaction, const Call& call) const {
    const engine::Row row = fields(call);
    const std::string id = subject(call);
    const engine::Session& who = actor(call);

    Agreement agreement = existing_agreement(transaction, id);
    if (!transition_allowed(agreement.state, AgreementState::Open)) {
        throw RuleViolation("Only a closed agreement can be reopened.");
    }

    const std::string reason =
        required_text(row, "reason", "Reopening an agreement needs a reason.");
    const std::int64_t now = clock_();

    // Reopening an agreement whose end date has already passed would put a
    // lapsed price list back into force. The new end date has to be stated in
    // the same breath.
    if (row.has("valid_until")) {
        agreement.valid_until = required_number(
            row, "valid_until", "That end date could not be read as a number.");
        if (agreement.valid_until > 0 && agreement.valid_until < agreement.valid_from) {
            throw RuleViolation("An agreement cannot end before it begins.");
        }
    }
    if (lapsed_at_moment(agreement, now)) {
        throw RuleViolation("Reopening an agreement past its end date needs a new end date.");
    }

    agreement.state = AgreementState::Open;
    agreement.reopen_reason = reason;
    agreement.reopened_at = now;
    agreement.reopened_by = engine::to_string(who.person);
    if (agreement.opened_at <= 0) {
        agreement.opened_at = now;
        agreement.opened_by = agreement.reopened_by;
    }

    data::save_agreement(transaction, agreement);
}

}  // namespace squiflow::modules::agreements
