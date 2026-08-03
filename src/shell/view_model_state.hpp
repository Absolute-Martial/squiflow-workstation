#pragma once
#include <cstdint>
#include <string>
#include <variant>
#include <type_traits>
namespace squiflow::shell {
struct IdleState{};struct LoadingState{std::uint64_t generation{0};};struct ReadyState{bool stale{false};bool has_more{false};};struct OfflineState{bool has_cached_data{false};};struct FailedState{std::string message_key;};using ViewModelState=std::variant<IdleState,LoadingState,ReadyState,OfflineState,FailedState>;
enum class ViewStateKind{Idle,Loading,Ready,Offline,Failed};
inline ViewStateKind state_kind(const ViewModelState& s)noexcept{return std::visit([](const auto& value){using T=std::decay_t<decltype(value)>;if constexpr(std::is_same_v<T,IdleState>)return ViewStateKind::Idle;else if constexpr(std::is_same_v<T,LoadingState>)return ViewStateKind::Loading;else if constexpr(std::is_same_v<T,ReadyState>)return ViewStateKind::Ready;else if constexpr(std::is_same_v<T,OfflineState>)return ViewStateKind::Offline;else return ViewStateKind::Failed;},s);}
}
