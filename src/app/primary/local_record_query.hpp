#pragma once

#include "app/primary/record_query.hpp"

namespace squiflow::engine {
class Database;
}

namespace squiflow::app::primary {

class LocalRecordQuery final : public RecordQueryPort {
  public:
    explicit LocalRecordQuery(engine::Database& database) noexcept : database_(database) {}

    Result<RecordSnapshot, DomainError> load(PageKind kind,
                                             std::string_view stable_id) override;

  private:
    engine::Database& database_;
};

}  // namespace squiflow::app::primary
