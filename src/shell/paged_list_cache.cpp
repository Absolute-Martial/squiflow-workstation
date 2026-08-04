#include "shell/paged_list_cache.hpp"
#include <stdexcept>
namespace squiflow::shell {
PageBuffer::PageBuffer():arena_(storage_.data(),storage_.size()),rows_(&arena_){}
void PageBuffer::append(const RowInput&r){rows_.emplace_back(r,&arena_);}
std::uint64_t PagedListCache::begin_query(){pages_.clear();selected_.reset();return++generation_;}
bool PagedListCache::contains(std::string_view id)const{for(const auto&p:pages_)for(const auto&r:p->rows())if(r.id==id)return true;return false;}
bool PagedListCache::apply(std::uint64_t g,std::vector<RowInput> rows){if(g!=generation_)return false;if(rows.size()>kMaximumPageRows)throw std::length_error("page too large");auto page=std::make_unique<PageBuffer>();for(const auto&r:rows){if(r.id.empty()||r.id.size()>128||r.title.size()>256||r.subtitle.size()>256||contains(r.id))throw std::invalid_argument("invalid or duplicate row");page->append(r);}pages_.push_back(std::move(page));if(pages_.size()>kMaximumPages){pages_.pop_front();if(selected_&&!contains(*selected_))selected_.reset();}return true;}
void PagedListCache::select(std::string_view id){selected_=contains(id)?std::optional<std::string>{id}:std::nullopt;}
std::size_t PagedListCache::row_count()const noexcept{std::size_t count=0;for(const auto&p:pages_)count+=p->rows().size();return count;}
std::vector<RowInput> PagedListCache::snapshot()const{std::vector<RowInput> result;result.reserve(row_count());for(const auto& page:pages_)for(const auto& row:page->rows())result.push_back({std::string(row.id),std::string(row.title),std::string(row.subtitle)});return result;}
}
