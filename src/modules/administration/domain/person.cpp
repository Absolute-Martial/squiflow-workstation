#include "modules/administration/domain/person.hpp"

#include <cctype>

#include "modules/context.hpp"

namespace squiflow::modules::administration {
namespace {

bool is_space(unsigned char character) { return std::isspace(character) != 0; }

}  // namespace

std::string trim(std::string_view raw) {
    std::size_t start = 0;
    std::size_t stop = raw.size();
    while (start < stop && is_space(static_cast<unsigned char>(raw[start]))) {
        ++start;
    }
    while (stop > start && is_space(static_cast<unsigned char>(raw[stop - 1]))) {
        --stop;
    }
    return std::string(raw.substr(start, stop - start));
}

std::string normalise_username(std::string_view raw) {
    std::string result = trim(raw);
    for (char& character : result) {
        const unsigned char byte = static_cast<unsigned char>(character);
        if (byte < 128) {
            character = static_cast<char>(std::tolower(byte));
        }
    }
    return result;
}

void validate(const Person& person) {
    if (person.id.empty()) {
        throw RuleViolation("This person has no record to be saved under.");
    }
    if (trim(person.display_name).empty()) {
        throw RuleViolation("A person needs a name.");
    }
    if (person.username.empty()) {
        throw RuleViolation("A person needs a username to sign in with.");
    }
    if (person.username.size() > 40) {
        throw RuleViolation("That username is too long to type twice a day.");
    }
    for (const char character : person.username) {
        if (is_space(static_cast<unsigned char>(character))) {
            throw RuleViolation("A username cannot contain spaces.");
        }
    }
    if (person.password_hash.empty()) {
        // Not "choose a better password": this module never sees a password,
        // so an empty hash means the sign-in layer failed to do its job.
        throw RuleViolation("This person has no password set.");
    }
}

engine::Row to_row(const Person& person) {
    engine::Row row;
    row.set("id", engine::Value::text(person.id));
    row.set("display_name", engine::Value::text(person.display_name));
    row.set("username", engine::Value::text(person.username));
    row.set("password_hash", engine::Value::text(person.password_hash));
    row.set("is_owner", engine::Value::boolean(person.is_owner));
    row.set("disabled", engine::Value::boolean(person.disabled));
    row.set("created_at", engine::Value::integer(person.created_at));
    row.set("updated_at", engine::Value::integer(person.updated_at));
    row.set("created_by", engine::Value::text(person.created_by));
    return row;
}

Person person_from_row(const engine::Row& row) {
    Person person;
    person.id = row.get("id").text_or({});
    person.display_name = row.get("display_name").text_or({});
    person.username = row.get("username").text_or({});
    person.password_hash = row.get("password_hash").text_or({});
    person.is_owner = row.get("is_owner").boolean_or(false);
    person.disabled = row.get("disabled").boolean_or(false);
    person.created_at = row.get("created_at").integer_or(0);
    person.updated_at = row.get("updated_at").integer_or(0);
    person.created_by = row.get("created_by").text_or({});
    return person;
}

std::optional<protocol::RightId> right_from_name(std::string_view name) {
    for (std::size_t index = 0; index < protocol::kRightCount; ++index) {
        const protocol::RightId right = static_cast<protocol::RightId>(index);
        if (protocol::right_name(right) == name) {
            return right;
        }
    }
    return std::nullopt;
}

}  // namespace squiflow::modules::administration
