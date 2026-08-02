#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "engine/records/audit.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::engine {

class AuditLog {
public:
    // Keyed by workflow idempotency key. Retrying one workflow therefore
    // cannot produce a second audit row even if the caller lost the response.
    static const std::string& table_name();
    static void define(Store& store);

    // Stable across devices and process restarts. The table key remains the
    // full idempotency string; this identifier is the typed AuditEntry id.
    static RecordId id_for(std::string_view idempotency_key) noexcept;

    static void record(Transaction& transaction,
                       const std::string& idempotency_key,
                       const AuditEntry& entry);
};

AuditEntry audit_entry_from_row(const Row& row) noexcept;

template <typename Reader>
std::optional<AuditEntry> find_audit(const Reader& reader,
                                     const std::string& idempotency_key) {
    const auto row = reader.find(AuditLog::table_name(), idempotency_key);
    return row ? std::optional<AuditEntry>{audit_entry_from_row(*row)} : std::nullopt;
}

template <typename Reader>
std::vector<AuditEntry> audits_for_operation(const Reader& reader,
                                             protocol::OperationId operation) {
    Query query{AuditLog::table_name()};
    query.where_equals("operation", Value::integer(static_cast<std::int64_t>(operation)));
    query.order_by("at").order_by("idempotency_key");
    std::vector<AuditEntry> result;
    for (const Row& row : reader.select(query)) {
        result.push_back(audit_entry_from_row(row));
    }
    return result;
}

template <typename Reader>
std::vector<AuditEntry> audits_for_subject(const Reader& reader,
                                           const Reference& subject) {
    if (!subject.is_valid() || !protocol::is_valid(subject.module)) {
        return {};
    }
    Query query{AuditLog::table_name()};
    query.where_equals("subject_module",
                       Value::integer(static_cast<std::int64_t>(subject.module)));
    query.where_equals("subject_record", Value::text(to_string(subject.record)));
    query.order_by("at").order_by("idempotency_key");
    std::vector<AuditEntry> result;
    for (const Row& row : reader.select(query)) {
        result.push_back(audit_entry_from_row(row));
    }
    return result;
}

}  // namespace squiflow::engine
