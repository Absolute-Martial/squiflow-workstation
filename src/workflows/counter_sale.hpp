#pragma once

#include <cstdint>
#include <functional>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using CounterSaleClock = std::function<std::int64_t()>;

WorkflowDefinition make_counter_sale(CounterSaleClock clock);

}  // namespace squiflow::workflows
