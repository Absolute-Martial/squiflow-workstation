#include "modules/parties/data/repository.hpp"

namespace squiflow::modules::parties::data {
namespace {

void upsert(engine::Transaction& transaction, const std::string& table,
            const std::string& key, const engine::Row& row) {
    if (!transaction.replace(table, key, row)) {
        transaction.insert(table, row);
    }
}

}  // namespace

void save_party(engine::Transaction& transaction, const Party& party) {
    upsert(transaction, tables::kParty, party.id, to_row(party));
}

void save_contact(engine::Transaction& transaction, const ContactInfo& contact) {
    upsert(transaction, tables::kContact, contact.id, to_row(contact));
}

}  // namespace squiflow::modules::parties::data
