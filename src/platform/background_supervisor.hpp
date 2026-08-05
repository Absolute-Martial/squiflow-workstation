#pragma once
#include "platform/background_executor.hpp"
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
namespace squiflow::platform {
class BackgroundSupervisor final {
public:
 explicit BackgroundSupervisor(std::size_t queue_capacity=kMaxBackgroundQueue);~BackgroundSupervisor();
 BackgroundSupervisor(const BackgroundSupervisor&)=delete;BackgroundSupervisor& operator=(const BackgroundSupervisor&)=delete;
 void register_service(BackgroundServiceDefinition definition);void seal();
 SubmissionResult trigger(std::string_view id,BackgroundTrigger reason);
 void coarse_tick(BackgroundClock::time_point now);
 void calendar_day(std::chrono::year_month_day local_day);
 void set_network_authorized(bool value);void set_idle(bool value);
 std::vector<BackgroundServiceStatus> statuses() const;std::optional<BackgroundServiceStatus> status(std::string_view id) const;
 ShutdownResult shutdown(BackgroundClock::duration timeout);bool sealed() const;
private:
 struct Entry {BackgroundServiceDefinition definition;BackgroundServiceStatus status;bool submission_pending{false};bool rerun{false};std::uint32_t rerun_triggers{0};};
 SubmissionResult trigger_locked(Entry& entry,BackgroundTrigger reason,std::unique_lock<std::mutex>& lock);
 SubmissionResult dispatch_locked(Entry& entry,std::unique_lock<std::mutex>& lock);
 void completed(const std::string& id,BackgroundOutcome outcome,std::string error);
 void publish_locked();bool gated(const Entry& entry) const noexcept;
 mutable std::mutex mutex_;std::unordered_map<std::string,Entry> entries_;bool sealed_{false};bool stopping_{false};bool network_authorized_{false};bool idle_{false};std::optional<std::chrono::sys_days> last_calendar_day_;BackgroundExecutor executor_;std::shared_ptr<const std::vector<BackgroundServiceStatus>> snapshot_;
};
}
