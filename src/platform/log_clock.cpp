#include "platform/log_clock.hpp"

#include <chrono>

namespace squiflow::platform {

std::int64_t SystemLogClock::now_milliseconds() const {
    const auto now = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

}  // namespace squiflow::platform
