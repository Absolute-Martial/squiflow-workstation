#pragma once

#include <squiflow/protocol/module_id.hpp>

#include "engine/records/identity.hpp"

namespace squiflow::engine {

// A pointer to any record in any module, without depending on that module.
//
// This is what lets tasks, approvals, attention items and audit rows attach to
// anything at all while the one-way dependency rule stays intact.
struct Reference {
    protocol::ModuleId module = protocol::ModuleId::administration;
    RecordId record;

    constexpr bool is_valid() const noexcept { return record.is_valid(); }

    friend constexpr bool operator==(const Reference&, const Reference&) = default;
};

}  // namespace squiflow::engine
