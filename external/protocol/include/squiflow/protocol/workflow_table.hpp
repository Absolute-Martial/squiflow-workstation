#pragma once

#include <span>

#include <squiflow/protocol/operation_table.hpp>

namespace squiflow::protocol {

// The cross-module operations declared in operations/workflows.def.
//
// This classification belongs in the shared protocol rather than in the
// workstation registry: the workstation and server must agree about which
// operations may be implemented by the cross-module layer.
std::span<const OperationId> workflow_operations() noexcept;

// Safe for any OperationId value, including values from a newer build.
bool is_workflow_operation(OperationId operation) noexcept;

}  // namespace squiflow::protocol
