#pragma once

#include "app/contracts/result.hpp"
#include "shell/paged_list_cache.hpp"
#include "shell/screen_registry.hpp"
#include "shell/view_model_state.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace squiflow::shell {

struct ListColumn final {
    std::string id{};
    std::string title_key{};
    bool sortable{false};
    bool filterable{false};

    friend bool operator==(const ListColumn&, const ListColumn&) = default;
};

enum class SortDirection : std::uint8_t { Ascending, Descending };

struct ListRequest final {
    std::uint64_t generation{0};
    std::size_t offset{0};
    std::size_t limit{PagedListCache::kMaximumPageRows};
    std::optional<std::string> sort_field{};
    SortDirection sort_direction{SortDirection::Ascending};
    std::optional<std::string> filter_field{};
    std::string filter_text{};

    friend bool operator==(const ListRequest&, const ListRequest&) = default;
};

enum class ListErrorCode : std::uint8_t {
    InvalidColumn,
    InvalidFilter,
    InvalidPage,
    StaleGeneration,
    NoRequestInFlight,
    NoMorePages,
    UnknownRow,
};

struct ListError final {
    ListErrorCode code{ListErrorCode::InvalidPage};
    std::string message_key{};
    std::optional<std::string> field{};

    friend bool operator==(const ListError&, const ListError&) = default;
};

class ListBridge final : public PresentationBridge {
  public:
    static constexpr std::size_t kMaximumColumns = 32;
    static constexpr std::size_t kMaximumFilterBytes = 256;

    explicit ListBridge(std::vector<ListColumn> columns);

    app::Result<ListRequest, ListError> begin_refresh(
        std::optional<std::string> sort_field = {},
        SortDirection direction = SortDirection::Ascending,
        std::optional<std::string> filter_field = {},
        std::string filter_text = {});
    app::Result<ListRequest, ListError> next_page();
    app::Result<void, ListError> apply_page(std::uint64_t generation,
                                           std::vector<RowInput> rows,
                                           bool has_more);
    app::Result<void, ListError> fail(std::uint64_t generation,
                                     std::string message_key);
    app::Result<void, ListError> select(std::string_view stable_id);
    void cancel() noexcept;

    const std::vector<ListColumn>& columns() const noexcept { return columns_; }
    const PagedListCache& cache() const noexcept { return cache_; }
    const ViewModelState& state() const noexcept { return state_; }
    const std::optional<ListRequest>& request_in_flight() const noexcept {
        return in_flight_;
    }

  private:
    const ListColumn* find_column(std::string_view id) const noexcept;
    static bool valid_identifier(std::string_view value) noexcept;
    static ListError error(ListErrorCode code, std::string message_key,
                           std::optional<std::string> field = {});

    std::vector<ListColumn> columns_{};
    PagedListCache cache_{};
    ViewModelState state_{IdleState{}};
    std::optional<ListRequest> in_flight_{};
    ListRequest current_query_{};
    bool has_more_{false};
};

}  // namespace squiflow::shell
