#pragma once

#include <cstdint>
#include <functional>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using DocumentDeliveryClock = std::function<std::int64_t()>;

WorkflowDefinition make_prepare_document_delivery(DocumentDeliveryClock clock);
WorkflowDefinition make_request_document_delivery(DocumentDeliveryClock clock);

}  // namespace squiflow::workflows
