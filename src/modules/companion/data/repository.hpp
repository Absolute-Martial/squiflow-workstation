#pragma once

#include <algorithm>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/companion/data/tables.hpp"
#include "modules/companion/domain/task.hpp"

namespace squiflow::modules::companion::data {

template <typename Reader>
std::optional<Task> find_task(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kTask, id);
    return row ? std::optional<Task>{task_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<TaskEvent> find_event(const Reader& reader, const std::string& id) {
    const auto row = reader.find(tables::kEvent, id);
    return row ? std::optional<TaskEvent>{event_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::optional<Task> find_by_source_key(const Reader& reader, const std::string& source_key) {
    engine::Query query{tables::kTask};
    query.where_equals("source_key", engine::Value::text(source_key));
    query.order_by("id").take(1);
    const auto rows = reader.select(query);
    return rows.empty() ? std::nullopt : std::optional<Task>{task_from_row(rows.front())};
}

template <typename Reader>
std::vector<TaskEvent> events_for_task(const Reader& reader, const std::string& task_id) {
    engine::Query query{tables::kEvent};
    query.where_equals("task_id", engine::Value::text(task_id));
    query.order_by("happened_at").order_by("id");
    std::vector<TaskEvent> result;
    for (const auto& row : reader.select(query)) result.push_back(event_from_row(row));
    return result;
}

template <typename Reader>
std::vector<Task> tasks_for_target(const Reader& reader, const engine::Reference& target) {
    if (!target.is_valid()) return {};
    engine::Query query{tables::kTask};
    query.where_equals("target_module", engine::Value::integer(
        static_cast<std::int64_t>(target.module)));
    query.where_equals("target_record", engine::Value::text(engine::to_string(target.record)));
    query.order_by("due_at").order_by("id");
    std::vector<Task> result;
    for (const auto& row : reader.select(query)) result.push_back(task_from_row(row));
    return result;
}

template <typename Reader>
std::vector<Task> visible_tasks(const Reader& reader, std::int64_t at) {
    engine::Query query{tables::kTask};
    query.where_equals("state", engine::Value::integer(
        static_cast<std::int64_t>(TaskState::Open)));
    std::vector<Task> result;
    for (const auto& row : reader.select(query)) {
        Task task = task_from_row(row);
        if (visible_at(task, at)) result.push_back(std::move(task));
    }
    std::sort(result.begin(), result.end(), [](const Task& left, const Task& right) {
        const std::int64_t left_due = effective_due_at(left);
        const std::int64_t right_due = effective_due_at(right);
        if (left_due != right_due) return left_due < right_due;
        return left.id < right.id;
    });
    return result;
}

template <typename Reader>
std::vector<Task> tasks_due_by(const Reader& reader, std::int64_t at) {
    std::vector<Task> result;
    for (Task task : visible_tasks(reader, at)) {
        if (task.due_at > 0) result.push_back(std::move(task));
    }
    return result;
}

template <typename Reader>
std::vector<Task> recurring_tasks(const Reader& reader) {
    engine::Query query{tables::kTask};
    query.where_equals("kind", engine::Value::integer(
        static_cast<std::int64_t>(TaskKind::Recurring)));
    query.order_by("due_at").order_by("id");
    std::vector<Task> result;
    for (const auto& row : reader.select(query)) result.push_back(task_from_row(row));
    return result;
}

void save_task(engine::Transaction& transaction, const Task& task);
void save_event(engine::Transaction& transaction, const TaskEvent& event);

}  // namespace squiflow::modules::companion::data
