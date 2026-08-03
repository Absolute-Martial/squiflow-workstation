#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace squiflow::platform {
enum class InstanceState : std::uint8_t { Idle, Primary, Secondary, Failed };
enum class InstanceFault : std::uint8_t { None, InvalidApplicationId, InvalidDataDirectory, NameTooLong, LockCreateFailed, LockWaitFailed, ActivationCreateFailed, ActivationSignalFailed, PermissionDenied };
struct InstanceAcquireResult { InstanceState state{InstanceState::Idle}; InstanceFault fault{InstanceFault::None}; std::string message{}; bool recovered_abandoned_owner{false}; };
class SingleInstanceLock {
public:
 SingleInstanceLock(const SingleInstanceLock&)=delete; SingleInstanceLock& operator=(const SingleInstanceLock&)=delete; virtual ~SingleInstanceLock()=default;
 virtual InstanceAcquireResult acquire(const std::string& application_id,const std::string& data_directory)=0;
 virtual bool take_activation_request() noexcept=0;
 virtual InstanceState state() const noexcept=0;
 virtual void release() noexcept=0;
protected: SingleInstanceLock()=default;
};
std::unique_ptr<SingleInstanceLock> make_single_instance_lock();
}  // namespace squiflow::platform
