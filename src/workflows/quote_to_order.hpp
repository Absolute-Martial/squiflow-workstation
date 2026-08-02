#pragma once

#include <cstdint>
#include <functional>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using WorkflowClock = std::function<std::int64_t()>;

// Copy one exact accepted quotation revision into an order. Prices are copied
// from the frozen revision; this workflow deliberately has no pricing lookup.
WorkflowDefinition make_quote_to_order(WorkflowClock clock);

}  // namespace squiflow::workflows
