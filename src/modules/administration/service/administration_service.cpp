#include "modules/administration/service/administration_service.hpp"

#include <algorithm>
#include <optional>
#include <stdexcept>

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/operation_table.hpp>

#include "engine/records/payload.hpp"
#include "modules/administration/data/repository.hpp"
#include "modules/administration/domain/device.hpp"
#include "modules/administration/domain/person.hpp"

namespace squiflow::modules::administration {
namespace {

std::string required_text(const engine::Row& fields, const std::string& name,
                          const std::string& complaint) {
    const std::string value = trim(fields.get(name).text_or({}));
    if (value.empty()) {
        throw RuleViolation(complaint);
    }
    return value;
}

protocol::RightId required_right(const engine::Row& fields) {
    const std::string name = trim(fields.get("right").text_or({}));
    const std::optional<protocol::RightId> right = right_from_name(name);
    if (!right) {
        // Not a person's mistake: the screen sent a right this version does
        // not have. Saying which one is what makes that findable.
        throw RuleViolation("There is no permission called '" + name + "'.");
    }
    return *right;
}

std::string subject(const Call& call) {
    if (call.record_id.empty()) {
        throw RuleViolation("This request does not say which record it is about.");
    }
    return call.record_id;
}

std::vector<std::string> split(const std::string& text, char separator) {
    std::vector<std::string> parts;
    std::string current;
    for (const char character : text) {
        if (character == separator) {
            if (!trim(current).empty()) {
                parts.push_back(trim(current));
            }
            current.clear();
        } else {
            current.push_back(character);
        }
    }
    if (!trim(current).empty()) {
        parts.push_back(trim(current));
    }
    return parts;
}

}  // namespace

engine::Row read_fields(const Call& call) {
    if (call.payload.empty()) {
        return {};
    }
    try {
        return engine::decode_payload(call.payload);
    } catch (const engine::PayloadError& broken) {
        throw RuleViolation(std::string("This request could not be read: ") + broken.what());
    }
}

const engine::Session& actor(const Call& call) {
    if (call.actor == nullptr) {
        // The registry always fills this in. Reaching here means a handler was
        // called by something other than the registry, which is the exact
        // bypass the single-door rule exists to prevent.
        throw std::logic_error("a handler was run without a session");
    }
    return *call.actor;
}

AdministrationService::AdministrationService(Clock clock) : clock_(std::move(clock)) {
    if (!clock_) {
        throw std::logic_error("administration needs a clock");
    }
}

void AdministrationService::record(engine::Transaction& transaction, const Call& call,
                                   const std::string& summary) {
    const engine::Session& who = actor(call);
    data::AuditRecord entry;
    entry.at = clock_();
    entry.person = engine::to_string(who.person);
    entry.device = engine::to_string(who.device);
    entry.operation = std::string(protocol::operation(call.operation).name);
    entry.record = call.record_id;
    entry.summary = summary;
    data::append_audit(transaction, entry);
}

void AdministrationService::create_person(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);

    Person person;
    person.id = subject(call);
    person.display_name = required_text(fields, "display_name", "A person needs a name.");
    person.username = normalise_username(fields.get("username").text_or({}));
    person.password_hash = fields.get("password_hash").text_or({});
    person.created_at = clock_();
    person.updated_at = person.created_at;
    person.created_by = engine::to_string(actor(call).person);
    validate(person);

    if (data::find_person(transaction, person.id)) {
        throw RuleViolation("That person already exists.");
    }
    if (data::find_person_by_username(transaction, person.username)) {
        throw RuleViolation("Somebody already signs in with the username '" + person.username +
                            "'.");
    }

    // The first person created is the owner, and there is never a second one.
    // The owner is not a role: it decides whose version wins a sync conflict
    // and who can still work with the connection down, and two answers to
    // either of those is not a permission model, it is a coin toss.
    const std::size_t existing = data::all_people(transaction).size();
    const bool asked_for_owner = fields.get("is_owner").boolean_or(false);
    if (existing == 0) {
        person.is_owner = true;
    } else if (asked_for_owner) {
        throw RuleViolation("The shop already has an owner.");
    }

    data::insert_person(transaction, person);

    // The owner holds everything from the start. Nobody else does: a new
    // person can do nothing at all until the shopkeeper grants them something,
    // which is the safer direction to be wrong in.
    if (person.is_owner) {
        for (std::size_t index = 0; index < protocol::kRightCount; ++index) {
            data::grant_right(transaction, person.id, static_cast<protocol::RightId>(index),
                              person.created_at, person.created_by);
        }
    }

    record(transaction, call, "added " + person.display_name);
}

void AdministrationService::update_person(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);

    std::optional<Person> existing = data::find_person(transaction, id);
    if (!existing) {
        throw RuleViolation("That person is not in the list.");
    }

    Person updated = *existing;
    if (fields.has("display_name")) {
        updated.display_name = required_text(fields, "display_name", "A person needs a name.");
    }
    if (fields.has("username")) {
        updated.username = normalise_username(fields.get("username").text_or({}));
        const std::optional<Person> clash =
            data::find_person_by_username(transaction, updated.username);
        if (clash && clash->id != updated.id) {
            throw RuleViolation("Somebody already signs in with the username '" +
                                updated.username + "'.");
        }
    }
    if (fields.has("password_hash")) {
        updated.password_hash = fields.get("password_hash").text_or({});
    }
    if (fields.has("is_owner")) {
        // Ownership is not editable. Transferring the shop is a deliberate,
        // rare act that deserves its own operation rather than a checkbox that
        // can be clicked by accident.
        throw RuleViolation("Who owns the shop cannot be changed here.");
    }

    updated.updated_at = clock_();
    validate(updated);
    data::save_person(transaction, updated);

    record(transaction, call, "changed " + updated.display_name);
}

void AdministrationService::disable_person(engine::Transaction& transaction, const Call& call) {
    const std::string id = subject(call);
    std::optional<Person> person = data::find_person(transaction, id);
    if (!person) {
        throw RuleViolation("That person is not in the list.");
    }
    if (person->is_owner) {
        throw RuleViolation("The owner cannot be switched off.");
    }
    if (engine::to_string(actor(call).person) == id) {
        // Allowed by every right they hold, and still wrong: the shopkeeper
        // would be signed out mid-click with no way back in.
        throw RuleViolation("You cannot switch off your own sign-in.");
    }
    if (person->disabled) {
        return;  // Already off. Asking twice is not an error.
    }

    person->disabled = true;
    person->updated_at = clock_();
    data::save_person(transaction, *person);

    record(transaction, call, "switched off " + person->display_name);
}

void AdministrationService::grant_right(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);
    const protocol::RightId right = required_right(fields);

    const std::optional<Person> person = data::find_person(transaction, id);
    if (!person) {
        throw RuleViolation("That person is not in the list.");
    }
    if (person->disabled) {
        throw RuleViolation("Switch this person back on before giving them permissions.");
    }

    // Nobody can hand out what they do not hold. Without this, one right - the
    // right to grant rights - quietly becomes all of them.
    const engine::Session& who = actor(call);
    if (!who.rights.has(right)) {
        throw RuleViolation("You cannot give away a permission you do not have yourself.");
    }

    data::grant_right(transaction, id, right, clock_(), engine::to_string(who.person));
    record(transaction, call,
           std::string("gave ") + person->display_name + " " +
               std::string(protocol::right_name(right)));
}

void AdministrationService::revoke_right(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string id = subject(call);
    const protocol::RightId right = required_right(fields);

    const std::optional<Person> person = data::find_person(transaction, id);
    if (!person) {
        throw RuleViolation("That person is not in the list.");
    }
    if (person->is_owner) {
        throw RuleViolation("The owner keeps every permission.");
    }

    // Taking away the last grant of the right to grant rights locks the shop
    // out of its own permissions. The owner holds everything, so this can only
    // bite in a shop whose owner record was lost - which is precisely when it
    // matters.
    if (right == protocol::RightId::right_rights_grant &&
        data::holders_of(transaction, right) <= 1) {
        throw RuleViolation("Somebody has to be able to give out permissions.");
    }

    data::revoke_right(transaction, id, right);
    record(transaction, call,
           std::string("took ") + std::string(protocol::right_name(right)) + " from " +
               person->display_name);
}

void AdministrationService::register_device(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);

    Device device;
    device.id = subject(call);
    device.name = trim(fields.get("name").text_or({}));
    device.registered_at = clock_();
    device.registered_by = engine::to_string(actor(call).person);
    validate(device);

    if (data::find_device(transaction, device.id)) {
        throw RuleViolation("That machine is already registered.");
    }

    data::save_device(transaction, device);
    record(transaction, call, "registered " + device.name);
}

void AdministrationService::retire_device(engine::Transaction& transaction, const Call& call) {
    const std::string id = subject(call);
    std::optional<Device> device = data::find_device(transaction, id);
    if (!device) {
        throw RuleViolation("That machine is not registered.");
    }
    if (engine::to_string(actor(call).device) == id) {
        throw RuleViolation("You cannot retire the machine you are working on.");
    }
    if (device->retired) {
        return;
    }

    device->retired = true;
    device->retired_at = clock_();
    data::save_device(transaction, *device);
    record(transaction, call, "retired " + device->name);
}

void AdministrationService::update_setting(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);
    const std::string key = subject(call);
    if (!fields.has("value")) {
        throw RuleViolation("This setting was sent with nothing to set it to.");
    }

    const std::string value = fields.get("value").text_or({});
    data::put_setting(transaction, key, value, clock_(), engine::to_string(actor(call).person));
    record(transaction, call, "set " + key);
}

void AdministrationService::set_activation(engine::Transaction& transaction, const Call& call) {
    const engine::Row fields = read_fields(call);

    std::vector<protocol::ModuleId> disabled;
    for (const std::string& name : split(fields.get("disabled").text_or({}), ',')) {
        bool known = false;
        for (std::size_t index = 0; index < protocol::kModuleCount; ++index) {
            const protocol::ModuleId module = static_cast<protocol::ModuleId>(index);
            if (protocol::module_name(module) == name) {
                disabled.push_back(module);
                known = true;
                break;
            }
        }
        if (!known) {
            throw RuleViolation("There is no part of the application called '" + name + "'.");
        }
    }

    // The protocol works out the consequences: switching off something another
    // switched-on part depends on switches that one off too, and a core part
    // cannot be switched off at all. This module does not re-implement any of
    // that, because two implementations of a dependency rule is one too many.
    const protocol::ActivationResult resolved = protocol::resolve_activation(disabled);
    if (!resolved.ok) {
        throw RuleViolation(resolved.error);
    }

    data::set_module_disabled(transaction, disabled);

    std::string summary = "switched off:";
    if (disabled.empty()) {
        summary = "switched everything on";
    } else {
        for (const protocol::ModuleId module : disabled) {
            summary += " ";
            summary += protocol::module_name(module);
        }
    }
    record(transaction, call, summary);
}

std::vector<engine::Row> AdministrationService::export_audit(const engine::Store& store,
                                                             const Call& call) const {
    const engine::Row fields = read_fields(call);
    const std::int64_t since = fields.get("since").integer_or(0);
    const std::int64_t limit = fields.get("limit").integer_or(0);
    if (limit < 0) {
        throw RuleViolation("A negative number of entries is not a request.");
    }
    return data::audit_since(store, since, static_cast<std::size_t>(limit));
}

}  // namespace squiflow::modules::administration
