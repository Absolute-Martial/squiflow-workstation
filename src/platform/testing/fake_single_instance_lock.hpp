#pragma once
#include "platform/single_instance.hpp"
namespace squiflow::platform::testing {
class FakeSingleInstanceLock final:public SingleInstanceLock{
public:InstanceAcquireResult next{InstanceState::Primary,InstanceFault::None,"acquired",false};
InstanceAcquireResult acquire(const std::string&,const std::string&)override{state_=next.state;return next;}
bool take_activation_request()noexcept override{const bool value=activation_;activation_=false;return value;}
InstanceState state()const noexcept override{return state_;}
void release()noexcept override{state_=InstanceState::Idle;}
void request_activation()noexcept{activation_=true;}
private:InstanceState state_{InstanceState::Idle};bool activation_{false};};
}  // namespace squiflow::platform::testing
