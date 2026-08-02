#pragma once

// An in-memory directory probe.
//
// The failures worth testing are the ones a developer machine will never
// produce on demand: a read-only program-data folder, a file sitting exactly
// where a directory belongs, a parent that refuses to answer, a creation that
// fails after the parent was made. This fake produces all of them in
// microseconds and without administrative rights.
//
// It lives beside the interface rather than under tests/ because the platform
// contract includes its fake: no interface here gets a second caller before it
// has one.

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "platform/directory_probe.hpp"

namespace squiflow::platform::testing {

class FakeDirectoryProbe final : public DirectoryProbe {
public:
    FakeDirectoryProbe() = default;

    // Arrangement.
    void add_directory(const std::string& path) { directories_.insert(path); }
    void add_file(const std::string& path) { files_.insert(path); }
    void make_undeterminable(const std::string& path) { unknown_.insert(path); }
    void deny_creation_under(const std::string& prefix) { no_create_.push_back(prefix); }
    void deny_writes_under(const std::string& prefix) { no_write_.push_back(prefix); }

    // Observation.
    const std::vector<std::string>& created() const { return created_; }
    std::size_t write_checks() const { return write_checks_; }
    bool has_directory(const std::string& path) const {
        return directories_.count(path) != 0;
    }

    DirectoryState inspect(const std::string& path) const override {
        if (path.empty()) {
            return DirectoryState::Unknown;
        }
        if (unknown_.count(path) != 0) {
            return DirectoryState::Unknown;
        }
        if (files_.count(path) != 0) {
            return DirectoryState::Blocked;
        }
        if (directories_.count(path) != 0) {
            return DirectoryState::Directory;
        }
        return DirectoryState::Missing;
    }

    bool create_directory_tree(const std::string& path) override {
        if (path.empty()) {
            return false;
        }
        created_.push_back(path);
        if (files_.count(path) != 0 || unknown_.count(path) != 0) {
            return false;
        }
        if (matches(no_create_, path)) {
            return false;
        }

        // Creating a tree creates its parents, exactly like the real one, so a
        // test that only arranges the root still sees a plausible filesystem.
        std::string walked;
        for (std::size_t index = 0; index < path.size(); ++index) {
            if (path[index] == '/' && index > 0) {
                walked = path.substr(0, index);
                if (!walked.empty() && files_.count(walked) == 0) {
                    directories_.insert(walked);
                }
            }
        }
        directories_.insert(path);
        return true;
    }

    bool can_write(const std::string& path) const override {
        ++write_checks_;
        if (inspect(path) != DirectoryState::Directory) {
            return false;
        }
        return !matches(no_write_, path);
    }

private:
    static bool matches(const std::vector<std::string>& prefixes,
                        const std::string& path) {
        for (const std::string& prefix : prefixes) {
            if (path.compare(0, prefix.size(), prefix) == 0) {
                return true;
            }
        }
        return false;
    }

    std::set<std::string> directories_{};
    std::set<std::string> files_{};
    std::set<std::string> unknown_{};
    std::vector<std::string> no_create_{};
    std::vector<std::string> no_write_{};
    std::vector<std::string> created_{};
    mutable std::size_t write_checks_{0};
};

}  // namespace squiflow::platform::testing
