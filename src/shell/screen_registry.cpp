#include "shell/screen_registry.hpp"
#include <algorithm>
#include <stdexcept>
namespace squiflow::shell {
const ScreenContribution* ScreenRegistry::find(std::string_view id)const noexcept{auto it=std::find_if(screens_.begin(),screens_.end(),[id](const auto&s){return s.id==id;});return it==screens_.end()?nullptr:&*it;}
void ScreenRegistry::add(ScreenContribution s){if(screens_.size()>=kMaximumScreens)throw std::length_error("screen registry full");if(!protocol::is_valid(s.owner)||s.id.empty()||s.id.size()>64||s.title_key.empty()||s.component.empty()||!s.create_bridge)throw std::invalid_argument("invalid screen contribution");if(find(s.id))throw std::logic_error("duplicate screen id");screens_.push_back(std::move(s));}
std::vector<const ScreenContribution*> ScreenRegistry::visible(const ScreenAccess&a)const{if(!a.active||!a.permitted)throw std::invalid_argument("incomplete access policy");std::vector<const ScreenContribution*> out;for(const auto&s:screens_)if(a.active(s.owner)&&a.permitted(s.required_right))out.push_back(&s);std::sort(out.begin(),out.end(),[](auto*x,auto*y){return x->id<y->id;});return out;}
std::unique_ptr<PresentationBridge> ScreenRegistry::create(std::string_view id,const ScreenAccess&a)const{const auto*s=find(id);if(!s||!a.active||!a.permitted||!a.active(s->owner)||!a.permitted(s->required_right))return{};return s->create_bridge();}
}
