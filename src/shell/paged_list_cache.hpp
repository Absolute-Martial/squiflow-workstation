#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <memory_resource>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
namespace squiflow::shell {
struct RowInput{std::string id,title,subtitle;};struct RowSnapshot{std::pmr::string id,title,subtitle;RowSnapshot(const RowInput&r,std::pmr::memory_resource*m):id(r.id,m),title(r.title,m),subtitle(r.subtitle,m){}};
class PageBuffer final{public:static constexpr std::size_t kInlineBytes=64*1024;PageBuffer();void append(const RowInput&);const std::pmr::vector<RowSnapshot>& rows()const noexcept{return rows_;}private:std::array<std::byte,kInlineBytes> storage_{};std::pmr::monotonic_buffer_resource arena_;std::pmr::vector<RowSnapshot> rows_;};
class PagedListCache final{public:static constexpr std::size_t kMaximumPages=3,kMaximumPageRows=100;std::uint64_t begin_query();bool apply(std::uint64_t,std::vector<RowInput>);void select(std::string_view);std::size_t row_count()const noexcept;std::size_t page_count()const noexcept{return pages_.size();}std::optional<std::string> selected()const{return selected_;}std::vector<RowInput> snapshot()const;private:bool contains(std::string_view)const;std::uint64_t generation_{0};std::deque<std::unique_ptr<PageBuffer>> pages_;std::optional<std::string> selected_;};
}
