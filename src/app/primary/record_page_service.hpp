#pragma once

#include "app/primary/record_query.hpp"

#include <squiflow/protocol/module_graph.hpp>

#include <cstddef>
#include <string_view>

namespace squiflow::app::primary {

class RecordPageService final {
  public:
    static constexpr std::size_t kMaximumFields = 48;
    static constexpr std::size_t kMaximumLines = 64;
    static constexpr std::size_t kMaximumHistory = 64;
    static constexpr std::size_t kMaximumActions = 16;
    static constexpr std::size_t kMaximumIdBytes = 128;
    static constexpr std::size_t kMaximumKeyBytes = 128;
    static constexpr std::size_t kMaximumTitleBytes = 512;
    static constexpr std::size_t kMaximumSubtitleBytes = 1024;
    static constexpr std::size_t kMaximumDetailBytes = 4096;

    explicit RecordPageService(RecordQueryPort& query) noexcept : query_(query) {}

    Result<RecordSnapshot, DomainError> load(const RequestContext& context,
                                             const protocol::Activation& activation,
                                             PageKind kind,
                                             std::string_view stable_id) const;

  private:
    RecordQueryPort& query_;
};

}  // namespace squiflow::app::primary
