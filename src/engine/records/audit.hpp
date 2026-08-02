#pragma once

#include <string>

#include <squiflow/protocol/operation_table.hpp>

#include "engine/records/identity.hpp"
#include "engine/records/reference.hpp"

namespace squiflow::engine {

// Who did what, on which device, when.
//
// Written inside the same transaction as the change it describes. Not
// afterwards, not from a queue: an audit trail that can be missing entries for
// the interesting cases is worse than none, because it is trusted.
struct AuditEntry {
    RecordId id;
    protocol::OperationId operation = protocol::OperationId::Count;
    PersonId person;
    DeviceId device;
    Timestamp at;
    Reference subject;
    // Human-readable, written at the moment it happened. Reconstructing what a
    // change meant years later from raw column values does not work.
    std::string detail;
};

}  // namespace squiflow::engine
