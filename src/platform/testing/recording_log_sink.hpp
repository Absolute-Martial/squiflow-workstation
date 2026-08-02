#pragma once

// A sink that keeps every line in memory.
//
// Used to assert what the logger produced without involving rotation, files,
// or a disk. It can also be told to fail, so the logger's failure counting is
// exercised rather than assumed.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "platform/log_sink.hpp"

namespace squiflow::platform::testing {

class RecordingLogSink final : public LogSink {
public:
    bool write_line(std::string_view line) override {
        if (accepting_) {
            lines_.emplace_back(line);
            return true;
        }
        ++refusals_;
        return false;
    }

    void flush() override { ++flushes_; }

    void set_accepting(bool accepting) { accepting_ = accepting; }

    const std::vector<std::string>& lines() const { return lines_; }
    std::uint64_t flushes() const { return flushes_; }
    std::uint64_t refusals() const { return refusals_; }

    bool contains(std::string_view fragment) const {
        for (const std::string& line : lines_) {
            if (line.find(fragment) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

private:
    std::vector<std::string> lines_;
    std::uint64_t flushes_ = 0;
    std::uint64_t refusals_ = 0;
    bool accepting_ = true;
};

}  // namespace squiflow::platform::testing
