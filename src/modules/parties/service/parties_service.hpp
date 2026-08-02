#pragma once

#include <cstdint>
#include <functional>
#include <vector>

#include "engine/storage/store.hpp"
#include "modules/context.hpp"

namespace squiflow::modules::parties {

class PartiesService {
public:
    using Clock = std::function<std::int64_t()>;
    explicit PartiesService(Clock clock);

    void create_party(engine::Transaction& transaction, const Call& call);
    void update_party(engine::Transaction& transaction, const Call& call);
    void archive_party(engine::Transaction& transaction, const Call& call);
    void set_terms(engine::Transaction& transaction, const Call& call);
    void add_contact(engine::Transaction& transaction, const Call& call);

private:
    Clock clock_;
};

// Payload helpers shared with the test.
engine::Row read_fields(const Call& call);
const engine::Session& actor(const Call& call);

}  // namespace squiflow::modules::parties
