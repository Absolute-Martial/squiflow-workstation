#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include "engine/files/identity.hpp"
#include "engine/records/reference.hpp"
#include "engine/storage/store.hpp"

namespace squiflow::modules::files {

enum class Presence : std::uint8_t { Present, Missing };
enum class VolumeState : std::uint8_t { Online, Offline, Stale };

struct FileAsset {
    std::string id{};
    std::string content_hash{};
    std::int64_t size_bytes{0};
    std::string extension{};
    std::string media_type{};
    std::string predecessor_id{};
    bool forgotten{false};
    std::int64_t forgotten_at{0};
    std::string forgotten_by{};
    std::string forget_reason{};
    std::int64_t created_at{0};
    std::string created_by{};
};

struct FileLocation {
    std::string id{};
    engine::LocalFileIdentity identity{};
    std::string asset_id{};
    std::string path{};
    Presence presence{Presence::Present};
    std::int64_t modified_at{0};
    std::int64_t observed_at{0};
    std::int64_t scan_generation{0};
};

struct FileLink {
    std::string id{};
    std::string asset_id{};
    engine::Reference target{};
    std::string role{};
    std::string search_text{};
    std::int64_t linked_at{0};
    std::string linked_by{};
};

struct FileVolume {
    std::string id{};
    engine::DeviceId device{};
    std::string volume_id{};
    std::string label{};
    VolumeState state{VolumeState::Online};
    std::int64_t scan_generation{0};
    std::int64_t observed_at{0};
};

std::optional<std::string> normalize_sha256(const std::string& value);
bool same_identity(const FileLocation& left, const FileLocation& right) noexcept;
void validate(const FileAsset& asset);
void validate(const FileLocation& location);
void validate(const FileLink& link);
void validate(const FileVolume& volume);
engine::Row to_row(const FileAsset& asset);
engine::Row to_row(const FileLocation& location);
engine::Row to_row(const FileLink& link);
engine::Row to_row(const FileVolume& volume);
FileAsset asset_from_row(const engine::Row& row);
FileLocation location_from_row(const engine::Row& row);
FileLink link_from_row(const engine::Row& row);
FileVolume volume_from_row(const engine::Row& row);

}  // namespace squiflow::modules::files
