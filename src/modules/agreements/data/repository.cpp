#include "modules/agreements/data/repository.hpp"

namespace squiflow::modules::agreements::data {
namespace {

void upsert(engine::Transaction& transaction,
            const char* table,
            const std::string& key,
            const engine::Row& row) {
    if (!transaction.replace(table, key, row)) {
        transaction.insert(table, row);
    }
}

}  // namespace

void save_agreement(engine::Transaction& transaction, const Agreement& agreement) {
    validate(agreement);
    upsert(transaction, tables::kAgreement, agreement.id, to_row(agreement));
}

void save_line(engine::Transaction& transaction, const AgreementLine& line) {
    validate(line);
    upsert(transaction, tables::kLine, line.id, to_row(line));
}

std::size_t remove_lines_for_agreement(engine::Transaction& transaction,
                                       const std::string& agreement_id) {
    const std::vector<AgreementLine> existing = lines_for_agreement(transaction, agreement_id);
    std::size_t removed = 0;
    for (const AgreementLine& line : existing) {
        if (transaction.remove(tables::kLine, line.id)) {
            ++removed;
        }
    }
    return removed;
}

}  // namespace squiflow::modules::agreements::data
