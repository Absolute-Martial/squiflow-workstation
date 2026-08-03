#pragma once
#include "app/contracts/domain_error.hpp"
#include "app/contracts/result.hpp"
#include <any>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <stdexcept>
#include <typeindex>
#include <utility>
#include <vector>
namespace squiflow::app {
struct EventMetadata{std::string event_id,correlation_id,causation_id;std::int64_t occurred_at_ms{0};};
class DomainEventBus final{public:template<class E>using Handler=std::function<Result<void,DomainError>(const EventMetadata&,const E&)>;template<class E>void subscribe(Handler<E> h){if(frozen_)throw std::logic_error("event subscriptions frozen");handlers_[typeid(E)].push_back([fn=std::move(h)](const EventMetadata&m,const std::any&e){return fn(m,std::any_cast<const E&>(e));});}void freeze()noexcept{frozen_=true;}template<class E>Result<void,DomainError> publish(const EventMetadata&m,const E&e){if(!frozen_)return Result<void,DomainError>::failure({DomainErrorCode::Conflict,"event_bus.not_frozen",{}});if(publishing_)return Result<void,DomainError>::failure({DomainErrorCode::Conflict,"event_bus.reentrant_publish",{}});publishing_=true;struct Reset{bool&v;~Reset(){v=false;}}reset{publishing_};auto it=handlers_.find(typeid(E));if(it==handlers_.end())return Result<void,DomainError>::success();for(auto&h:it->second){auto result=h(m,e);if(!result)return result;}return Result<void,DomainError>::success();}private:using Erased=std::function<Result<void,DomainError>(const EventMetadata&,const std::any&)>;std::map<std::type_index,std::vector<Erased>> handlers_;bool frozen_{false},publishing_{false};};
class PostCommitEvents final{public:using Dispatch=std::function<Result<void,DomainError>()>;void add(Dispatch d){if(committed_)throw std::logic_error("event batch already committed");dispatch_.push_back(std::move(d));}Result<void,DomainError> commit(){if(committed_)return Result<void,DomainError>::failure({DomainErrorCode::Conflict,"event_batch.already_committed",{}});committed_=true;for(auto&d:dispatch_){auto result=d();if(!result)return result;}dispatch_.clear();return Result<void,DomainError>::success();}void rollback()noexcept{dispatch_.clear();committed_=true;}private:std::vector<Dispatch>dispatch_;bool committed_{false};};
}
