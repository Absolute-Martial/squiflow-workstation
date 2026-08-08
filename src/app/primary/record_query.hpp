#pragma once

#include "app/contracts/domain_error.hpp"
#include "app/contracts/result.hpp"
#include "app/primary/primary_query.hpp"

#include <squiflow/protocol/operation_table.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace squiflow::app::primary {

struct FieldSnapshot final {
    std::string id{};
    std::string label_key{};
    std::string value_text{};
    std::optional<std::int64_t> exact_minor_units{};
    std::optional<std::int64_t> exact_scaled_units{};

    friend bool operator==(const FieldSnapshot&, const FieldSnapshot&) = default;
};

struct LineSnapshot final {
    std::string id{};
    std::string title{};
    std::string subtitle{};
    std::string quantity_text{};
    std::string amount_text{};
    std::optional<std::int64_t> exact_scaled_units{};
    std::optional<std::int64_t> exact_minor_units{};

    friend bool operator==(const LineSnapshot&, const LineSnapshot&) = default;
};

struct HistorySnapshot final {
    std::string id{};
    std::string label_key{};
    std::string detail_text{};
    std::int64_t occurred_at_ms{0};

    friend bool operator==(const HistorySnapshot&, const HistorySnapshot&) = default;
};

struct ActionSnapshot final {
    std::string id{};
    std::string label_key{};
    protocol::OperationId operation{protocol::OperationId::Count};
    std::string record_id{};

    friend bool operator==(const ActionSnapshot&, const ActionSnapshot&) = default;
};

struct RecordSnapshot final {
    std::string stable_id{};
    std::string title{};
    std::string subtitle{};
    std::vector<FieldSnapshot> fields{};
    std::vector<LineSnapshot> lines{};
    std::vector<HistorySnapshot> history{};
    std::vector<ActionSnapshot> actions{};

    friend bool operator==(const RecordSnapshot&, const RecordSnapshot&) = default;
};

class RecordQueryPort {
  public:
    virtual ~RecordQueryPort() = default;
    virtual Result<RecordSnapshot, DomainError> load(PageKind kind,
                                                     std::string_view stable_id) = 0;
};

}  // namespace squiflow::app::primary
