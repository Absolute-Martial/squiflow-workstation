#pragma once

// Reads are templates over the reader so the same function serves a plain
// Store and an open Transaction. A handler mid-write must see its own
// uncommitted rows; a screen listing quotations must not need a transaction to
// do it.

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/quotations/data/tables.hpp"
#include "modules/quotations/domain/quotation.hpp"

namespace squiflow::modules::quotations::data {

template <typename Reader>
std::optional<Quotation> find_quotation(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kQuotation, id);
    return row ? std::optional<Quotation>{quotation_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<QuotationRevision> find_revision(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kRevision, id);
    return row ? std::optional<QuotationRevision>{revision_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<QuotationLine> find_line(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kLine, id);
    return row ? std::optional<QuotationLine>{line_from_row(*row)} : std::nullopt;
}

// Every revision of one quotation, oldest first. The stack, in the order it
// was built, which is the order the shopkeeper shows the customer.
template <typename Reader>
std::vector<QuotationRevision> revisions_for_quotation(const Reader& reader,
                                                       const std::string& quotation_id) {
    engine::Query query{tables::kRevision};
    query.where_equals("quotation_id", engine::Value::text(quotation_id));
    query.order_by("revision");
    query.order_by("id");
    std::vector<QuotationRevision> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(revision_from_row(row));
    }
    return result;
}

// One numbered revision of one quotation. Numbers are unique within a
// quotation, so a second match would be a corrupted stack rather than a
// choice, and the first is returned.
template <typename Reader>
std::optional<QuotationRevision> revision_numbered(const Reader& reader,
                                                   const std::string& quotation_id,
                                                   std::int64_t revision) {
    engine::Query query{tables::kRevision};
    query.where_equals("quotation_id", engine::Value::text(quotation_id));
    query.where_equals("revision", engine::Value::integer(revision));
    query.order_by("id");
    for (const engine::Row& row : reader.select(query)) {
        return revision_from_row(row);
    }
    return std::nullopt;
}

// The live revision: the highest numbered one on the stack.
template <typename Reader>
std::optional<QuotationRevision> latest_revision(const Reader& reader,
                                                 const std::string& quotation_id) {
    const std::vector<QuotationRevision> all = revisions_for_quotation(reader, quotation_id);
    if (all.empty()) {
        return std::nullopt;
    }
    return all.back();
}

// True when this series and number already sit on a different revision. Two
// documents sharing one number is the failure numbering is designed to
// prevent, and it must be caught before the second one is printed.
template <typename Reader>
bool number_taken(const Reader& reader,
                  const std::string& series,
                  std::int64_t number,
                  const std::string& except_revision_id) {
    engine::Query query{tables::kRevision};
    query.where_equals("series", engine::Value::text(series));
    query.where_equals("number", engine::Value::integer(number));
    for (const engine::Row& row : reader.select(query)) {
        if (row.get("id").text_or({}) != except_revision_id) {
            return true;
        }
    }
    return false;
}

// The lines of one revision, in printing order. Position first, then id, so
// two devices print the same page even when positions collide.
template <typename Reader>
std::vector<QuotationLine> lines_for_revision(const Reader& reader,
                                              const std::string& revision_id) {
    engine::Query query{tables::kLine};
    query.where_equals("revision_id", engine::Value::text(revision_id));
    query.order_by("position");
    query.order_by("id");
    std::vector<QuotationLine> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(line_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<Quotation> quotations_for_party(const Reader& reader,
                                            const std::string& party_id) {
    engine::Query query{tables::kQuotation};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.order_by("created_at");
    query.order_by("id");
    std::vector<Quotation> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(quotation_from_row(row));
    }
    return result;
}

// Issued revisions whose validity date has already passed at the moment given,
// oldest deadline first. This is what makes an expiring quotation an attention
// item instead of something the shopkeeper has to remember.
//
// Revisions with no validity date are excluded rather than treated as expiring
// far in the future: an offer with no date does not expire by time at all.
template <typename Reader>
std::vector<QuotationRevision> revisions_lapsed_by(const Reader& reader, std::int64_t at) {
    engine::Query query{tables::kRevision};
    query.where_equals("issued", engine::Value::boolean(true));
    query.where("valid_until", engine::Comparison::Greater, engine::Value::integer(0));
    query.where("valid_until", engine::Comparison::Less, engine::Value::integer(at));
    query.order_by("valid_until");
    query.order_by("id");
    std::vector<QuotationRevision> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(revision_from_row(row));
    }
    return result;
}

// Writes. A transaction is required, and the caller cannot open one, so a
// write outside the gate above is not expressible here.
void save_quotation(engine::Transaction& transaction, const Quotation& quotation);
void save_revision(engine::Transaction& transaction, const QuotationRevision& revision);
void save_line(engine::Transaction& transaction, const QuotationLine& line);

// Clears the lines of a revision that has not been issued. Used when a draft
// is edited: the draft is replaced wholesale rather than patched line by line,
// because the protocol declares no line-level operation to patch it with.
//
// Returns how many lines were removed.
std::size_t remove_lines_for_revision(engine::Transaction& transaction,
                                      const std::string& revision_id);

}  // namespace squiflow::modules::quotations::data
