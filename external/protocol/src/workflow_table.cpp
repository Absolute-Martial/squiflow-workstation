#include <squiflow/protocol/workflow_table.hpp>

#include <array>
#include <cstddef>

namespace squiflow::protocol {
namespace {

constexpr OperationId kWorkflowOperations[] = {
#define SQF_OPERATION(name, module, right, cls, offline) OperationId::name,
#include <squiflow/protocol/operations/workflows.def>
#undef SQF_OPERATION
};

static_assert(std::size(kWorkflowOperations) == 8,
              "the workflow protocol surface changed; review the framework contract");

}  // namespace

std::span<const OperationId> workflow_operations() noexcept {
    return std::span<const OperationId>{kWorkflowOperations};
}

bool is_workflow_operation(OperationId operation) noexcept {
    for (const OperationId candidate : kWorkflowOperations) {
        if (candidate == operation) {
            return true;
        }
    }
    return false;
}

}  // namespace squiflow::protocol
