#pragma once

// A log file that cannot grow without limit, on a machine nobody administers.
//
// The shop counter has one disk, no operator, and no log shipper. A log that
// grows forever eventually competes with the shop's own database for space,
// and the failure arrives during a working day. So this sink obeys two limits
// at once:
//
//   A per-file limit, which decides when the current file is rotated.
//   A hard total budget across the current file and every kept generation,
//   which is enforced after every rotation by deleting the oldest files.
//
// The budget is a promise, not a target. If the only way to honour it is to
// discard the current file, the current file is discarded and the event is
// counted, because a log that fills the disk has stopped being a diagnostic
// and become the incident.
//
// Generations are numbered, never dated: the wall clock on a shop machine can
// move backwards, and rotation must not depend on it.

#include <cstdint>
#include <string>
#include <string_view>

#include "platform/log_sink.hpp"
#include "platform/log_storage.hpp"

namespace squiflow::platform {

inline constexpr std::uint64_t kMinimumLogFileBytes = 4096;
inline constexpr std::uint64_t kMaximumLogFileBytes = 64ULL * 1024 * 1024;
inline constexpr std::uint64_t kMaximumLogBudgetBytes = 1024ULL * 1024 * 1024;
inline constexpr std::uint8_t kMaximumLogGenerations = 20;
inline constexpr char kDefaultLogFileName[] = "squiflow.log";

struct LogRotationPolicy {
    std::uint64_t max_file_bytes = 1024ULL * 1024;
    std::uint8_t generations = 5;
    std::uint64_t total_budget_bytes = 8ULL * 1024 * 1024;
};

struct RotationPolicyCheck {
    bool adjusted = false;
    LogRotationPolicy policy;
    std::string message;
};

// Brings a requested policy inside the supportable range instead of refusing
// it. Configuration arrives from a file a human edited; an unusable number
// should cost a warning and a sane value, never a silent unlogged run.
RotationPolicyCheck sanitise_rotation_policy(LogRotationPolicy requested);

// squiflow.log -> squiflow.3.log
std::string generation_file_name(std::string_view base_name,
                                 std::uint8_t generation);

struct LogFileCounters {
    std::uint64_t lines_written = 0;
    std::uint64_t bytes_written = 0;
    std::uint64_t rotations = 0;
    std::uint64_t truncated_lines = 0;
    std::uint64_t storage_failures = 0;
    std::uint64_t discarded_files = 0;
};

class RotatingLogFile final : public LogSink {
public:
    RotatingLogFile(LogStorage& storage, LogRotationPolicy policy,
                    std::string base_name = kDefaultLogFileName);

    bool write_line(std::string_view line) override;
    void flush() override;

    const LogRotationPolicy& policy() const { return policy_; }
    const std::string& base_name() const { return base_name_; }
    const LogFileCounters& counters() const { return counters_; }

    // Total bytes currently held by the log family. Unmeasurable files count
    // as their last known size rather than as zero.
    std::uint64_t occupied_bytes() const;

private:
    bool rotate();
    void enforce_budget();
    std::uint64_t size_of_or_zero(const std::string& name) const;

    LogStorage& storage_;
    LogRotationPolicy policy_;
    std::string base_name_;
    LogFileCounters counters_;
};

}  // namespace squiflow::platform
