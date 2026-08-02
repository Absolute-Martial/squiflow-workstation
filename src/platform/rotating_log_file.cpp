#include "platform/rotating_log_file.hpp"

#include <algorithm>
#include <string>

namespace squiflow::platform {
namespace {

constexpr char kTruncatedLineMarker[] = "...[truncated]";

}  // namespace

RotationPolicyCheck sanitise_rotation_policy(LogRotationPolicy requested) {
    RotationPolicyCheck check;
    check.policy = requested;
    std::string message;

    if (check.policy.max_file_bytes < kMinimumLogFileBytes) {
        check.policy.max_file_bytes = kMinimumLogFileBytes;
        message += "file size raised to the smallest useful value; ";
    } else if (check.policy.max_file_bytes > kMaximumLogFileBytes) {
        check.policy.max_file_bytes = kMaximumLogFileBytes;
        message += "file size lowered to the largest supported value; ";
    }

    if (check.policy.generations == 0) {
        check.policy.generations = 1;
        message += "at least one kept generation is required; ";
    } else if (check.policy.generations > kMaximumLogGenerations) {
        check.policy.generations = kMaximumLogGenerations;
        message += "kept generations lowered to the supported maximum; ";
    }

    // A budget smaller than two files could never hold a rotated file beside
    // the live one, which would make rotation pointless.
    const std::uint64_t smallest_useful_budget = check.policy.max_file_bytes * 2;
    if (check.policy.total_budget_bytes < smallest_useful_budget) {
        check.policy.total_budget_bytes = smallest_useful_budget;
        message += "total budget raised to hold two files; ";
    } else if (check.policy.total_budget_bytes > kMaximumLogBudgetBytes) {
        check.policy.total_budget_bytes = kMaximumLogBudgetBytes;
        message += "total budget lowered to the supported maximum; ";
    }

    if (!message.empty()) {
        check.adjusted = true;
        message.erase(message.size() - 2);
        check.message = message;
    }
    return check;
}

std::string generation_file_name(std::string_view base_name,
                                 std::uint8_t generation) {
    const std::string number = std::to_string(static_cast<int>(generation));
    const std::size_t dot = base_name.find_last_of('.');
    if (dot == std::string_view::npos || dot == 0) {
        return std::string(base_name) + "." + number;
    }
    return std::string(base_name.substr(0, dot)) + "." + number +
           std::string(base_name.substr(dot));
}

RotatingLogFile::RotatingLogFile(LogStorage& storage, LogRotationPolicy policy,
                                 std::string base_name)
    : storage_(storage),
      policy_(sanitise_rotation_policy(policy).policy),
      base_name_(std::move(base_name)) {}

std::uint64_t RotatingLogFile::size_of_or_zero(const std::string& name) const {
    const auto size = storage_.size_of(name);
    if (size.has_value()) {
        return *size;
    }
    // Unmeasurable but present: treat it as a full file so that the budget
    // errs towards deleting rather than towards overflowing.
    return storage_.exists(name) ? policy_.max_file_bytes : 0;
}

std::uint64_t RotatingLogFile::occupied_bytes() const {
    std::uint64_t total = size_of_or_zero(base_name_);
    for (std::uint8_t generation = 1; generation <= policy_.generations;
         ++generation) {
        total += size_of_or_zero(generation_file_name(base_name_, generation));
    }
    return total;
}

bool RotatingLogFile::rotate() {
    const std::string oldest =
        generation_file_name(base_name_, policy_.generations);
    if (storage_.exists(oldest) && !storage_.remove(oldest)) {
        ++counters_.storage_failures;
    }

    for (std::uint8_t generation = policy_.generations; generation > 1;
         --generation) {
        const std::string from = generation_file_name(base_name_,
                                                      static_cast<std::uint8_t>(
                                                          generation - 1));
        if (!storage_.exists(from)) {
            continue;
        }
        if (!storage_.rename(from, generation_file_name(base_name_, generation))) {
            ++counters_.storage_failures;
        }
    }

    if (!storage_.exists(base_name_)) {
        ++counters_.rotations;
        return true;
    }

    if (storage_.rename(base_name_, generation_file_name(base_name_, 1))) {
        ++counters_.rotations;
        return true;
    }

    // The live file could not be moved aside, usually because something else
    // holds it open. Letting it keep growing would break the promise this
    // class exists to keep, so it is discarded and the loss is counted.
    ++counters_.storage_failures;
    if (storage_.remove(base_name_)) {
        ++counters_.discarded_files;
        ++counters_.rotations;
        return true;
    }
    return false;
}

void RotatingLogFile::enforce_budget() {
    for (std::uint8_t generation = policy_.generations; generation >= 1;
         --generation) {
        if (occupied_bytes() <= policy_.total_budget_bytes) {
            return;
        }
        const std::string name = generation_file_name(base_name_, generation);
        if (!storage_.exists(name)) {
            continue;
        }
        if (storage_.remove(name)) {
            ++counters_.discarded_files;
        } else {
            ++counters_.storage_failures;
        }
    }

    if (occupied_bytes() > policy_.total_budget_bytes &&
        storage_.exists(base_name_)) {
        if (storage_.remove(base_name_)) {
            ++counters_.discarded_files;
        } else {
            ++counters_.storage_failures;
        }
    }
}

bool RotatingLogFile::write_line(std::string_view line) {
    std::string payload(line);
    // One line must always fit inside one file, otherwise rotation would spin
    // forever trying to make room that can never be enough.
    const std::size_t room =
        static_cast<std::size_t>(policy_.max_file_bytes) - 1;
    if (payload.size() > room) {
        const std::size_t marker_length = sizeof(kTruncatedLineMarker) - 1;
        payload.resize(room > marker_length ? room - marker_length : 0);
        payload += kTruncatedLineMarker;
        ++counters_.truncated_lines;
    }
    payload.push_back('\n');

    const std::uint64_t current = size_of_or_zero(base_name_);
    if (current + payload.size() > policy_.max_file_bytes) {
        if (!rotate()) {
            ++counters_.storage_failures;
            return false;
        }
        enforce_budget();
    }

    if (!storage_.append(base_name_, payload)) {
        ++counters_.storage_failures;
        return false;
    }

    ++counters_.lines_written;
    counters_.bytes_written += payload.size();

    if (occupied_bytes() > policy_.total_budget_bytes) {
        enforce_budget();
    }
    return true;
}

void RotatingLogFile::flush() { storage_.flush(); }

}  // namespace squiflow::platform
