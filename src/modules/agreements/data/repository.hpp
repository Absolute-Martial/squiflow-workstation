#pragma once

// Reads are templates over the reader so the same function serves a plain
// Store and an open Transaction. A handler mid-write must see its own
// uncommitted rows; a screen listing agreements must not need a transaction to
// do it.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <set>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/agreements/data/tables.hpp"
#include "modules/agreements/domain/agreement.hpp"
#include "modules/agreements/domain/consumption.hpp"

namespace squiflow::modules::agreements::data {

template <typename Reader>
std::optional<Agreement> find_agreement(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kAgreement, id);
    return row ? std::optional<Agreement>{agreement_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<AgreementLine> find_line(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kLine, id);
    return row ? std::optional<AgreementLine>{line_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<AgreementConsumption> find_consumption(
    const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kConsumption, id);
    return row ? std::optional<AgreementConsumption>{consumption_from_row(*row)}
               : std::nullopt;
}

template <typename Reader>
std::vector<AgreementConsumption> consumptions_for_invoice(
    const Reader& reader, const std::string& invoice_id) {
    engine::Query query{tables::kConsumption};
    query.where_equals("invoice_id", engine::Value::text(invoice_id));
    query.order_by("consumed_at"); query.order_by("id");
    std::vector<AgreementConsumption> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(consumption_from_row(row));
    }
    return result;
}

// The agreed rates of one agreement, in the order they are shown. Position
// first, then id, so two devices print the same page even when positions
// collide.
template <typename Reader>
std::vector<AgreementLine> lines_for_agreement(const Reader& reader,
                                               const std::string& agreement_id) {
    engine::Query query{tables::kLine};
    query.where_equals("agreement_id", engine::Value::text(agreement_id));
    query.order_by("position");
    query.order_by("id");
    std::vector<AgreementLine> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(line_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<Agreement> agreements_for_party(const Reader& reader,
                                            const std::string& party_id) {
    engine::Query query{tables::kAgreement};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.order_by("valid_from");
    query.order_by("id");
    std::vector<Agreement> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(agreement_from_row(row));
    }
    return result;
}

// The agreements actually in force for a customer. A draft has not been agreed
// yet and a closed one no longer prices anything, so neither belongs in the
// list a job would price against.
template <typename Reader>
std::vector<Agreement> open_agreements_for_party(const Reader& reader,
                                                 const std::string& party_id) {
    engine::Query query{tables::kAgreement};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.where_equals("state",
                       engine::Value::integer(static_cast<std::int64_t>(AgreementState::Open)));
    query.order_by("valid_from");
    query.order_by("id");
    std::vector<Agreement> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(agreement_from_row(row));
    }
    return result;
}

// Every line in one agreement that prices this product. Deliberately a list
// and not a single answer: the same product may be listed twice under two
// agreed names at two different rates, on purpose, and nothing here may merge
// them or pick one on the shopkeeper's behalf.
template <typename Reader>
std::vector<AgreementLine> lines_for_product(const Reader& reader,
                                             const std::string& agreement_id,
                                             const std::string& product_id) {
    engine::Query query{tables::kLine};
    query.where_equals("agreement_id", engine::Value::text(agreement_id));
    query.where_equals("product_id", engine::Value::text(product_id));
    query.order_by("position");
    query.order_by("id");
    std::vector<AgreementLine> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(line_from_row(row));
    }
    return result;
}

// Open agreements already past their date at the moment given, earliest
// deadline first.
//
// Open-ended agreements are excluded by the query rather than filtered out
// afterwards: an agreement with no end date does not lapse, and must never
// appear on a list of things that have.
template <typename Reader>
std::vector<Agreement> agreements_lapsed_by(const Reader& reader, std::int64_t at) {
    engine::Query query{tables::kAgreement};
    query.where_equals("state",
                       engine::Value::integer(static_cast<std::int64_t>(AgreementState::Open)));
    query.where("valid_until", engine::Comparison::Greater, engine::Value::integer(0));
    query.where("valid_until", engine::Comparison::Less, engine::Value::integer(at));
    query.order_by("valid_until");
    query.order_by("id");
    std::vector<Agreement> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(agreement_from_row(row));
    }
    return result;
}

// Open agreements whose date falls inside the warning window and has not yet
// passed. This is what warns the shopkeeper before a rate lapses rather than
// after.
template <typename Reader>
std::vector<Agreement> agreements_expiring_by(const Reader& reader,
                                              std::int64_t at,
                                              std::int64_t window) {
    engine::Query query{tables::kAgreement};
    query.where_equals("state",
                       engine::Value::integer(static_cast<std::int64_t>(AgreementState::Open)));
    query.where("valid_until", engine::Comparison::Greater, engine::Value::integer(0));
    query.order_by("valid_until");
    query.order_by("id");
    std::vector<Agreement> result;
    for (const engine::Row& row : reader.select(query)) {
        const Agreement agreement = agreement_from_row(row);
        if (expiring_at_moment(agreement, at, window)) {
            result.push_back(agreement);
        }
    }
    return result;
}

// Lines that have reached or passed their cap, so the shop is warned before it
// commits work it did not agree a price for. Uncapped lines can never appear
// here, however much is consumed against them.
template <typename Reader>
std::vector<AgreementLine> lines_needing_attention(const Reader& reader,
                                                   const std::string& agreement_id) {
    std::vector<AgreementLine> result;
    for (const AgreementLine& line : lines_for_agreement(reader, agreement_id)) {
        const CapState state = cap_state(line);
        if (state.nearing || state.exceeded) {
            result.push_back(line);
        }
    }
    return result;
}

// The chain of agreements, following supersession forward from the one given.
//
// A cycle here would hang any screen that reads the history end to end, and a
// cycle is exactly what a bad sync merge could write. So the walk carries the
// set of records it has already seen and stops the moment it meets one twice,
// rather than trusting the data to be acyclic.
template <typename Reader>
std::vector<Agreement> agreement_chain(const Reader& reader, const std::string& start_id) {
    std::vector<Agreement> chain;
    std::set<std::string> seen;
    std::string current = start_id;

    while (!current.empty() && seen.insert(current).second) {
        const std::optional<Agreement> agreement = find_agreement(reader, current);
        if (!agreement) {
            break;
        }
        chain.push_back(*agreement);
        current = agreement->superseded_by;
    }
    return chain;
}

// Writes. A transaction is required, and the caller cannot open one, so a
// write outside the gate above is not expressible here.
void save_agreement(engine::Transaction& transaction, const Agreement& agreement);
void save_line(engine::Transaction& transaction, const AgreementLine& line);
void save_consumption(engine::Transaction& transaction,
                      const AgreementConsumption& consumption);

// Clears the lines of an agreement that is being re-stated. Returns how many
// lines were removed.
std::size_t remove_lines_for_agreement(engine::Transaction& transaction,
                                       const std::string& agreement_id);

}  // namespace squiflow::modules::agreements::data
