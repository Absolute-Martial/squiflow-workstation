#include "modules/companion/data/repository.hpp"

namespace squiflow::modules::companion::data {
namespace {
void upsert(engine::Transaction& transaction, const char* table,
            const std::string& key, const engine::Row& row) {
    if (!transaction.replace(table, key, row)) transaction.insert(table, row);
}
}  // namespace

void save_task(engine::Transaction& transaction, const Task& task) {
    validate(task);
    upsert(transaction, tables::kTask, task.id, to_row(task));
}

void save_event(engine::Transaction& transaction, const TaskEvent& event) {
    validate(event);
    upsert(transaction, tables::kEvent, event.id, to_row(event));
}

}  // namespace squiflow::modules::companion::data
