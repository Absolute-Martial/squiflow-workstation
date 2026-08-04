#include "shell/list_bridge.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace squiflow::shell {

bool ListBridge::valid_identifier(std::string_view value) noexcept {
    return !value.empty() && value.size() <= 64 &&
        std::all_of(value.begin(), value.end(), [](char raw) {
            const auto c = static_cast<unsigned char>(raw);
            return (c >= static_cast<unsigned char>('a') &&
                    c <= static_cast<unsigned char>('z')) ||
                   (c >= static_cast<unsigned char>('0') &&
                    c <= static_cast<unsigned char>('9')) ||
                   c == static_cast<unsigned char>('_');
        });
}

ListError ListBridge::error(ListErrorCode code, std::string message_key,
                            std::optional<std::string> field) {
    return {code, std::move(message_key), std::move(field)};
}

ListBridge::ListBridge(std::vector<ListColumn> columns) : columns_(std::move(columns)) {
    if (columns_.empty() || columns_.size() > kMaximumColumns) {
        throw std::invalid_argument("list needs a bounded nonempty column set");
    }
    for (std::size_t index = 0; index < columns_.size(); ++index) {
        const ListColumn& column = columns_[index];
        if (!valid_identifier(column.id) || column.title_key.empty() ||
            column.title_key.size() > 128) {
            throw std::invalid_argument("invalid list column");
        }
        for (std::size_t prior = 0; prior < index; ++prior) {
            if (columns_[prior].id == column.id) {
                throw std::invalid_argument("duplicate list column");
            }
        }
    }
}

const ListColumn* ListBridge::find_column(std::string_view id) const noexcept {
    const auto found = std::find_if(columns_.begin(), columns_.end(),
                                    [id](const ListColumn& column) {
                                        return column.id == id;
                                    });
    return found == columns_.end() ? nullptr : &*found;
}

app::Result<ListRequest, ListError> ListBridge::begin_refresh(
    std::optional<std::string> sort_field, SortDirection direction,
    std::optional<std::string> filter_field, std::string filter_text) {
    if (sort_field) {
        const ListColumn* column = find_column(*sort_field);
        if (column == nullptr || !column->sortable) {
            return app::Result<ListRequest, ListError>::failure(
                error(ListErrorCode::InvalidColumn, "list.error.invalid_sort_field",
                      std::move(sort_field)));
        }
    }
    if (filter_text.size() > kMaximumFilterBytes) {
        return app::Result<ListRequest, ListError>::failure(
            error(ListErrorCode::InvalidFilter, "list.error.filter_too_long",
                  std::move(filter_field)));
    }
    if (!filter_text.empty()) {
        if (!filter_field) {
            return app::Result<ListRequest, ListError>::failure(
                error(ListErrorCode::InvalidFilter, "list.error.filter_field_required"));
        }
        const ListColumn* column = find_column(*filter_field);
        if (column == nullptr || !column->filterable) {
            return app::Result<ListRequest, ListError>::failure(
                error(ListErrorCode::InvalidColumn, "list.error.invalid_filter_field",
                      std::move(filter_field)));
        }
    } else {
        filter_field.reset();
    }

    current_query_ = {};
    current_query_.generation = cache_.begin_query();
    current_query_.limit = PagedListCache::kMaximumPageRows;
    current_query_.sort_field = std::move(sort_field);
    current_query_.sort_direction = direction;
    current_query_.filter_field = std::move(filter_field);
    current_query_.filter_text = std::move(filter_text);
    has_more_ = false;
    in_flight_ = current_query_;
    state_ = LoadingState{current_query_.generation};
    return app::Result<ListRequest, ListError>::success(current_query_);
}

app::Result<ListRequest, ListError> ListBridge::next_page() {
    if (in_flight_) {
        return app::Result<ListRequest, ListError>::failure(
            error(ListErrorCode::NoRequestInFlight, "list.error.request_already_running"));
    }
    if (!has_more_) {
        return app::Result<ListRequest, ListError>::failure(
            error(ListErrorCode::NoMorePages, "list.error.no_more_pages"));
    }
    current_query_.offset = cache_.row_count();
    in_flight_ = current_query_;
    state_ = LoadingState{current_query_.generation};
    return app::Result<ListRequest, ListError>::success(current_query_);
}

app::Result<void, ListError> ListBridge::apply_page(
    std::uint64_t generation, std::vector<RowInput> rows, bool has_more) {
    if (generation != current_query_.generation) {
        return app::Result<void, ListError>::failure(
            error(ListErrorCode::StaleGeneration, "list.error.stale_generation"));
    }
    if (!in_flight_ || in_flight_->generation != generation) {
        return app::Result<void, ListError>::failure(
            error(ListErrorCode::NoRequestInFlight, "list.error.no_request"));
    }
    try {
        if (!cache_.apply(generation, std::move(rows))) {
            return app::Result<void, ListError>::failure(
                error(ListErrorCode::StaleGeneration, "list.error.stale_generation"));
        }
    } catch (const std::exception&) {
        return app::Result<void, ListError>::failure(
            error(ListErrorCode::InvalidPage, "list.error.invalid_page"));
    }
    in_flight_.reset();
    has_more_ = has_more;
    state_ = ReadyState{false, has_more};
    return app::Result<void, ListError>::success();
}

app::Result<void, ListError> ListBridge::fail(std::uint64_t generation,
                                              std::string message_key) {
    if (generation != current_query_.generation) {
        return app::Result<void, ListError>::failure(
            error(ListErrorCode::StaleGeneration, "list.error.stale_generation"));
    }
    if (!in_flight_) {
        return app::Result<void, ListError>::failure(
            error(ListErrorCode::NoRequestInFlight, "list.error.no_request"));
    }
    in_flight_.reset();
    has_more_ = false;
    state_ = FailedState{std::move(message_key)};
    return app::Result<void, ListError>::success();
}

app::Result<void, ListError> ListBridge::select(std::string_view stable_id) {
    cache_.select(stable_id);
    if (!cache_.selected()) {
        return app::Result<void, ListError>::failure(
            error(ListErrorCode::UnknownRow, "list.error.unknown_row"));
    }
    return app::Result<void, ListError>::success();
}

void ListBridge::cancel() noexcept {
    current_query_.generation = cache_.begin_query();
    current_query_.offset = 0;
    in_flight_.reset();
    has_more_ = false;
    state_ = IdleState{};
}

}  // namespace squiflow::shell
