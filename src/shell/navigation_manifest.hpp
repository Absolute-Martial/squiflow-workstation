#pragma once

#include "shell/list_bridge.hpp"
#include "shell/screen_registry.hpp"

#include <squiflow/protocol/module_id.hpp>

#include <string>
#include <string_view>
#include <vector>

namespace squiflow::shell {

// Lightweight, portable bridge identity shared by every primary module route.
// Phase 7.3 layers list behavior on top without changing route identity.
class RoutePresentationBridge final : public PresentationBridge {
  public:
    RoutePresentationBridge(std::string route_id, std::vector<ListColumn> columns);
    std::string_view route_id() const noexcept { return route_id_; }
    ListBridge& list() noexcept { return list_; }
    const ListBridge& list() const noexcept { return list_; }

  private:
    std::string route_id_;
    ListBridge list_;
};

ScreenRegistry make_navigation_manifest();

// Startup completeness check. A compiled module must either have at least one
// route or be explicitly named headless. Unknown, repeated and contradictory
// exceptions are rejected rather than weakening the check.
void require_navigation_complete(
    const ScreenRegistry& manifest,
    const std::vector<protocol::ModuleId>& registered_modules,
    const std::vector<protocol::ModuleId>& deliberately_headless = {});

}  // namespace squiflow::shell
