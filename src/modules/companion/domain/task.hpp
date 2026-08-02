#pragma once

// Companion owns tasks, not the records they point at. A generic Reference is
// the entire attachment boundary, which keeps this module independent of every
// customer, job, invoice, agreement, and purchase implementation.

#include <cstdint>
#include <optional>
#include <string>

#include "engine/records/reference.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::companion {

enum class TaskKind : std::uint8_t { Personal, Reminder, Recurring, Attention };
enum class TaskState : std::uint8_t { Open, Completed };
enum class RecurrenceUnit : std::uint8_t { None, Day, Week, Month, Year };
enum class TaskEventKind : std::uint8_t { Created, Updated, Completed, Snoozed };

const char* to_string(TaskKind kind) noexcept;
const char* to_string(TaskState state) noexcept;
const char* to_string(RecurrenceUnit unit) noexcept;
const char* to_string(TaskEventKind kind) noexcept;

struct Task {
    std::string id{};
    TaskKind kind{TaskKind::Personal};
    TaskState state{TaskState::Open};
    std::string title{};
    std::string note{};
    engine::Reference target{};

    // Stable identity for rule-generated attention. Empty for human tasks.
    std::string source_key{};

    // Milliseconds UTC. Zero means no due date, which only personal tasks may
    // use. Snoozing never rewrites due_at.
    std::int64_t due_at{0};
    std::int64_t snoozed_until{0};
    std::string snooze_reason{};

    RecurrenceUnit recurrence_unit{RecurrenceUnit::None};
    std::int64_t recurrence_interval{0};
    std::int64_t completion_count{0};

    std::int64_t created_at{0};
    std::string created_by{};
    std::int64_t updated_at{0};
    std::string updated_by{};
    std::int64_t completed_at{0};
    std::string completed_by{};
    std::int64_t last_completed_at{0};
    std::string last_completed_by{};
};

struct TaskEvent {
    std::string id{};
    std::string task_id{};
    TaskEventKind kind{TaskEventKind::Created};
    std::int64_t happened_at{0};
    std::string happened_by{};
    std::string reason{};
    std::int64_t previous_due_at{0};
    std::int64_t next_due_at{0};
};

constexpr bool has_target(const Task& task) noexcept { return task.target.is_valid(); }
constexpr bool is_recurring(const Task& task) noexcept {
    return task.kind == TaskKind::Recurring;
}

// The moment an open task becomes visible. A snooze can only push it later.
std::int64_t effective_due_at(const Task& task) noexcept;
bool visible_at(const Task& task, std::int64_t at) noexcept;

// Calendar-aware advancement. Null means the input or result cannot be safely
// represented. Month and year recurrence clamp to the final valid day.
std::optional<std::int64_t> next_due_after(std::int64_t due_at,
                                           RecurrenceUnit unit,
                                           std::int64_t interval) noexcept;

void validate(const Task& task);
void validate(const TaskEvent& event);

engine::Row to_row(const Task& task);
engine::Row to_row(const TaskEvent& event);
Task task_from_row(const engine::Row& row);
TaskEvent event_from_row(const engine::Row& row);

}  // namespace squiflow::modules::companion
