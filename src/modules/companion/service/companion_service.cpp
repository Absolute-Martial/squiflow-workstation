#include "modules/companion/service/companion_service.hpp"

#include <limits>
#include <stdexcept>
#include <string>

#include <squiflow/protocol/module_id.hpp>

#include "engine/identity/session.hpp"
#include "engine/records/identity.hpp"
#include "engine/records/payload.hpp"
#include "modules/companion/data/repository.hpp"

namespace squiflow::modules::companion {
namespace {

bool blank(const std::string& text) noexcept {
    for (const char c : text) if (static_cast<unsigned char>(c) > ' ') return false;
    return true;
}

engine::Row fields(const Call& call) {
    try { return engine::decode_payload(call.payload); }
    catch (const engine::PayloadError&) {
        throw RuleViolation("This request could not be read. Please try it again.");
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) throw std::logic_error("companion: write without session");
    return *call.actor;
}

std::string actor_id(const Call& call) { return engine::to_string(actor(call).person); }

std::string subject(const Call& call) {
    if (blank(call.record_id)) throw RuleViolation("This request does not identify its task.");
    return call.record_id;
}

std::string required_text(const engine::Row& row, const std::string& name,
                          const char* complaint) {
    const std::string* value = row.get(name).as_text();
    if (value == nullptr || blank(*value)) throw RuleViolation(complaint);
    return *value;
}

std::string optional_text(const engine::Row& row, const std::string& name,
                          const char* complaint, const std::string& fallback = {}) {
    if (!row.has(name)) return fallback;
    const std::string* value = row.get(name).as_text();
    if (value == nullptr) throw RuleViolation(complaint);
    return *value;
}

std::int64_t required_number(const engine::Row& row, const std::string& name,
                             const char* complaint) {
    const auto value = row.get(name).as_integer();
    if (!value) throw RuleViolation(complaint);
    return *value;
}

std::int64_t optional_number(const engine::Row& row, const std::string& name,
                             const char* complaint, std::int64_t fallback = 0) {
    return row.has(name) ? required_number(row, name, complaint) : fallback;
}

TaskKind kind_from(const std::string& value) {
    if (value == "personal") return TaskKind::Personal;
    if (value == "reminder") return TaskKind::Reminder;
    if (value == "recurring") return TaskKind::Recurring;
    if (value == "attention") return TaskKind::Attention;
    throw RuleViolation("That task kind is not understood.");
}

RecurrenceUnit recurrence_from(const std::string& value) {
    if (value.empty() || value == "none") return RecurrenceUnit::None;
    if (value == "day") return RecurrenceUnit::Day;
    if (value == "week") return RecurrenceUnit::Week;
    if (value == "month") return RecurrenceUnit::Month;
    if (value == "year") return RecurrenceUnit::Year;
    throw RuleViolation("That recurrence unit is not understood.");
}

engine::Reference target_from(const engine::Row& row) {
    const bool has_module = row.has("target_module");
    const bool has_record = row.has("target_record");
    if (!has_module && !has_record) return {};
    if (has_module != has_record) {
        throw RuleViolation("A task target must name both its module and record.");
    }
    const std::int64_t number = required_number(
        row, "target_module", "That target module could not be read as a number.");
    if (number < 0 || number > std::numeric_limits<std::uint32_t>::max()) {
        throw RuleViolation("That task target names a module this build does not know.");
    }
    protocol::ModuleId module{};
    if (!protocol::module_from_number(static_cast<std::uint32_t>(number), module)) {
        throw RuleViolation("That task target names a module this build does not know.");
    }
    const std::string record = required_text(
        row, "target_record", "That task target must name its record.");
    engine::Reference target;
    target.module = module;
    target.record = engine::record_id_from_string(record);
    if (!target.is_valid()) throw RuleViolation("That task target record is malformed.");
    return target;
}

Task existing_task(const engine::Transaction& transaction, const std::string& id) {
    const auto task = data::find_task(transaction, id);
    if (!task) throw RuleViolation("That task is not on file.");
    validate(*task);
    return *task;
}

TaskEvent event_for(const Call& call, TaskEventKind kind, std::int64_t at) {
    TaskEvent event;
    event.id = call.idempotency_key;
    event.task_id = call.record_id;
    event.kind = kind;
    event.happened_at = at;
    event.happened_by = actor_id(call);
    return event;
}

}  // namespace

void CompanionService::create(engine::Transaction& transaction, const Call& call) const {
    const std::string id = subject(call);
    const engine::Row row = fields(call);
    if (data::find_task(transaction, id)) throw RuleViolation("That task is already on file.");

    const std::int64_t at = clock_();
    Task task;
    task.id = id;
    task.kind = kind_from(optional_text(row, "kind", "That task kind could not be read as text.", "personal"));
    task.title = required_text(row, "title", "A task must have a title.");
    task.note = optional_text(row, "note", "That task note could not be read as text.");
    task.target = target_from(row);
    task.source_key = optional_text(row, "source_key", "That source key could not be read as text.");
    task.due_at = optional_number(row, "due_at", "That due date could not be read as a number.");
    task.recurrence_unit = recurrence_from(optional_text(
        row, "recurrence_unit", "That recurrence unit could not be read as text."));
    task.recurrence_interval = optional_number(
        row, "recurrence_interval", "That recurrence interval could not be read as a number.");
    task.created_at = at;
    task.created_by = actor_id(call);
    task.updated_at = at;
    task.updated_by = task.created_by;
    validate(task);

    TaskEvent event = event_for(call, TaskEventKind::Created, at);
    event.next_due_at = task.due_at;
    if (task.kind == TaskKind::Attention) {
        create_attention(transaction, task, event);
        return;
    }
    data::save_task(transaction, task);
    data::save_event(transaction, event);
}

void CompanionService::update(engine::Transaction& transaction, const Call& call) const {
    const std::string id = subject(call);
    const engine::Row row = fields(call);
    Task task = existing_task(transaction, id);
    if (task.state != TaskState::Open) throw RuleViolation("A completed task cannot be amended.");
    if (row.has("kind") || row.has("source_key") || row.has("target_module") ||
        row.has("target_record")) {
        throw RuleViolation("A task cannot change its kind, source, or attached record.");
    }

    const std::int64_t previous_due = task.due_at;
    task.title = optional_text(row, "title", "That task title could not be read as text.", task.title);
    task.note = optional_text(row, "note", "That task note could not be read as text.", task.note);
    task.due_at = optional_number(row, "due_at", "That due date could not be read as a number.", task.due_at);
    if (row.has("recurrence_unit")) {
        task.recurrence_unit = recurrence_from(optional_text(
            row, "recurrence_unit", "That recurrence unit could not be read as text."));
    }
    task.recurrence_interval = optional_number(
        row, "recurrence_interval", "That recurrence interval could not be read as a number.",
        task.recurrence_interval);
    task.updated_at = clock_();
    task.updated_by = actor_id(call);
    validate(task);

    TaskEvent event = event_for(call, TaskEventKind::Updated, task.updated_at);
    event.previous_due_at = previous_due;
    event.next_due_at = task.due_at;
    data::save_task(transaction, task);
    data::save_event(transaction, event);
}

void CompanionService::complete(engine::Transaction& transaction, const Call& call) const {
    const std::string id = subject(call);
    fields(call);
    Task task = existing_task(transaction, id);
    if (task.state != TaskState::Open) throw RuleViolation("That task is already completed.");

    const std::int64_t at = clock_();
    const std::int64_t previous_due = task.due_at;
    TaskEvent event = event_for(call, TaskEventKind::Completed, at);
    event.previous_due_at = previous_due;
    task.updated_at = at;
    task.updated_by = actor_id(call);
    task.snoozed_until = 0;
    task.snooze_reason.clear();

    if (is_recurring(task)) {
        const auto next = next_due_after(task.due_at, task.recurrence_unit,
                                         task.recurrence_interval);
        if (!next || task.completion_count == std::numeric_limits<std::int64_t>::max()) {
            throw RuleViolation("That recurring task cannot calculate another safe due date.");
        }
        task.due_at = *next;
        ++task.completion_count;
        task.last_completed_at = at;
        task.last_completed_by = actor_id(call);
        event.next_due_at = task.due_at;
    } else {
        task.state = TaskState::Completed;
        task.completed_at = at;
        task.completed_by = actor_id(call);
        ++task.completion_count;
    }
    data::save_task(transaction, task);
    data::save_event(transaction, event);
}

void CompanionService::snooze(engine::Transaction& transaction, const Call& call) const {
    const std::string id = subject(call);
    const engine::Row row = fields(call);
    Task task = existing_task(transaction, id);
    if (task.state != TaskState::Open) throw RuleViolation("A completed task cannot be snoozed.");
    const std::int64_t at = clock_();
    const std::int64_t until = required_number(
        row, "snoozed_until", "That snooze return time could not be read as a number.");
    const std::string reason = required_text(row, "reason", "Snoozing a task needs a reason.");
    if (until <= at) throw RuleViolation("A snoozed task must return in the future.");

    TaskEvent event = event_for(call, TaskEventKind::Snoozed, at);
    event.reason = reason;
    event.previous_due_at = task.snoozed_until;
    event.next_due_at = until;
    task.snoozed_until = until;
    task.snooze_reason = reason;
    task.updated_at = at;
    task.updated_by = actor_id(call);
    data::save_task(transaction, task);
    data::save_event(transaction, event);
}

void CompanionService::create_attention(engine::Transaction& transaction,
                                         const Task& task,
                                         const TaskEvent& event) const {
    validate(task);
    validate(event);
    if (task.kind != TaskKind::Attention || event.kind != TaskEventKind::Created ||
        event.task_id != task.id) {
        throw RuleViolation("That deterministic attention item is malformed.");
    }
    if (data::find_task(transaction, task.id)) throw RuleViolation("That task is already on file.");
    if (data::find_by_source_key(transaction, task.source_key)) {
        throw RuleViolation("That attention item is already on file.");
    }
    data::save_task(transaction, task);
    data::save_event(transaction, event);
}

}  // namespace squiflow::modules::companion
