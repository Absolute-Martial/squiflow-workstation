#include "engine/audit/audit_log.hpp"

#include <cstdint>
#include <limits>

#include <squiflow/protocol/module_id.hpp>

namespace squiflow::engine {
namespace {

const std::string kTable = "workflow_audit_entry";

bool blank(const std::string& text) noexcept {
    for (const char c : text) {
        if (static_cast<unsigned char>(c) > ' ') {
            return false;
        }
    }
    return true;
}

std::uint64_t fnv64(std::string_view text, std::uint64_t offset) noexcept {
    constexpr std::uint64_t prime = 1099511628211ULL;
    std::uint64_t hash = offset;
    for (const char c : text) {
        hash ^= static_cast<std::uint64_t>(static_cast<unsigned char>(c));
        hash *= prime;
    }
    return hash;
}

void validate_entry(const std::string& idempotency_key, const AuditEntry& entry) {
    if (idempotency_key.empty()) {
        throw StoreError("an audit entry needs an idempotency key");
    }
    if (!entry.id.is_valid() || entry.id != AuditLog::id_for(idempotency_key)) {
        throw StoreError("an audit entry needs the deterministic id for its key");
    }
    if (!protocol::is_valid(entry.operation)) {
        throw StoreError("an audit entry names an operation this build does not have");
    }
    if (!entry.person.is_valid() || !entry.device.is_valid()) {
        throw StoreError("an audit entry must identify its person and device");
    }
    if (entry.at.ms <= 0) {
        throw StoreError("an audit entry must record a positive timestamp");
    }
    if (!entry.subject.is_valid() || !protocol::is_valid(entry.subject.module)) {
        throw StoreError("an audit entry must identify a valid subject");
    }
    if (blank(entry.detail)) {
        throw StoreError("an audit entry needs a human-readable detail");
    }
}

Row to_row(const std::string& idempotency_key, const AuditEntry& entry) {
    Row row;
    row.set("idempotency_key", Value::text(idempotency_key));
    row.set("id", Value::text(to_string(entry.id)));
    row.set("operation", Value::integer(static_cast<std::int64_t>(entry.operation)));
    row.set("person", Value::text(to_string(entry.person)));
    row.set("device", Value::text(to_string(entry.device)));
    row.set("at", Value::integer(entry.at.ms));
    row.set("subject_module",
            Value::integer(static_cast<std::int64_t>(entry.subject.module)));
    row.set("subject_record", Value::text(to_string(entry.subject.record)));
    row.set("detail", Value::text(entry.detail));
    return row;
}

}  // namespace

const std::string& AuditLog::table_name() {
    return kTable;
}

void AuditLog::define(Store& store) {
    store.define_table(kTable, "idempotency_key");
}

RecordId AuditLog::id_for(std::string_view idempotency_key) noexcept {
    RecordId id;
    id.high = fnv64(idempotency_key, 14695981039346656037ULL);
    id.low = fnv64(idempotency_key, 1099511628211ULL);
    if (!id.is_valid()) {
        id.low = 1;
    }
    return id;
}

void AuditLog::record(Transaction& transaction,
                      const std::string& idempotency_key,
                      const AuditEntry& entry) {
    validate_entry(idempotency_key, entry);
    if (transaction.find(kTable, idempotency_key)) {
        throw StoreError("that workflow audit entry is already on file");
    }
    transaction.insert(kTable, to_row(idempotency_key, entry));
}

AuditEntry audit_entry_from_row(const Row& row) noexcept {
    AuditEntry entry;
    entry.id = record_id_from_string(row.get("id").text_or({}));

    const std::int64_t operation_number = row.get("operation").integer_or(-1);
    if (operation_number >= 0 &&
        operation_number <= std::numeric_limits<std::uint32_t>::max()) {
        const auto* info = protocol::find_operation(
            static_cast<std::uint32_t>(operation_number));
        entry.operation = info == nullptr ? protocol::OperationId::Count : info->id;
    } else {
        entry.operation = protocol::OperationId::Count;
    }

    entry.person = record_id_from_string(row.get("person").text_or({}));
    entry.device = record_id_from_string(row.get("device").text_or({}));
    entry.at.ms = row.get("at").integer_or(0);

    const std::int64_t module_number = row.get("subject_module").integer_or(-1);
    protocol::ModuleId module = protocol::ModuleId::Count;
    if (module_number >= 0 &&
        module_number <= std::numeric_limits<std::uint32_t>::max()) {
        protocol::module_from_number(static_cast<std::uint32_t>(module_number), module);
    }
    entry.subject.module = module;
    entry.subject.record =
        record_id_from_string(row.get("subject_record").text_or({}));
    entry.detail = row.get("detail").text_or({});
    return entry;
}

}  // namespace squiflow::engine
