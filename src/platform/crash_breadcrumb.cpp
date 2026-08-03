#include "platform/crash_breadcrumb.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <climits>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <unistd.h>

namespace squiflow::platform {
namespace {

static_assert(std::atomic<std::uint64_t>::is_always_lock_free,
              "crash breadcrumbs require a lock-free sequence counter");

constexpr std::uint64_t writing_sequence(std::uint64_t ticket) noexcept {
    return ticket * 2U + 1U;
}

constexpr std::uint64_t complete_sequence(std::uint64_t ticket) noexcept {
    return ticket * 2U + 2U;
}

template <std::size_t Size>
void copy_bounded(std::array<std::atomic<char>, Size>& destination,
                  std::string_view source) noexcept {
    static_assert(Size > 0);
    const std::size_t count = std::min(source.size(), Size - 1U);
    for (std::size_t index = 0; index < count; ++index) {
        destination[index].store(source[index], std::memory_order_relaxed);
    }
    for (std::size_t index = count; index < Size; ++index) {
        destination[index].store('\0', std::memory_order_relaxed);
    }
}

template <std::size_t Size>
void load_bounded(const std::array<std::atomic<char>, Size>& source,
                  char (&destination)[Size]) noexcept {
    for (std::size_t index = 0; index < Size; ++index) {
        destination[index] = source[index].load(std::memory_order_relaxed);
    }
    destination[Size - 1U] = '\0';
}

struct FixedLine {
    std::array<char, 256> bytes{};
    std::size_t used{0};

    void append_char(char value) noexcept {
        if (used < bytes.size()) {
            bytes[used++] = value;
        }
    }

    void append(const char* value) noexcept {
        while (*value != '\0' && used < bytes.size()) {
            bytes[used++] = *value++;
        }
    }

    void append_integer(std::int64_t value) noexcept {
        std::array<char, 32> reverse{};
        std::size_t count = 0;
        const bool negative = value < 0;
        std::uint64_t magnitude = negative
            ? static_cast<std::uint64_t>(-(value + 1)) + 1U
            : static_cast<std::uint64_t>(value);
        do {
            reverse[count++] = static_cast<char>('0' + (magnitude % 10U));
            magnitude /= 10U;
        } while (magnitude != 0U && count < reverse.size());
        if (negative) {
            append_char('-');
        }
        while (count != 0U) {
            append_char(reverse[--count]);
        }
    }
};

FixedLine format_entry(const BreadcrumbEntry& entry) noexcept {
    FixedLine line;
    line.append_char('[');
    line.append_integer(entry.timestamp_milliseconds);
    line.append("] [");
    line.append(level_name(entry.level));
    line.append("] ");
    line.append(entry.category[0] == '\0' ? "general" : entry.category);
    line.append(": ");
    line.append(entry.message);
    line.append_char('\n');
    return line;
}

bool write_all(int fd, const char* data, std::size_t size,
               std::size_t& total) noexcept {
    while (size != 0U) {
        const ssize_t written = ::write(fd, data, size);
        if (written < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }
        if (written == 0) {
            return false;
        }
        const std::size_t count = static_cast<std::size_t>(written);
        data += count;
        size -= count;
        total += count;
    }
    return true;
}

}  // namespace

struct CrashBreadcrumb::Slot {
    std::atomic<std::uint64_t> sequence{0};
    std::atomic<std::uint8_t> level{static_cast<std::uint8_t>(LogLevel::Info)};
    std::atomic<std::int64_t> timestamp_milliseconds{0};
    std::array<std::atomic<char>, kBreadcrumbCategoryLength> category{};
    std::array<std::atomic<char>, kBreadcrumbMessageLength> message{};
    std::atomic<bool> valid{false};
};

static_assert(std::atomic<std::int64_t>::is_always_lock_free,
              "crash breadcrumbs require lock-free timestamps");
static_assert(std::atomic<char>::is_always_lock_free,
              "crash breadcrumbs require lock-free character publication");

CrashBreadcrumb::CrashBreadcrumb(std::size_t capacity)
    : capacity_(std::clamp(capacity, std::size_t{1},
                           kMaxBreadcrumbCapacity)),
      slots_(std::make_unique<Slot[]>(capacity_)) {}

CrashBreadcrumb::~CrashBreadcrumb() = default;

void CrashBreadcrumb::push(LogLevel level, std::string_view category,
                           std::string_view message,
                           std::int64_t timestamp_milliseconds) noexcept {
    const std::uint64_t ticket = next_.fetch_add(1, std::memory_order_relaxed);
    Slot& slot = slots_[static_cast<std::size_t>(ticket % capacity_)];
    slot.sequence.store(writing_sequence(ticket), std::memory_order_release);
    slot.valid.store(false, std::memory_order_relaxed);
    slot.level.store(static_cast<std::uint8_t>(level), std::memory_order_relaxed);
    slot.timestamp_milliseconds.store(timestamp_milliseconds,
                                      std::memory_order_relaxed);
    copy_bounded(slot.category, category);
    copy_bounded(slot.message, message);
    slot.valid.store(true, std::memory_order_relaxed);
    slot.sequence.store(complete_sequence(ticket), std::memory_order_release);
}

bool CrashBreadcrumb::read_ticket(
    std::uint64_t ticket, BreadcrumbEntry& destination) const noexcept {
    const Slot& slot = slots_[static_cast<std::size_t>(ticket % capacity_)];
    const std::uint64_t expected = complete_sequence(ticket);
    if (slot.sequence.load(std::memory_order_acquire) != expected) {
        return false;
    }
    destination.level = static_cast<LogLevel>(
        slot.level.load(std::memory_order_relaxed));
    destination.timestamp_milliseconds =
        slot.timestamp_milliseconds.load(std::memory_order_relaxed);
    load_bounded(slot.category, destination.category);
    load_bounded(slot.message, destination.message);
    destination.valid = slot.valid.load(std::memory_order_relaxed);
    return slot.sequence.load(std::memory_order_acquire) == expected &&
           destination.valid;
}

std::size_t CrashBreadcrumb::dump_to_fd(int fd) const noexcept {
    if (fd < 0) {
        return 0;
    }
    const std::uint64_t end = next_.load(std::memory_order_acquire);
    const std::uint64_t retained = std::min<std::uint64_t>(end, capacity_);
    const std::uint64_t begin = end - retained;
    std::size_t total = 0;
    for (std::uint64_t ticket = begin; ticket < end; ++ticket) {
        BreadcrumbEntry entry;
        if (!read_ticket(ticket, entry)) {
            continue;
        }
        const FixedLine line = format_entry(entry);
        if (!write_all(fd, line.bytes.data(), line.used, total)) {
            break;
        }
    }
    return total;
}

bool CrashBreadcrumb::dump_to_file(std::string_view path) const noexcept {
    if (path.empty() || path.size() >= static_cast<std::size_t>(PATH_MAX) ||
        path.find('\0') != std::string_view::npos) {
        return false;
    }
    std::array<char, PATH_MAX> terminated{};
    std::memcpy(terminated.data(), path.data(), path.size());
    const int fd = ::open(terminated.data(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (fd < 0) {
        return false;
    }
    static_cast<void>(dump_to_fd(fd));
    const bool closed = ::close(fd) == 0;
    return closed;
}

std::uint64_t CrashBreadcrumb::push_count() const noexcept {
    return next_.load(std::memory_order_acquire);
}

BreadcrumbEntry CrashBreadcrumb::entry_at(
    std::size_t physical_index) const noexcept {
    BreadcrumbEntry result;
    if (physical_index >= capacity_) {
        return result;
    }
    const Slot& slot = slots_[physical_index];
    const std::uint64_t before = slot.sequence.load(std::memory_order_acquire);
    if (before == 0U || (before & 1U) != 0U) {
        return result;
    }
    result.level = static_cast<LogLevel>(
        slot.level.load(std::memory_order_relaxed));
    result.timestamp_milliseconds =
        slot.timestamp_milliseconds.load(std::memory_order_relaxed);
    load_bounded(slot.category, result.category);
    load_bounded(slot.message, result.message);
    result.valid = slot.valid.load(std::memory_order_relaxed);
    if (slot.sequence.load(std::memory_order_acquire) != before) {
        return BreadcrumbEntry{};
    }
    return result;
}

}  // namespace squiflow::platform
