#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using OrderToJobsClock = std::function<std::int64_t()>;

// Deterministically derives one job id from the conversion identity supplied
// as Call::record_id and the exact source order-line id. Exposed so clients and
// tests can predict collisions without reproducing the algorithm.
std::string order_job_id(const std::string& conversion_id,
                         const std::string& order_line_id);

// Convert all lines, or an explicit non-empty subset, from one open order.
// Every selected source line becomes exactly one draft job. Commercial values
// are copied from the frozen order snapshot; pricing is never consulted.
WorkflowDefinition make_order_to_jobs(OrderToJobsClock clock);

}  // namespace squiflow::workflows
