#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <optional>
#include <stop_token>
#include <string>

namespace squiflow::platform {
inline constexpr std::size_t kMaxBackgroundServices=32;
inline constexpr std::size_t kMaxBackgroundQueue=64;
inline constexpr std::size_t kMaxBackgroundId=64;
inline constexpr std::size_t kMaxBackgroundError=256;
enum class BackgroundLane { Synchronization, Shared };
enum class BackgroundTrigger : std::uint32_t { Explicit=1U, DataChanged=2U, NetworkAvailable=4U, ConnectionRestored=8U, ApplicationStarted=16U, Idle=32U, DayRollover=64U, CoarseTick=128U, Shutdown=256U };
enum class BackgroundState { Idle, Pending, Running, Disabled, Stopping };
enum class BackgroundOutcome { NeverRun, Succeeded, Cancelled, Failed, Disabled };
enum class SubmissionResult { Accepted, Coalesced, QueueFull, Stopping, UnknownService, Disabled, Gated };
enum class ShutdownResult { Clean, AlreadyStopped, DeadlineExceeded };
using BackgroundClock=std::chrono::steady_clock;
using BackgroundTask=std::function<void(std::stop_token)>;
struct BackgroundServiceDefinition { std::string id; BackgroundLane lane{BackgroundLane::Shared}; BackgroundTask task; std::uint32_t failure_limit{3}; bool network_required{false}; bool idle_required{false}; std::optional<BackgroundClock::duration> interval; };
struct BackgroundServiceStatus { std::string id; BackgroundState state{BackgroundState::Idle}; BackgroundOutcome last_outcome{BackgroundOutcome::NeverRun}; std::uint32_t pending_triggers{0}; std::uint32_t consecutive_failures{0}; std::uint64_t run_count{0}; std::optional<BackgroundClock::time_point> last_started; std::optional<BackgroundClock::time_point> last_finished; std::optional<BackgroundClock::time_point> next_eligible; std::string last_error; };
void validate_background_service(const BackgroundServiceDefinition& definition);
std::uint32_t trigger_bit(BackgroundTrigger trigger) noexcept;
}
