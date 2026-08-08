#include "app/primary/record_page_service.hpp"

#include "app/primary/primary_page_service.hpp"
#include "engine/records/identity.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_set>
#include <utility>

namespace squiflow::app::primary {
namespace {

DomainError error(DomainErrorCode code, std::string message,
                  std::string field = {}) {
    return {code, std::move(message),
            field.empty() ? std::optional<std::string>{}
                          : std::optional<std::string>{std::move(field)}};
}

bool safe_identifier(std::string_view value, std::size_t maximum,
                     bool allow_dot) noexcept {
    if (value.empty() || value.size() > maximum) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [allow_dot](char raw) {
        const auto c = static_cast<unsigned char>(raw);
        return std::isalnum(c) != 0 || c == static_cast<unsigned char>('_') ||
               c == static_cast<unsigned char>('-') ||
               (allow_dot && c == static_cast<unsigned char>('.'));
    });
}

bool safe_text(std::string_view value, std::size_t maximum) noexcept {
    return value.size() <= maximum &&
           std::none_of(value.begin(), value.end(), [](char raw) {
               return static_cast<unsigned char>(raw) < static_cast<unsigned char>(' ');
           });
}

bool canonical_record_id(std::string_view value) noexcept {
    if (value.size() != 32) {
        return false;
    }
    const auto id = engine::record_id_from_string(value);
    return id.is_valid() && engine::to_string(id) == value;
}

bool valid_field(const FieldSnapshot& field) noexcept {
    return safe_identifier(field.id, RecordPageService::kMaximumIdBytes, true) &&
           safe_identifier(field.label_key, RecordPageService::kMaximumKeyBytes, true) &&
           !field.value_text.empty() &&
           safe_text(field.value_text, RecordPageService::kMaximumDetailBytes);
}

bool valid_line(const LineSnapshot& line) noexcept {
    return safe_identifier(line.id, RecordPageService::kMaximumIdBytes, true) &&
           !line.title.empty() &&
           safe_text(line.title, RecordPageService::kMaximumTitleBytes) &&
           safe_text(line.subtitle, RecordPageService::kMaximumSubtitleBytes) &&
           safe_text(line.quantity_text, RecordPageService::kMaximumTitleBytes) &&
           safe_text(line.amount_text, RecordPageService::kMaximumTitleBytes);
}

bool valid_history(const HistorySnapshot& item) noexcept {
    return safe_identifier(item.id, RecordPageService::kMaximumIdBytes, true) &&
           safe_identifier(item.label_key, RecordPageService::kMaximumKeyBytes, true) &&
           safe_text(item.detail_text, RecordPageService::kMaximumDetailBytes) &&
           item.occurred_at_ms > 0;
}

bool valid_action(const ActionSnapshot& item,
                  std::string_view stable_id) noexcept {
    return safe_identifier(item.id, RecordPageService::kMaximumIdBytes, true) &&
           safe_identifier(item.label_key, RecordPageService::kMaximumKeyBytes, true) &&
           protocol::try_operation(item.operation) != nullptr &&
           item.record_id == stable_id;
}

template <class T, class Projection>
bool unique_ids(const std::vector<T>& values, Projection projection) {
    std::unordered_set<std::string> ids;
    ids.reserve(values.size());
    for (const auto& value : values) {
        if (!ids.insert(std::string(projection(value))).second) {
            return false;
        }
    }
    return true;
}

}  // namespace

Result<RecordSnapshot, DomainError> RecordPageService::load(
    const RequestContext& context, const protocol::Activation& activation,
    PageKind kind, std::string_view stable_id) const {
    if (!is_valid(kind)) {
        return Result<RecordSnapshot, DomainError>::failure(
            error(DomainErrorCode::ValidationFailed,
                  "primary.error.invalid_page_kind", "kind"));
    }
    if (!canonical_record_id(stable_id)) {
        return Result<RecordSnapshot, DomainError>::failure(
            error(DomainErrorCode::ValidationFailed,
                  "record.error.invalid_record_id", "record_id"));
    }

    const auto module = PrimaryPageService::owner(kind);
    const auto right = PrimaryPageService::read_right(kind);
    if (!activation.is_active(module)) {
        return Result<RecordSnapshot, DomainError>::failure(
            error(DomainErrorCode::Unauthorized,
                  "primary.error.module_inactive"));
    }
    if (!context.permissions().has(right)) {
        return Result<RecordSnapshot, DomainError>::failure(
            error(DomainErrorCode::Unauthorized,
                  "primary.error.read_denied"));
    }

    auto loaded = query_.load(kind, stable_id);
    if (!loaded) {
        return Result<RecordSnapshot, DomainError>::failure(loaded.error());
    }

    RecordSnapshot snapshot = std::move(loaded).value();
    if (snapshot.stable_id != stable_id || !canonical_record_id(snapshot.stable_id) ||
        snapshot.fields.size() > kMaximumFields ||
        snapshot.lines.size() > kMaximumLines ||
        snapshot.history.size() > kMaximumHistory ||
        snapshot.actions.size() > kMaximumActions ||
        snapshot.title.empty() ||
        !safe_text(snapshot.title, kMaximumTitleBytes) ||
        !safe_text(snapshot.subtitle, kMaximumSubtitleBytes) ||
        !unique_ids(snapshot.fields, [](const FieldSnapshot& value) { return value.id; }) ||
        !unique_ids(snapshot.lines, [](const LineSnapshot& value) { return value.id; }) ||
        !unique_ids(snapshot.history, [](const HistorySnapshot& value) { return value.id; }) ||
        !unique_ids(snapshot.actions, [](const ActionSnapshot& value) { return value.id; })) {
        return Result<RecordSnapshot, DomainError>::failure(
            error(DomainErrorCode::ValidationFailed,
                  "record.error.invalid_snapshot", "record"));
    }
    if (!std::all_of(snapshot.fields.begin(), snapshot.fields.end(), valid_field) ||
        !std::all_of(snapshot.lines.begin(), snapshot.lines.end(), valid_line) ||
        !std::all_of(snapshot.history.begin(), snapshot.history.end(), valid_history) ||
        !std::all_of(snapshot.actions.begin(), snapshot.actions.end(),
                     [stable_id](const ActionSnapshot& action) {
                         return valid_action(action, stable_id);
                     })) {
        return Result<RecordSnapshot, DomainError>::failure(
            error(DomainErrorCode::ValidationFailed,
                  "record.error.invalid_snapshot", "record"));
    }

    std::erase_if(snapshot.actions, [&](const ActionSnapshot& action) {
        const auto& info = protocol::operation(action.operation);
        return !activation.is_active(info.module) ||
               !context.permissions().has(info.right);
    });
    return Result<RecordSnapshot, DomainError>::success(std::move(snapshot));
}

}  // namespace squiflow::app::primary
