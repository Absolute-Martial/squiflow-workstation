#include "platform/local_log_storage.hpp"

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace squiflow::platform {
namespace {

constexpr std::size_t kMaxLogFileNameLength = 64;

}  // namespace

bool is_plain_file_name(std::string_view name) {
    if (name.empty() || name.size() > kMaxLogFileNameLength) {
        return false;
    }
    if (name == "." || name == "..") {
        return false;
    }
    for (const char character : name) {
        const auto raw = static_cast<unsigned char>(character);
        if (raw < 0x20 || raw == 0x7F) {
            return false;
        }
        if (character == '/' || character == '\\' || character == ':' ||
            character == '*' || character == '?' || character == '"' ||
            character == '<' || character == '>' || character == '|') {
            return false;
        }
    }
    return true;
}

LocalLogStorage::LocalLogStorage(std::string directory)
    : directory_(std::move(directory)) {}

std::string LocalLogStorage::full_path(const std::string& name) const {
    if (directory_.empty()) {
        return name;
    }
    if (directory_.back() == '/' || directory_.back() == '\\') {
        return directory_ + name;
    }
    return directory_ + "/" + name;
}

bool LocalLogStorage::append(const std::string& name, std::string_view bytes) {
    if (!is_plain_file_name(name)) {
        return false;
    }
    std::ofstream stream(full_path(name),
                         std::ios::out | std::ios::app | std::ios::binary);
    if (!stream.is_open()) {
        return false;
    }
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.flush();
    // Checked after the flush: a full disk usually reports itself here rather
    // than at the write, and a silently lost line is worse than no line.
    return stream.good();
}

std::optional<std::uint64_t> LocalLogStorage::size_of(
    const std::string& name) const {
    if (!is_plain_file_name(name)) {
        return std::nullopt;
    }
    std::error_code error;
    const auto size = std::filesystem::file_size(full_path(name), error);
    if (error) {
        return std::nullopt;
    }
    return static_cast<std::uint64_t>(size);
}

bool LocalLogStorage::exists(const std::string& name) const {
    if (!is_plain_file_name(name)) {
        return false;
    }
    std::error_code error;
    const auto status = std::filesystem::status(full_path(name), error);
    if (error) {
        return false;
    }
    return std::filesystem::is_regular_file(status);
}

bool LocalLogStorage::rename(const std::string& from, const std::string& to) {
    if (!is_plain_file_name(from) || !is_plain_file_name(to)) {
        return false;
    }
    std::error_code error;
    std::filesystem::rename(full_path(from), full_path(to), error);
    return !error;
}

bool LocalLogStorage::remove(const std::string& name) {
    if (!is_plain_file_name(name)) {
        return false;
    }
    std::error_code error;
    std::filesystem::remove(full_path(name), error);
    if (error) {
        return false;
    }
    return !exists(name);
}

void LocalLogStorage::flush() {
    // Each append opens, writes, flushes, and closes, so there is nothing held
    // back here. The method exists because the interface promises it and a
    // buffered implementation would need it.
}

}  // namespace squiflow::platform
