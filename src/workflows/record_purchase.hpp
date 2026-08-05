#pragma once

#include <cstdint>
#include <functional>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using RecordPurchaseClock = std::function<std::int64_t()>;

WorkflowDefinition make_record_purchase(RecordPurchaseClock clock);

}  // namespace squiflow::workflows
