#include "shell/surface_lifecycle.hpp"
namespace squiflow::shell {
app::StepResult SurfaceLifecycle::start_shell(bool ok,std::string error){if(state_!=SurfaceState::Empty)return{app::StepDisposition::Failed,"shell started out of order"};if(!ok){state_=SurfaceState::Failed;return{app::StepDisposition::Failed,app::bounded_lifecycle_message(error)};}state_=SurfaceState::ShellReady;return{};}
app::StepResult SurfaceLifecycle::start_window(bool ok,std::string error){if(state_!=SurfaceState::ShellReady)return{app::StepDisposition::Failed,"window started before shell"};if(!ok){state_=SurfaceState::Failed;return{app::StepDisposition::Failed,app::bounded_lifecycle_message(error)};}state_=SurfaceState::WindowReady;return{};}
void SurfaceLifecycle::request_shutdown(){if(state_!=SurfaceState::WindowReady)return;state_=SurfaceState::ShutdownRequested;if(request_)request_(app::ShutdownReason::WindowClosed);}
void SurfaceLifecycle::stop_window()noexcept{if(state_==SurfaceState::WindowReady||state_==SurfaceState::ShutdownRequested)state_=SurfaceState::ShellReady;}
void SurfaceLifecycle::stop_shell()noexcept{if(state_==SurfaceState::ShellReady||state_==SurfaceState::Failed)state_=SurfaceState::Stopped;}
}
