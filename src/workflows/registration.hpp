#pragma once

#include <cstdint>
#include <functional>

namespace squiflow::modules {
class Registry;
}

namespace squiflow::workflows {

using RegistrationClock = std::function<std::int64_t()>;

void register_all_workflows(modules::Registry& registry,
                            RegistrationClock clock);

}  // namespace squiflow::workflows
