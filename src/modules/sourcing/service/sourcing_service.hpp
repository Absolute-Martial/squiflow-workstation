#pragma once

// Four protocol handlers live here. Purchase creation is deliberately not a
// fifth handler: the protocol owns it as the later record_purchase workflow.
// That workflow will call record_purchase inside its transaction, so the rules
// below remain the single path by which a purchase enters the module.

#include <cstddef>
#include <cstdint>
#include <functional>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"
#include "modules/sourcing/domain/sourcing.hpp"

namespace squiflow::modules::sourcing {

inline constexpr std::size_t kDefaultLookupLimit = 100;
inline constexpr std::size_t kMaxLookupLimit = 500;

class SourcingService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit SourcingService(Clock clock) : clock_{std::move(clock)} {}

    void create_supplier(engine::Transaction& transaction, const Call& call) const;
    void update_supplier(engine::Transaction& transaction, const Call& call) const;
    void settle(engine::Transaction& transaction, const Call& call) const;
    std::vector<engine::Row> lookup(const engine::Store& store, const Call& call) const;

    // Module-level rule used by the Phase 5 record_purchase workflow. Both
    // objects are checked before either is written, so a bad purchase cannot
    // leave behind a material that never actually arrived.
    void record_purchase(engine::Transaction& transaction,
                         const Material& material,
                         const Purchase& purchase) const;

private:
    Clock clock_;
};

}  // namespace squiflow::modules::sourcing
