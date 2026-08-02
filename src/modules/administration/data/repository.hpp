#pragma once

// Reading and writing administration's tables.
//
// Reads are templates over anything with find() and select(), which is both a
// Store and a Transaction. Without that, every query would have to be written
// twice - once for reading outside a transaction and once for reading inside
// one - and the two copies would drift.

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/module_id.hpp>
#include <squiflow/protocol/right_id.hpp>

#include "engine/identity/rights_set.hpp"
#include "engine/storage/store.hpp"
#include "modules/administration/data/tables.hpp"
#include "modules/administration/domain/device.hpp"
#include "modules/administration/domain/person.hpp"

namespace squiflow::modules::administration::data {

struct AuditRecord {
    std::int64_t at{0};
    std::string person{};
    std::string device{};
    std::string operation{};
    std::string record{};
    std::string summary{};
};

std::string right_key(const std::string& person_id, protocol::RightId right);

// --- reads -----------------------------------------------------------------

template <typename Reader>
std::optional<Person> find_person(const Reader& reader, const std::string& id) {
    const std::optional<engine::Row> row = reader.find(tables::kPerson, id);
    if (!row) {
        return std::nullopt;
    }
    return person_from_row(*row);
}

template <typename Reader>
std::optional<Person> find_person_by_username(const Reader& reader,
                                              const std::string& username) {
    engine::Query query{tables::kPerson};
    query.where_equals("username", engine::Value::text(username));
    const std::vector<engine::Row> rows = reader.select(query);
    if (rows.empty()) {
        return std::nullopt;
    }
    return person_from_row(rows.front());
}

template <typename Reader>
std::vector<Person> all_people(const Reader& reader) {
    engine::Query query{tables::kPerson};
    query.order_by("created_at");
    std::vector<Person> people;
    for (const engine::Row& row : reader.select(query)) {
        people.push_back(person_from_row(row));
    }
    return people;
}

template <typename Reader>
engine::RightsSet rights_of(const Reader& reader, const std::string& person_id) {
    engine::Query query{tables::kPersonRight};
    query.where_equals("person_id", engine::Value::text(person_id));

    engine::RightsSet rights;
    for (const engine::Row& row : reader.select(query)) {
        const std::optional<protocol::RightId> right =
            right_from_name(row.get("right").text_or({}));
        // A grant naming a right this version does not have is skipped, not
        // fatal. It can only come from a newer version of the application
        // having written it, and refusing to sign anybody in because of a
        // right that will exist after the next update is worse than ignoring
        // it.
        if (right) {
            rights.grant(*right);
        }
    }
    return rights;
}

template <typename Reader>
std::size_t holders_of(const Reader& reader, protocol::RightId right) {
    engine::Query query{tables::kPersonRight};
    query.where_equals("right", engine::Value::text(std::string(protocol::right_name(right))));
    return reader.select(query).size();
}

template <typename Reader>
std::optional<Device> find_device(const Reader& reader, const std::string& id) {
    const std::optional<engine::Row> row = reader.find(tables::kDevice, id);
    if (!row) {
        return std::nullopt;
    }
    return device_from_row(*row);
}

template <typename Reader>
std::optional<std::string> get_setting(const Reader& reader, const std::string& key) {
    const std::optional<engine::Row> row = reader.find(tables::kSetting, key);
    if (!row) {
        return std::nullopt;
    }
    return row->get("value").text_or({});
}

template <typename Reader>
std::vector<protocol::ModuleId> disabled_modules(const Reader& reader) {
    engine::Query query{tables::kModuleState};
    std::vector<protocol::ModuleId> disabled;
    for (const engine::Row& row : reader.select(query)) {
        const std::string name = row.get("module").text_or({});
        for (std::size_t index = 0; index < protocol::kModuleCount; ++index) {
            const protocol::ModuleId module = static_cast<protocol::ModuleId>(index);
            if (protocol::module_name(module) == name) {
                disabled.push_back(module);
                break;
            }
        }
    }
    return disabled;
}

template <typename Reader>
std::vector<engine::Row> audit_since(const Reader& reader, std::int64_t since,
                                     std::size_t limit) {
    engine::Query query{tables::kAudit};
    if (since > 0) {
        query.where("at", engine::Comparison::GreaterOrEqual, engine::Value::integer(since));
    }
    query.order_by("at");
    if (limit > 0) {
        query.take(limit);
    }
    return reader.select(query);
}

// --- writes ----------------------------------------------------------------

void insert_person(engine::Transaction& transaction, const Person& person);
void save_person(engine::Transaction& transaction, const Person& person);

void grant_right(engine::Transaction& transaction, const std::string& person_id,
                 protocol::RightId right, std::int64_t at, const std::string& by);
bool revoke_right(engine::Transaction& transaction, const std::string& person_id,
                  protocol::RightId right);

void save_device(engine::Transaction& transaction, const Device& device);

void put_setting(engine::Transaction& transaction, const std::string& key,
                 const std::string& value, std::int64_t at, const std::string& by);

void set_module_disabled(engine::Transaction& transaction,
                         const std::vector<protocol::ModuleId>& disabled);

void append_audit(engine::Transaction& transaction, const AuditRecord& record);

}  // namespace squiflow::modules::administration::data
