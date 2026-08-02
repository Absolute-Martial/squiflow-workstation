#include "modules/jobs/data/repository.hpp"

namespace squiflow::modules::jobs::data {
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

void save_job(engine::Transaction& transaction, const Job& job) {
    validate(job);
    upsert(transaction, tables::kJob, job.id, to_row(job));
}

}  // namespace squiflow::modules::jobs::data
