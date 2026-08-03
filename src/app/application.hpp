#pragma once
#include "app/startup_sequence.hpp"
namespace squiflow::app {
class Application final {public:explicit Application(StartupRuntime& runtime):sequence_(runtime){}~Application(){sequence_.shutdown(ShutdownReason::NormalExit);}StartupResult start(){return sequence_.start();}void shutdown(ShutdownReason reason)noexcept{sequence_.shutdown(reason);}LifecycleState state()const noexcept{return sequence_.state();}private:StartupSequence sequence_;};
}
