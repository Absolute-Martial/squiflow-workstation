#pragma once
#include <cstdint>
#include <functional>
#include "modules/context.hpp"
namespace squiflow::modules::catalog {
class CatalogService {
public:
    using Clock = std::function<std::int64_t()>;
    explicit CatalogService(Clock clock);
    void create_product(engine::Transaction& tx, const Call& call);
    void update_product(engine::Transaction& tx, const Call& call);
    void archive_product(engine::Transaction& tx, const Call& call);
private:
    Clock clock_;
};
engine::Row read_fields(const Call& call);
const engine::Session& actor(const Call& call);
}  // namespace squiflow::modules::catalog
