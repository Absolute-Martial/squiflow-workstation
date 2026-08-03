#pragma once

#include <memory>
#include <string>

namespace squiflow::platform {
class NetworkMonitor;

class QtNetworkStateAdapter {
public:
    explicit QtNetworkStateAdapter(NetworkMonitor& monitor);
    QtNetworkStateAdapter(const QtNetworkStateAdapter&) = delete;
    QtNetworkStateAdapter& operator=(const QtNetworkStateAdapter&) = delete;
    ~QtNetworkStateAdapter();
    bool backend_loaded() const noexcept;
    const std::string& backend_name() const noexcept;
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace squiflow::platform
