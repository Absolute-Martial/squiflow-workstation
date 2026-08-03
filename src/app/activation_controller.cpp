#include "app/activation_controller.hpp"
namespace squiflow::app {
void ActivationController::attach(ApplicationSurface& s){bool fire=false;{std::scoped_lock l(mutex_);if(stopping_)return;surface_=&s;fire=pending_;pending_=false;}if(fire)s.activate();}
void ActivationController::request(){ApplicationSurface* s=nullptr;{std::scoped_lock l(mutex_);if(stopping_)return;if(surface_==nullptr){pending_=true;return;}s=surface_;}s->activate();}
void ActivationController::stop()noexcept{std::scoped_lock l(mutex_);stopping_=true;surface_=nullptr;pending_=false;}
bool ActivationController::pending()const noexcept{std::scoped_lock l(mutex_);return pending_;}
}