#pragma once
#include "app/contracts/result.hpp"
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <vector>
namespace squiflow::shell{struct ThumbnailKey{std::string file_id,content_hash;friend bool operator==(const ThumbnailKey&,const ThumbnailKey&)=default;};struct ThumbnailError{std::string message_key;};class ThumbnailCache{public:ThumbnailCache(std::filesystem::path,std::uint64_t cap=64*1024*1024,std::size_t entries=2048);app::Result<std::filesystem::path,ThumbnailError> store(const ThumbnailKey&,std::span<const std::uint8_t>);std::optional<std::filesystem::path> lookup(const ThumbnailKey&);std::size_t entry_count()const;std::uint64_t byte_count()const;static bool valid(const ThumbnailKey&)noexcept;private:struct Entry{ThumbnailKey key;std::filesystem::path path;std::uint64_t bytes,access;};void evict();std::filesystem::path root_;std::uint64_t cap_,bytes_{},clock_{};std::size_t entry_cap_;mutable std::mutex mutex_;std::vector<Entry> entries_;};}
