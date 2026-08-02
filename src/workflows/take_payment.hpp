#pragma once

#include <cstdint>
#include <functional>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using TakePaymentClock = std::function<std::int64_t()>;

// Record incoming customer money without inventing a tracking number or
// allocating it automatically. A cheque number, bank reference, or any other
// external evidence is optional free text.
WorkflowDefinition make_take_payment(TakePaymentClock clock);

}  // namespace squiflow::workflows
