#pragma once
#include "app/startup.hpp"
#include <mutex>
#include <thread>
namespace squiflow::app {
class StartupSequence final {
public:
 explicit StartupSequence(StartupRuntime& runtime):runtime_(runtime){}
 StartupSequence(const StartupSequence&)=delete;StartupSequence& operator=(const StartupSequence&)=delete;
 StartupResult start();
 void shutdown(ShutdownReason reason) noexcept;
 LifecycleState state() const noexcept;
 std::vector<StartupStep> completed_steps() const;
 std::vector<RollbackFailure> rollback_failures() const;
private:
 void unwind(ShutdownReason reason) noexcept;
 void record_rollback_failure(StartupStep step,std::string_view message) noexcept;
 StartupRuntime& runtime_; mutable std::mutex mutex_; LifecycleState state_{LifecycleState::Idle};
 std::vector<StartupStep> completed_; std::vector<RollbackFailure> rollback_failures_;
 bool stop_requested_{false}; ShutdownReason requested_reason_{ShutdownReason::NormalExit}; std::thread::id startup_thread_{};
};
}
