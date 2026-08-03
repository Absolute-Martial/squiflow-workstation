#include "platform/background_service.hpp"
#include <stdexcept>
namespace squiflow::platform {
void validate_background_service(const BackgroundServiceDefinition& d){if(d.id.empty())throw std::invalid_argument("background service id is empty");if(d.id.size()>kMaxBackgroundId)throw std::length_error("background service id is too long");if(!d.task)throw std::invalid_argument("background service task is empty");if(d.failure_limit==0U)throw std::invalid_argument("background failure limit is zero");if(d.interval && *d.interval<=BackgroundClock::duration::zero())throw std::invalid_argument("background interval must be positive");}
std::uint32_t trigger_bit(BackgroundTrigger trigger) noexcept{return static_cast<std::uint32_t>(trigger);}
}
