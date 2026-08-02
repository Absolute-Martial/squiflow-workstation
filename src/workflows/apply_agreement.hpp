#pragma once
#include <cstdint>
#include <functional>
#include "workflows/definition.hpp"
namespace squiflow::workflows {
using ApplyAgreementClock = std::function<std::int64_t()>;
WorkflowDefinition make_apply_agreement(ApplyAgreementClock clock);
}  // namespace squiflow::workflows
