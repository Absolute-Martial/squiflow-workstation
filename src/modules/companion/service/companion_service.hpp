#pragma once

#include <cstdint>
#include <functional>

#include "engine/storage/store.hpp"
#include "modules/companion/domain/task.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::companion {

class CompanionService {
public:
    using Clock = std::function<std::int64_t()>;
    explicit CompanionService(Clock clock) : clock_{std::move(clock)} {}

    void create(engine::Transaction& transaction, const Call& call) const;
    void update(engine::Transaction& transaction, const Call& call) const;
    void complete(engine::Transaction& transaction, const Call& call) const;
    void snooze(engine::Transaction& transaction, const Call& call) const;

    // Later deterministic rule evaluators use this path. The source key is
    // checked before either row is written, so one rule cannot create twins.
    void create_attention(engine::Transaction& transaction,
                          const Task& task,
                          const TaskEvent& event) const;

private:
    Clock clock_;
};

}  // namespace squiflow::modules::companion
