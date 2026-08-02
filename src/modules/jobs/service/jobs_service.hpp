#pragma once

#include <cstdint>
#include <functional>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::jobs {

class JobsService {
public:
    using Clock = std::function<std::int64_t()>;

    explicit JobsService(Clock clock) : clock_{std::move(clock)} {}

    void create(engine::Transaction& transaction, const Call& call) const;
    void update(engine::Transaction& transaction, const Call& call) const;
    void state_change(engine::Transaction& transaction, const Call& call) const;
    void cancel(engine::Transaction& transaction, const Call& call) const;

private:
    Clock clock_;
};

}  // namespace squiflow::modules::jobs
