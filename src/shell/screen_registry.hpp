#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include <squiflow/protocol/module_id.hpp>
namespace squiflow::shell {
class PresentationBridge{public:virtual~PresentationBridge()=default;};using BridgeFactory=std::function<std::unique_ptr<PresentationBridge>()>;
struct ScreenContribution{protocol::ModuleId owner{};std::string id,title_key,icon,component,group;std::uint32_t required_right{0};BridgeFactory create_bridge;};
struct ScreenAccess{std::function<bool(protocol::ModuleId)> active;std::function<bool(std::uint32_t)> permitted;};
class ScreenRegistry final{public:static constexpr std::size_t kMaximumScreens=128;void add(ScreenContribution);std::vector<const ScreenContribution*> visible(const ScreenAccess&)const;std::unique_ptr<PresentationBridge> create(std::string_view,const ScreenAccess&)const;private:const ScreenContribution* find(std::string_view)const noexcept;std::vector<ScreenContribution> screens_;};
}
