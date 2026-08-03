#pragma once

// A bounded record of the last log events before a crash. push() performs no
// allocation and takes no lock. A per-slot publication sequence prevents a
// crash dump from printing a record while another thread is still writing it.

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>

#include "platform/log_record.hpp"

namespace squiflow::platform {

inline constexpr std::size_t kDefaultBreadcrumbCapacity = 32;
inline constexpr std::size_t kMaxBreadcrumbCapacity = 256;
inline constexpr std::size_t kBreadcrumbCategoryLength = 32;
inline constexpr std::size_t kBreadcrumbMessageLength = 120;

struct BreadcrumbEntry {
    LogLevel level{LogLevel::Info};
    std::int64_t timestamp_milliseconds{0};
    char category[kBreadcrumbCategoryLength]{};
    char message[kBreadcrumbMessageLength]{};
    bool valid{false};
};

class CrashBreadcrumb {
public:
    explicit CrashBreadcrumb(
        std::size_t capacity = kDefaultBreadcrumbCapacity);
    ~CrashBreadcrumb();

    CrashBreadcrumb(const CrashBreadcrumb&) = delete;
    CrashBreadcrumb& operator=(const CrashBreadcrumb&) = delete;
    CrashBreadcrumb(CrashBreadcrumb&&) = delete;
    CrashBreadcrumb& operator=(CrashBreadcrumb&&) = delete;

    void push(LogLevel level, std::string_view category,
              std::string_view message,
              std::int64_t timestamp_milliseconds) noexcept;

    // Writes complete records in oldest-first order. A record being changed
    // concurrently is skipped rather than emitted torn. Uses only write().
    std::size_t dump_to_fd(int fd) const noexcept;

    // Convenience path for startup code and tests; not signal-safe.
    bool dump_to_file(std::string_view path) const noexcept;

    std::size_t capacity() const noexcept { return capacity_; }
    std::uint64_t push_count() const noexcept;
    BreadcrumbEntry entry_at(std::size_t physical_index) const noexcept;

private:
    struct Slot;

    bool read_ticket(std::uint64_t ticket,
                     BreadcrumbEntry& destination) const noexcept;

    std::size_t capacity_;
    std::unique_ptr<Slot[]> slots_;
    std::atomic<std::uint64_t> next_{0};
};

}  // namespace squiflow::platform
