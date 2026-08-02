#pragma once
#include <functional>
#include <string>
#include <vector>
#include <squiflow/protocol/module_id.hpp>
#include <squiflow/protocol/operation_table.hpp>
#include "engine/records/reference.hpp"
#include "engine/storage/store.hpp"
#include "modules/context.hpp"
namespace squiflow::workflows {
struct WorkflowResult { engine::Reference audit_subject{}; std::string audit_detail{}; };
using WorkflowHandler = std::function<WorkflowResult(engine::Transaction&, const modules::Call&)>;
struct WorkflowDefinition { protocol::OperationId operation{}; std::vector<protocol::ModuleId> requirements{}; WorkflowHandler handler{}; };
}  // namespace squiflow::workflows
