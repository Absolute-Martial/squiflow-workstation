#include "modules/administration/data/repository.hpp"

#include <string>

namespace squiflow::modules::administration::data {

namespace {

// The store's replace() overwrites a row that is already there and answers
// false when it is not. Every write below wants "this row should now say
// this", whether or not it existed a moment ago, so the two are put together
// here once rather than at each call site - where forgetting the insert would
// silently write nothing at all, which is exactly what happened the first time
// this was written.
void upsert(engine::Transaction& transaction, const std::string& table, const std::string& key,
            const engine::Row& row) {
    if (!transaction.replace(table, key, row)) {
        transaction.insert(table, row);
    }
}

}  // namespace

std::string right_key(const std::string& person_id, protocol::RightId right) {
    return person_id + "/" + std::string(protocol::right_name(right));
}

void insert_person(engine::Transaction& transaction, const Person& person) {
    transaction.insert(tables::kPerson, to_row(person));
}

void save_person(engine::Transaction& transaction, const Person& person) {
    upsert(transaction, tables::kPerson, person.id, to_row(person));
}

void grant_right(engine::Transaction& transaction, const std::string& person_id,
                 protocol::RightId right, std::int64_t at, const std::string& by) {
    engine::Row row;
    row.set("id", engine::Value::text(right_key(person_id, right)));
    row.set("person_id", engine::Value::text(person_id));
    row.set("right", engine::Value::text(std::string(protocol::right_name(right))));
    row.set("granted_at", engine::Value::integer(at));
    row.set("granted_by", engine::Value::text(by));

    // Upsert, not insert: granting a right somebody already has is not an
    // error, it is a shopkeeper making sure.
    upsert(transaction, tables::kPersonRight, right_key(person_id, right), row);
}

bool revoke_right(engine::Transaction& transaction, const std::string& person_id,
                  protocol::RightId right) {
    return transaction.remove(tables::kPersonRight, right_key(person_id, right));
}

void save_device(engine::Transaction& transaction, const Device& device) {
    upsert(transaction, tables::kDevice, device.id, to_row(device));
}

void put_setting(engine::Transaction& transaction, const std::string& key,
                 const std::string& value, std::int64_t at, const std::string& by) {
    engine::Row row;
    row.set("key", engine::Value::text(key));
    row.set("value", engine::Value::text(value));
    row.set("updated_at", engine::Value::integer(at));
    row.set("updated_by", engine::Value::text(by));
    upsert(transaction, tables::kSetting, key, row);
}

void set_module_disabled(engine::Transaction& transaction,
                         const std::vector<protocol::ModuleId>& disabled) {
    // The whole set is rewritten rather than patched. The table is at most
    // twelve rows, and a patch would need to work out what was removed, which
    // is exactly where this kind of code goes wrong.
    const engine::Query existing{tables::kModuleState};
    for (const engine::Row& row : transaction.select(existing)) {
        transaction.remove(tables::kModuleState, row.get("module").text_or({}));
    }

    for (const protocol::ModuleId module : disabled) {
        engine::Row row;
        row.set("module", engine::Value::text(std::string(protocol::module_name(module))));
        transaction.insert(tables::kModuleState, row);
    }
}

void append_audit(engine::Transaction& transaction, const AuditRecord& record) {
    // The identifier has to be unique and has to sort in the order things
    // happened. A timestamp alone does neither when two changes land in the
    // same millisecond, so the count of what is already there breaks the tie.
    const engine::Query all{tables::kAudit};
    const std::size_t existing = transaction.select(all).size();

    std::string id = std::to_string(record.at);
    id += "-";
    id += std::to_string(existing + 1);

    engine::Row row;
    row.set("id", engine::Value::text(id));
    row.set("at", engine::Value::integer(record.at));
    row.set("person", engine::Value::text(record.person));
    row.set("device", engine::Value::text(record.device));
    row.set("operation", engine::Value::text(record.operation));
    row.set("record", engine::Value::text(record.record));
    row.set("summary", engine::Value::text(record.summary));
    transaction.insert(tables::kAudit, row);
}

}  // namespace squiflow::modules::administration::data
