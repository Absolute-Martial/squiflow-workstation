#pragma once

#include <cstdint>
#include <functional>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using IssueInvoiceClock = std::function<std::int64_t()>;

// Freeze an existing invoice draft and consume one final number from a block
// reserved for the current device.  Draft lines are already the commercial
// snapshot: issuance validates and locks them but never reprices or rewrites
// them.
WorkflowDefinition make_issue_invoice(IssueInvoiceClock clock);

}  // namespace squiflow::workflows
