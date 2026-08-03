#pragma once
#include <mutex>
namespace squiflow::app {
class ApplicationSurface {public:virtual ~ApplicationSurface()=default;virtual void show()=0;virtual void activate()=0;virtual void close()=0;};
class ActivationController final {public:void attach(ApplicationSurface&);void request();void stop() noexcept;bool pending()const noexcept;private:mutable std::mutex mutex_;ApplicationSurface* surface_{nullptr};bool pending_{false};bool stopping_{false};};
}