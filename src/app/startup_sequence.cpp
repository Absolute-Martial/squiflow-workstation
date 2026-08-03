#include "app/startup_sequence.hpp"
#include <exception>
#include <stdexcept>
namespace squiflow::app {
StartupResult StartupSequence::start(){
 {std::scoped_lock lock(mutex_);if(state_!=LifecycleState::Idle)return {StartupDisposition::Failed,StartupFailure{StartupStep::Paths,"startup may run only once"}};state_=LifecycleState::Starting;startup_thread_=std::this_thread::get_id();}
 for(const auto step:startup_order()){
  StepResult result;
  try{result=runtime_.start(step);}catch(const std::exception& e){result={StepDisposition::Failed,bounded_lifecycle_message(e.what())};}catch(...){result={StepDisposition::Failed,"unknown startup exception"};}
  bool requested=false;ShutdownReason reason=ShutdownReason::StartupFailure;
  {std::scoped_lock lock(mutex_);if(result.disposition==StepDisposition::Started)completed_.push_back(step);requested=stop_requested_;reason=requested_reason_;}
  if(result.disposition==StepDisposition::SecondaryInstance){unwind(ShutdownReason::NormalExit);std::scoped_lock lock(mutex_);state_=LifecycleState::SecondaryInstance;return {StartupDisposition::SecondaryInstance,std::nullopt};}
  if(result.disposition==StepDisposition::Failed){unwind(ShutdownReason::StartupFailure);std::scoped_lock lock(mutex_);state_=LifecycleState::Failed;return {StartupDisposition::Failed,StartupFailure{step,bounded_lifecycle_message(result.message)}};}
  if(requested){unwind(reason);std::scoped_lock lock(mutex_);state_=LifecycleState::Stopped;return {StartupDisposition::Failed,StartupFailure{step,"shutdown requested during startup"}};}
 }
 {std::scoped_lock lock(mutex_);state_=LifecycleState::Running;startup_thread_={};}
 return {StartupDisposition::Running,std::nullopt};
}
void StartupSequence::shutdown(ShutdownReason reason) noexcept{
 {std::scoped_lock lock(mutex_);if(state_==LifecycleState::Stopped||state_==LifecycleState::Stopping||state_==LifecycleState::SecondaryInstance)return;if(state_==LifecycleState::Starting){stop_requested_=true;requested_reason_=reason;return;}if(state_==LifecycleState::Idle){state_=LifecycleState::Stopped;return;}state_=LifecycleState::Stopping;}
 unwind(reason);std::scoped_lock lock(mutex_);state_=LifecycleState::Stopped;
}
void StartupSequence::unwind(ShutdownReason reason) noexcept{
 {std::scoped_lock lock(mutex_);state_=LifecycleState::Stopping;}
 for(;;){StartupStep step;{std::scoped_lock lock(mutex_);if(completed_.empty())break;step=completed_.back();completed_.pop_back();}
  try{runtime_.stop(step,reason);}catch(const std::exception& e){record_rollback_failure(step,e.what());}catch(...){record_rollback_failure(step,"unknown rollback exception");}}
}
void StartupSequence::record_rollback_failure(StartupStep step,std::string_view message) noexcept{RollbackFailure f{step,bounded_lifecycle_message(message)};{try{std::scoped_lock lock(mutex_);rollback_failures_.push_back(f);}catch(...){}}try{runtime_.rollback_diagnostic(f);}catch(...){}}
LifecycleState StartupSequence::state() const noexcept{std::scoped_lock lock(mutex_);return state_;}
std::vector<StartupStep> StartupSequence::completed_steps()const{std::scoped_lock lock(mutex_);return completed_;}
std::vector<RollbackFailure> StartupSequence::rollback_failures()const{std::scoped_lock lock(mutex_);return rollback_failures_;}
}