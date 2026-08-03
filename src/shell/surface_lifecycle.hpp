#pragma once
#include "app/startup.hpp"
#include <functional>
#include <string>
namespace squiflow::shell {
enum class SurfaceState{Empty,ShellReady,WindowReady,ShutdownRequested,Stopped,Failed};
class SurfaceLifecycle final{public:using ShutdownRequest=std::function<void(squiflow::app::ShutdownReason)>;explicit SurfaceLifecycle(ShutdownRequest request):request_(std::move(request)){}app::StepResult start_shell(bool engine_ready,std::string error={});app::StepResult start_window(bool root_ready,std::string error={});void request_shutdown();void stop_window()noexcept;void stop_shell()noexcept;SurfaceState state()const noexcept{return state_;}private:ShutdownRequest request_;SurfaceState state_{SurfaceState::Empty};};
}
