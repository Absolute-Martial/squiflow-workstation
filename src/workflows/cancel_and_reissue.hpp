#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include "workflows/definition.hpp"

namespace squiflow::workflows {

using CancelAndReissueClock = std::function<std::int64_t()>;

// Derive one replacement line identity from the new invoice and the exact
// source line.  Exposed so clients and tests can predict target collisions.
std::string replacement_invoice_line_id(
    const std::string& replacement_invoice_id,
    const std::string& source_invoice_line_id);

// Cancel permanent issued evidence, release its active allocations, and copy
// it into an editable unnumbered replacement draft.  Issuing that draft later
// completes the permanent two-way link.
WorkflowDefinition make_cancel_and_reissue(CancelAndReissueClock clock);

}  // namespace squiflow::workflows
