#pragma once

#include <cstddef>
#include <cstdint>
#include <deque>
#include <string>
#include <string_view>

namespace squiflow::shell {

enum class NotificationSeverity : std::uint8_t { Information, Success, Warning, Error };

struct ShellNotification final {
    std::string id{};
    std::string deduplication_key{};
    std::string message_key{};
    std::string detail{};
    NotificationSeverity severity{NotificationSeverity::Information};
    std::uint32_t occurrences{1};

    friend bool operator==(const ShellNotification&, const ShellNotification&) = default;
};

class NotificationQueue final {
  public:
    static constexpr std::size_t kMaximumVisible = 5;
    static constexpr std::size_t kMaximumKeyBytes = 128;
    static constexpr std::size_t kMaximumDetailBytes = 512;

    bool push(std::string deduplication_key, std::string message_key,
              std::string detail, NotificationSeverity severity);
    bool dismiss(std::string_view id) noexcept;
    void clear() noexcept;
    const std::deque<ShellNotification>& items() const noexcept { return items_; }

  private:
    std::deque<ShellNotification> items_{};
    std::uint64_t next_id_{0};
};

}  // namespace squiflow::shell
