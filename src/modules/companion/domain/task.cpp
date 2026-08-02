#include "modules/companion/domain/task.hpp"

#include <chrono>
#include <limits>

#include <squiflow/protocol/module_id.hpp>

#include "modules/context.hpp"

namespace squiflow::modules::companion {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') return false;
    }
    return true;
}

TaskKind task_kind_from(std::int64_t value) noexcept {
    switch (value) {
        case 0: return TaskKind::Personal;
        case 1: return TaskKind::Reminder;
        case 2: return TaskKind::Recurring;
        case 3: return TaskKind::Attention;
        default: return TaskKind::Attention;
    }
}

TaskState task_state_from(std::int64_t value) noexcept {
    return value == 0 ? TaskState::Open : TaskState::Completed;
}

RecurrenceUnit recurrence_from(std::int64_t value) noexcept {
    switch (value) {
        case 0: return RecurrenceUnit::None;
        case 1: return RecurrenceUnit::Day;
        case 2: return RecurrenceUnit::Week;
        case 3: return RecurrenceUnit::Month;
        case 4: return RecurrenceUnit::Year;
        default: return RecurrenceUnit::None;
    }
}

TaskEventKind event_kind_from(std::int64_t value) noexcept {
    switch (value) {
        case 0: return TaskEventKind::Created;
        case 1: return TaskEventKind::Updated;
        case 2: return TaskEventKind::Completed;
        case 3: return TaskEventKind::Snoozed;
        default: return TaskEventKind::Updated;
    }
}

std::optional<std::int64_t> add_milliseconds(std::int64_t value,
                                             std::int64_t amount) noexcept {
    if (amount <= 0 || value > std::numeric_limits<std::int64_t>::max() - amount) {
        return std::nullopt;
    }
    return value + amount;
}

engine::Reference reference_from_row(const engine::Row& row) noexcept {
    engine::Reference reference;
    const std::int64_t stored_module = row.get("target_module").integer_or(-1);
    const std::string stored_record = row.get("target_record").text_or({});
    if (stored_module < 0 || stored_module > std::numeric_limits<std::uint32_t>::max()) {
        reference.module = protocol::ModuleId::Count;
        return reference;
    }
    protocol::ModuleId module{};
    if (!protocol::module_from_number(static_cast<std::uint32_t>(stored_module), module)) {
        reference.module = protocol::ModuleId::Count;
        return reference;
    }
    reference.module = module;
    reference.record = engine::record_id_from_string(stored_record);
    return reference;
}

}  // namespace

const char* to_string(TaskKind kind) noexcept {
    switch (kind) {
        case TaskKind::Personal: return "personal";
        case TaskKind::Reminder: return "reminder";
        case TaskKind::Recurring: return "recurring";
        case TaskKind::Attention: return "attention";
    }
    return "?";
}

const char* to_string(TaskState state) noexcept {
    switch (state) {
        case TaskState::Open: return "open";
        case TaskState::Completed: return "completed";
    }
    return "?";
}

const char* to_string(RecurrenceUnit unit) noexcept {
    switch (unit) {
        case RecurrenceUnit::None: return "none";
        case RecurrenceUnit::Day: return "day";
        case RecurrenceUnit::Week: return "week";
        case RecurrenceUnit::Month: return "month";
        case RecurrenceUnit::Year: return "year";
    }
    return "?";
}

const char* to_string(TaskEventKind kind) noexcept {
    switch (kind) {
        case TaskEventKind::Created: return "created";
        case TaskEventKind::Updated: return "updated";
        case TaskEventKind::Completed: return "completed";
        case TaskEventKind::Snoozed: return "snoozed";
    }
    return "?";
}

std::int64_t effective_due_at(const Task& task) noexcept {
    return task.snoozed_until > task.due_at ? task.snoozed_until : task.due_at;
}

bool visible_at(const Task& task, std::int64_t at) noexcept {
    if (task.state != TaskState::Open) return false;
    const std::int64_t due = effective_due_at(task);
    return due == 0 || due <= at;
}

std::optional<std::int64_t> next_due_after(std::int64_t due_at,
                                           RecurrenceUnit unit,
                                           std::int64_t interval) noexcept {
    if (due_at <= 0 || interval <= 0 || unit == RecurrenceUnit::None) {
        return std::nullopt;
    }
    constexpr std::int64_t kDayMs = 24LL * 60 * 60 * 1000;
    if (unit == RecurrenceUnit::Day || unit == RecurrenceUnit::Week) {
        const std::int64_t days = unit == RecurrenceUnit::Week ? 7 : 1;
        if (interval > std::numeric_limits<std::int64_t>::max() / days) return std::nullopt;
        const std::int64_t day_count = interval * days;
        if (day_count > std::numeric_limits<std::int64_t>::max() / kDayMs) {
            return std::nullopt;
        }
        return add_milliseconds(due_at, day_count * kDayMs);
    }

    if (interval > std::numeric_limits<int>::max()) return std::nullopt;
    using namespace std::chrono;
    const sys_time<milliseconds> instant{milliseconds{due_at}};
    const sys_days day = floor<days>(instant);
    const milliseconds time_of_day = duration_cast<milliseconds>(instant - day);
    const year_month_day current{day};
    if (!current.ok()) return std::nullopt;

    year_month target_month;
    if (unit == RecurrenceUnit::Month) {
        target_month = current.year() / current.month() + months{static_cast<int>(interval)};
    } else if (unit == RecurrenceUnit::Year) {
        target_month = (current.year() + years{static_cast<int>(interval)}) / current.month();
    } else {
        return std::nullopt;
    }
    if (!target_month.ok()) return std::nullopt;

    year_month_day target{target_month / current.day()};
    if (!target.ok()) target = year_month_day{target_month / last};
    if (!target.ok()) return std::nullopt;
    const sys_time<milliseconds> result = sys_days{target} + time_of_day;
    const std::int64_t count = result.time_since_epoch().count();
    if (count <= due_at) return std::nullopt;
    return count;
}

void validate(const Task& task) {
    if (task.id.empty()) throw RuleViolation("This task has no record to be saved under.");
    if (blank(task.title)) throw RuleViolation("A task must have a title.");
    if (task.due_at < 0 || task.snoozed_until < 0) {
        throw RuleViolation("A task date cannot be negative.");
    }
    if (task.created_at <= 0 || blank(task.created_by)) {
        throw RuleViolation("A task must record when and by whom it was created.");
    }
    if (task.updated_at < task.created_at || blank(task.updated_by)) {
        throw RuleViolation("A task update must record when and by whom it happened.");
    }
    if (task.completion_count < 0) {
        throw RuleViolation("A task completion count cannot be negative.");
    }
    if (has_target(task) && !protocol::is_valid(task.target.module)) {
        throw RuleViolation("That task points to a module this build does not know.");
    }

    if (task.kind == TaskKind::Reminder && (!has_target(task) || task.due_at <= 0)) {
        throw RuleViolation("A reminder must name its record and when to return.");
    }
    if (task.kind == TaskKind::Attention &&
        (!has_target(task) || blank(task.source_key))) {
        throw RuleViolation("An attention item must identify its source and record.");
    }
    if (task.kind != TaskKind::Attention && !task.source_key.empty()) {
        throw RuleViolation("Only an attention item may carry a deterministic source key.");
    }

    if (task.kind == TaskKind::Recurring) {
        if (task.due_at <= 0 || task.recurrence_unit == RecurrenceUnit::None ||
            task.recurrence_interval <= 0 ||
            !next_due_after(task.due_at, task.recurrence_unit, task.recurrence_interval)) {
            throw RuleViolation("A recurring task needs a valid future schedule.");
        }
    } else if (task.recurrence_unit != RecurrenceUnit::None ||
               task.recurrence_interval != 0) {
        throw RuleViolation("Only a recurring task may carry a recurrence schedule.");
    }

    if (task.snoozed_until > 0) {
        if (task.state != TaskState::Open || blank(task.snooze_reason)) {
            throw RuleViolation("A snoozed task must stay open and record a reason.");
        }
    } else if (!task.snooze_reason.empty()) {
        throw RuleViolation("A task cannot carry a snooze reason without a return time.");
    }

    if (task.state == TaskState::Completed) {
        if (task.kind == TaskKind::Recurring) {
            throw RuleViolation("A recurring task advances instead of becoming final.");
        }
        if (task.completed_at <= 0 || blank(task.completed_by)) {
            throw RuleViolation("A completed task must record when and by whom it was completed.");
        }
        if (task.snoozed_until != 0) {
            throw RuleViolation("A completed task cannot remain snoozed.");
        }
    } else if (task.completed_at != 0 || !task.completed_by.empty()) {
        throw RuleViolation("An open task cannot carry final completion evidence.");
    }

    const bool has_last = task.last_completed_at > 0 || !task.last_completed_by.empty();
    if (has_last && (task.last_completed_at <= 0 || blank(task.last_completed_by) ||
                     task.completion_count <= 0)) {
        throw RuleViolation("Recurring completion evidence must stay together.");
    }
}

void validate(const TaskEvent& event) {
    if (event.id.empty() || event.task_id.empty()) {
        throw RuleViolation("A task event must identify itself and its task.");
    }
    if (event.happened_at <= 0 || blank(event.happened_by)) {
        throw RuleViolation("A task event must record when and by whom it happened.");
    }
    if (event.previous_due_at < 0 || event.next_due_at < 0) {
        throw RuleViolation("A task event date cannot be negative.");
    }
    if (event.kind == TaskEventKind::Snoozed && blank(event.reason)) {
        throw RuleViolation("A snooze event must record its reason.");
    }
}

engine::Row to_row(const Task& task) {
    engine::Row row;
    row.set("id", engine::Value::text(task.id));
    row.set("kind", engine::Value::integer(static_cast<std::int64_t>(task.kind)));
    row.set("state", engine::Value::integer(static_cast<std::int64_t>(task.state)));
    row.set("title", engine::Value::text(task.title));
    row.set("note", engine::Value::text(task.note));
    row.set("target_module", engine::Value::integer(static_cast<std::int64_t>(task.target.module)));
    row.set("target_record", engine::Value::text(has_target(task) ? engine::to_string(task.target.record) : std::string{}));
    row.set("source_key", engine::Value::text(task.source_key));
    row.set("due_at", engine::Value::integer(task.due_at));
    row.set("snoozed_until", engine::Value::integer(task.snoozed_until));
    row.set("snooze_reason", engine::Value::text(task.snooze_reason));
    row.set("recurrence_unit", engine::Value::integer(static_cast<std::int64_t>(task.recurrence_unit)));
    row.set("recurrence_interval", engine::Value::integer(task.recurrence_interval));
    row.set("completion_count", engine::Value::integer(task.completion_count));
    row.set("created_at", engine::Value::integer(task.created_at));
    row.set("created_by", engine::Value::text(task.created_by));
    row.set("updated_at", engine::Value::integer(task.updated_at));
    row.set("updated_by", engine::Value::text(task.updated_by));
    row.set("completed_at", engine::Value::integer(task.completed_at));
    row.set("completed_by", engine::Value::text(task.completed_by));
    row.set("last_completed_at", engine::Value::integer(task.last_completed_at));
    row.set("last_completed_by", engine::Value::text(task.last_completed_by));
    return row;
}

engine::Row to_row(const TaskEvent& event) {
    engine::Row row;
    row.set("id", engine::Value::text(event.id));
    row.set("task_id", engine::Value::text(event.task_id));
    row.set("kind", engine::Value::integer(static_cast<std::int64_t>(event.kind)));
    row.set("happened_at", engine::Value::integer(event.happened_at));
    row.set("happened_by", engine::Value::text(event.happened_by));
    row.set("reason", engine::Value::text(event.reason));
    row.set("previous_due_at", engine::Value::integer(event.previous_due_at));
    row.set("next_due_at", engine::Value::integer(event.next_due_at));
    return row;
}

Task task_from_row(const engine::Row& row) {
    Task task;
    task.id = row.get("id").text_or({});
    task.kind = task_kind_from(row.get("kind").integer_or(3));
    task.state = task_state_from(row.get("state").integer_or(1));
    task.title = row.get("title").text_or({});
    task.note = row.get("note").text_or({});
    task.target = reference_from_row(row);
    task.source_key = row.get("source_key").text_or({});
    task.due_at = row.get("due_at").integer_or(0);
    task.snoozed_until = row.get("snoozed_until").integer_or(0);
    task.snooze_reason = row.get("snooze_reason").text_or({});
    task.recurrence_unit = recurrence_from(row.get("recurrence_unit").integer_or(0));
    task.recurrence_interval = row.get("recurrence_interval").integer_or(0);
    task.completion_count = row.get("completion_count").integer_or(0);
    task.created_at = row.get("created_at").integer_or(0);
    task.created_by = row.get("created_by").text_or({});
    task.updated_at = row.get("updated_at").integer_or(0);
    task.updated_by = row.get("updated_by").text_or({});
    task.completed_at = row.get("completed_at").integer_or(0);
    task.completed_by = row.get("completed_by").text_or({});
    task.last_completed_at = row.get("last_completed_at").integer_or(0);
    task.last_completed_by = row.get("last_completed_by").text_or({});
    return task;
}

TaskEvent event_from_row(const engine::Row& row) {
    TaskEvent event;
    event.id = row.get("id").text_or({});
    event.task_id = row.get("task_id").text_or({});
    event.kind = event_kind_from(row.get("kind").integer_or(1));
    event.happened_at = row.get("happened_at").integer_or(0);
    event.happened_by = row.get("happened_by").text_or({});
    event.reason = row.get("reason").text_or({});
    event.previous_due_at = row.get("previous_due_at").integer_or(0);
    event.next_due_at = row.get("next_due_at").integer_or(0);
    return event;
}

}  // namespace squiflow::modules::companion
