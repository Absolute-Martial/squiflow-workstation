#pragma once

#include <string>

#include "engine/records/identity.hpp"

namespace squiflow::engine {

// Stable identity supplied by the platform file agent. Path is deliberately
// absent: rename and move on one volume must not create a new identity.
struct LocalFileIdentity {
    DeviceId device{};
    std::string volume_id{};
    std::string file_id{};

    bool is_valid() const noexcept {
        return device.is_valid() && !volume_id.empty() && !file_id.empty();
    }

    friend bool operator==(const LocalFileIdentity&, const LocalFileIdentity&) = default;
};

}  // namespace squiflow::engine
