#pragma once

// Log storage in memory, with switches for the failures that matter.
//
// Rotation is judged by what it does when the disk misbehaves: a rename that
// fails because another process holds the file, a delete that is refused, a
// file whose size cannot be read, and an append that fails because the volume
// is full. Arranging those on a real disk is unreliable; arranging them here
// is a method call, so they are tested every single run.

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "platform/log_storage.hpp"

namespace squiflow::platform::testing {

class FakeLogStorage final : public LogStorage {
public:
    bool append(const std::string& name, std::string_view bytes) override {
        ++appends_;
        if (!accepting_writes_) {
            return false;
        }
        if (volume_capacity_bytes_.has_value()) {
            std::uint64_t used = 0;
            for (const auto& entry : files_) {
                used += static_cast<std::uint64_t>(entry.second.size());
            }
            if (used + bytes.size() > *volume_capacity_bytes_) {
                return false;
            }
        }
        files_[name].append(bytes);
        return true;
    }

    std::optional<std::uint64_t> size_of(const std::string& name) const override {
        if (unmeasurable_.count(name) != 0) {
            return std::nullopt;
        }
        const auto found = files_.find(name);
        if (found == files_.end()) {
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(found->second.size());
    }

    bool exists(const std::string& name) const override {
        return files_.count(name) != 0;
    }

    bool rename(const std::string& from, const std::string& to) override {
        ++renames_;
        if (locked_.count(from) != 0 || locked_.count(to) != 0) {
            return false;
        }
        const auto found = files_.find(from);
        if (found == files_.end()) {
            return false;
        }
        files_[to] = found->second;
        files_.erase(found);
        return true;
    }

    bool remove(const std::string& name) override {
        ++removals_;
        if (undeletable_.count(name) != 0) {
            return false;
        }
        files_.erase(name);
        return true;
    }

    void flush() override { ++flushes_; }

    // Test controls.
    void set_accepting_writes(bool accepting) { accepting_writes_ = accepting; }
    void set_volume_capacity(std::uint64_t bytes) {
        volume_capacity_bytes_ = bytes;
    }
    void lock_file(const std::string& name) { locked_.insert(name); }
    void make_undeletable(const std::string& name) { undeletable_.insert(name); }
    void make_unmeasurable(const std::string& name) {
        unmeasurable_.insert(name);
    }
    void put(const std::string& name, const std::string& contents) {
        files_[name] = contents;
    }

    // Test observation.
    std::string contents(const std::string& name) const {
        const auto found = files_.find(name);
        return found == files_.end() ? std::string() : found->second;
    }

    std::vector<std::string> names() const {
        std::vector<std::string> result;
        result.reserve(files_.size());
        for (const auto& entry : files_) {
            result.push_back(entry.first);
        }
        return result;
    }

    std::size_t file_count() const { return files_.size(); }

    std::uint64_t total_bytes() const {
        std::uint64_t total = 0;
        for (const auto& entry : files_) {
            total += static_cast<std::uint64_t>(entry.second.size());
        }
        return total;
    }

    std::uint64_t appends() const { return appends_; }
    std::uint64_t renames() const { return renames_; }
    std::uint64_t removals() const { return removals_; }
    std::uint64_t flushes() const { return flushes_; }

    std::size_t line_count(const std::string& name) const {
        const std::string text = contents(name);
        std::size_t lines = 0;
        for (const char character : text) {
            if (character == '\n') {
                ++lines;
            }
        }
        return lines;
    }

private:
    std::map<std::string, std::string> files_;
    std::set<std::string> locked_;
    std::set<std::string> undeletable_;
    std::set<std::string> unmeasurable_;
    std::optional<std::uint64_t> volume_capacity_bytes_;
    std::uint64_t appends_ = 0;
    std::uint64_t renames_ = 0;
    std::uint64_t removals_ = 0;
    std::uint64_t flushes_ = 0;
    bool accepting_writes_ = true;
};

}  // namespace squiflow::platform::testing
