#pragma once

#include "app/contracts/domain_error.hpp"
#include "app/contracts/request_context.hpp"
#include "app/contracts/result.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace squiflow::app::primary {

enum class PageKind : std::uint8_t {
    Parties,
    Catalog,
    Pricing,
    Orders,
    Receivables,
};

struct ListRequest final {
    std::size_t offset{0};
    std::size_t limit{50};
    std::string sort_field{};
    bool descending{false};
    std::string filter_field{};
    std::string filter_text{};

    friend bool operator==(const ListRequest&, const ListRequest&) = default;
};

struct ListRow final {
    std::string stable_id{};
    std::string title{};
    std::string subtitle{};

    friend bool operator==(const ListRow&, const ListRow&) = default;
};

struct ListPage final {
    std::vector<ListRow> rows{};
    bool has_more{false};

    friend bool operator==(const ListPage&, const ListPage&) = default;
};

class QueryPort {
  public:
    virtual ~QueryPort() = default;
    virtual Result<ListPage, DomainError> load(PageKind kind,
                                                const ListRequest& request) = 0;
};

}  // namespace squiflow::app::primary
