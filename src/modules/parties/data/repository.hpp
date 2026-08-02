#pragma once

#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/parties/data/tables.hpp"
#include "modules/parties/domain/party.hpp"

namespace squiflow::modules::parties::data {

// --- reads (template over Store or Transaction) ----------------------------

template <typename Reader>
std::optional<Party> find_party(const Reader& reader, const std::string& id) {
    const std::optional<engine::Row> row = reader.find(tables::kParty, id);
    if (!row) return std::nullopt;
    return party_from_row(*row);
}

template <typename Reader>
std::vector<Party> all_parties(const Reader& reader) {
    engine::Query query{tables::kParty};
    query.order_by("display_name");
    std::vector<Party> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(party_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<ContactInfo> contacts_of(const Reader& reader, const std::string& party_id) {
    engine::Query query{tables::kContact};
    query.where_equals("party_id", engine::Value::text(party_id));
    query.order_by("added_at");
    std::vector<ContactInfo> result;
    for (const engine::Row& row : reader.select(query)) {
        result.push_back(contact_from_row(row));
    }
    return result;
}

// --- writes ----------------------------------------------------------------

void save_party(engine::Transaction& transaction, const Party& party);
void save_contact(engine::Transaction& transaction, const ContactInfo& contact);

}  // namespace squiflow::modules::parties::data
