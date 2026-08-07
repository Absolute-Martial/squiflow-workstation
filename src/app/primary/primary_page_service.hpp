#pragma once

#include "app/primary/primary_query.hpp"

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/right_id.hpp>

#include <cstddef>
#include <string_view>

namespace squiflow::app::primary {

class PrimaryPageService final {
  public:
    static constexpr std::size_t kMaximumPageRows = 100;
    static constexpr std::size_t kMaximumOffset = 1000000;
    static constexpr std::size_t kMaximumFilterBytes = 256;
    static constexpr std::size_t kMaximumIdBytes = 64;
    static constexpr std::size_t kMaximumTitleBytes = 512;
    static constexpr std::size_t kMaximumSubtitleBytes = 1024;

    explicit PrimaryPageService(QueryPort& query) noexcept : query_(query) {}

    Result<ListPage, DomainError> list(const RequestContext& context,
                                       const protocol::Activation& activation,
                                       PageKind kind,
                                       const ListRequest& request) const;

    static protocol::ModuleId owner(PageKind kind) noexcept;
    static protocol::RightId read_right(PageKind kind) noexcept;
    static bool valid_field(PageKind kind, std::string_view field,
                            bool filtering) noexcept;

  private:
    QueryPort& query_;
};

}  // namespace squiflow::app::primary
