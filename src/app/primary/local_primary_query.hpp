#pragma once

#include "app/primary/primary_query.hpp"
#include "engine/storage/database.hpp"

namespace squiflow::app::primary {

// Local query adapter. It is read-only, bounded by the request, and returns
// immutable presentation snapshots instead of leaking storage rows to Qt.
class LocalPrimaryQuery final : public QueryPort {
  public:
    explicit LocalPrimaryQuery(engine::Database& database) noexcept
        : database_(database) {}

    Result<ListPage, DomainError> load(PageKind kind,
                                        const ListRequest& request) override;

  private:
    engine::Database& database_;
};

}  // namespace squiflow::app::primary
