#pragma once

// A machine that may sign in. There are two: the shopkeeper's and the
// counter's. They are recorded anyway, because "which machine made this
// change" is the first question asked when two versions of a record disagree,
// and because a device that is retired should stop being able to sync.

#include <cstdint>
#include <string>

#include "engine/storage/store.hpp"

namespace squiflow::modules::administration {

struct Device {
    std::string id{};
    std::string name{};
    bool retired{false};
    std::int64_t registered_at{0};
    std::int64_t retired_at{0};
    std::string registered_by{};
};

void validate(const Device& device);

engine::Row to_row(const Device& device);
Device device_from_row(const engine::Row& row);

}  // namespace squiflow::modules::administration
