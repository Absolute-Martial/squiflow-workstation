#include "platform/local_directory_probe.hpp"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

namespace squiflow::platform {
namespace {

// Two runs of the same process, and two processes started in the same tick,
// must not pick the same probe file name; a collision would make one of them
// report a directory as unwritable for no reason.
std::string unique_probe_name() {
    static std::atomic<unsigned long long> sequence{0};
    const unsigned long long ordinal = sequence.fetch_add(1) + 1;
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const auto ticks = static_cast<unsigned long long>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    return ".squiflow-write-probe-" + std::to_string(ticks) + "-"
           + std::to_string(ordinal);
}

}  // namespace

DirectoryState LocalDirectoryProbe::inspect(const std::string& path) const {
    if (path.empty()) {
        return DirectoryState::Unknown;
    }

    std::error_code error;
    const std::filesystem::file_status status =
        std::filesystem::symlink_status(std::filesystem::path(path), error);
    if (error) {
        // Not found is a real answer. Anything else means the question could
        // not be asked, which is a different thing and must not be treated as
        // an empty spot to create into.
        if (status.type() == std::filesystem::file_type::not_found) {
            return DirectoryState::Missing;
        }
        return DirectoryState::Unknown;
    }

    switch (status.type()) {
    case std::filesystem::file_type::not_found:
        return DirectoryState::Missing;
    case std::filesystem::file_type::directory:
        return DirectoryState::Directory;
    case std::filesystem::file_type::symlink: {
        // A symlink is followed once, but only to decide directory or not. A
        // dangling link is Blocked rather than Missing: something is already
        // sitting on the name.
        std::error_code target_error;
        const std::filesystem::file_status target =
            std::filesystem::status(std::filesystem::path(path), target_error);
        if (target_error) {
            return DirectoryState::Blocked;
        }
        return target.type() == std::filesystem::file_type::directory
                   ? DirectoryState::Directory
                   : DirectoryState::Blocked;
    }
    case std::filesystem::file_type::none:
    case std::filesystem::file_type::unknown:
        return DirectoryState::Unknown;
    default:
        return DirectoryState::Blocked;
    }
}

bool LocalDirectoryProbe::create_directory_tree(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code error;
    std::filesystem::create_directories(std::filesystem::path(path), error);

    // create_directories reports false with no error when the directory was
    // already there, so the result that matters is the state afterwards, not
    // the return value.
    return inspect(path) == DirectoryState::Directory;
}

bool LocalDirectoryProbe::can_write(const std::string& path) const {
    if (inspect(path) != DirectoryState::Directory) {
        return false;
    }

    const std::filesystem::path probe =
        std::filesystem::path(path) / unique_probe_name();

    {
        std::ofstream handle(probe, std::ios::binary | std::ios::trunc);
        if (!handle.is_open()) {
            return false;
        }
        handle << "squiflow";
        handle.flush();
        if (!handle.good()) {
            handle.close();
            std::error_code discard;
            std::filesystem::remove(probe, discard);
            return false;
        }
    }

    std::error_code remove_error;
    const bool removed = std::filesystem::remove(probe, remove_error);

    // A directory that accepts a file but refuses to give it back is not a
    // place to keep a shop's records.
    return removed && !remove_error;
}

}  // namespace squiflow::platform
