#include "modules/quotations/data/repository.hpp"

namespace squiflow::modules::quotations::data {
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

void save_quotation(engine::Transaction& transaction, const Quotation& quotation) {
    validate(quotation);
    upsert(transaction, tables::kQuotation, quotation.id, to_row(quotation));
}

void save_revision(engine::Transaction& transaction, const QuotationRevision& revision) {
    validate(revision);
    upsert(transaction, tables::kRevision, revision.id, to_row(revision));
}

void save_line(engine::Transaction& transaction, const QuotationLine& line) {
    validate(line);
    upsert(transaction, tables::kLine, line.id, to_row(line));
}

std::size_t remove_lines_for_revision(engine::Transaction& transaction,
                                      const std::string& revision_id) {
    const std::vector<QuotationLine> existing = lines_for_revision(transaction, revision_id);
    std::size_t removed = 0;
    for (const QuotationLine& line : existing) {
        if (transaction.remove(tables::kLine, line.id)) {
            ++removed;
        }
    }
    return removed;
}

}  // namespace squiflow::modules::quotations::data
