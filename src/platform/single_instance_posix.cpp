#include "platform/single_instance.hpp"
#include "platform/single_instance_name.hpp"
#include <cerrno>
#include <filesystem>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
namespace squiflow::platform {
namespace {
class PosixLock final:public SingleInstanceLock{
public:~PosixLock() override{release();}
InstanceAcquireResult acquire(const std::string& id,const std::string& dir) override{if(state_!=InstanceState::Idle)return {InstanceState::Failed,InstanceFault::LockWaitFailed,"The lock object is already in use.",false};auto names=make_single_instance_names(id,dir);if(!names.ok)return {InstanceState::Failed,InstanceFault::InvalidDataDirectory,names.error,false};std::error_code ec;if(!std::filesystem::is_directory(dir,ec))return {InstanceState::Failed,InstanceFault::InvalidDataDirectory,"The data directory is unavailable.",false};lock_path_=dir+"/.squiflow.instance.lock";activation_path_=dir+"/.squiflow.activation";fd_=::open(lock_path_.c_str(),O_RDWR|O_CREAT|O_CLOEXEC|O_NOFOLLOW,0600);if(fd_<0)return fail("Cannot open the instance lock.");if(::flock(fd_,LOCK_EX|LOCK_NB)==0){state_=InstanceState::Primary;const int a=::open(activation_path_.c_str(),O_RDWR|O_CREAT|O_TRUNC|O_CLOEXEC|O_NOFOLLOW,0600);if(a<0){release();return fail("Cannot create the activation signal.",InstanceFault::ActivationCreateFailed);}::close(a);return {state_,InstanceFault::None,"Primary instance acquired.",false};}if(errno!=EWOULDBLOCK&&errno!=EAGAIN){release();return fail("Cannot test the instance lock.",InstanceFault::LockWaitFailed);}::close(fd_);fd_=-1;const int a=::open(activation_path_.c_str(),O_WRONLY|O_APPEND|O_CLOEXEC|O_NOFOLLOW);if(a<0)return fail("Cannot signal the primary instance.",InstanceFault::ActivationSignalFailed);const char byte='1';const bool ok=::write(a,&byte,1)==1;::close(a);state_=ok?InstanceState::Secondary:InstanceState::Failed;return {state_,ok?InstanceFault::None:InstanceFault::ActivationSignalFailed,ok?"Existing instance notified.":"Activation signal failed.",false};}
bool take_activation_request() noexcept override{if(state_!=InstanceState::Primary)return false;const int a=::open(activation_path_.c_str(),O_RDWR|O_CLOEXEC|O_NOFOLLOW);if(a<0)return false;char byte{};const bool present=::read(a,&byte,1)==1;if(present)::ftruncate(a,0);::close(a);return present;}
InstanceState state()const noexcept override{return state_;}
void release()noexcept override{if(fd_>=0){::flock(fd_,LOCK_UN);::close(fd_);fd_=-1;}state_=InstanceState::Idle;}
private:InstanceAcquireResult fail(const char* m,InstanceFault f=InstanceFault::LockCreateFailed){state_=InstanceState::Failed;return {state_,errno==EACCES?InstanceFault::PermissionDenied:f,m,false};}int fd_{-1};InstanceState state_{InstanceState::Idle};std::string lock_path_,activation_path_;};
}
std::unique_ptr<SingleInstanceLock> make_single_instance_lock(){return std::make_unique<PosixLock>();}
}  // namespace squiflow::platform
