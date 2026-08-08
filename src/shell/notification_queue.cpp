#include "shell/notification_queue.hpp"

#include <algorithm>
#include <utility>

namespace squiflow::shell {
namespace {

bool valid_key(std::string_view value, std::size_t maximum) noexcept {
    if (value.empty() || value.size() > maximum) {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](const char raw) {
        const auto character = static_cast<unsigned char>(raw);
        return (character >= static_cast<unsigned char>('a') &&
                character <= static_cast<unsigned char>('z')) ||
               (character >= static_cast<unsigned char>('0') &&
                character <= static_cast<unsigned char>('9')) ||
               character == static_cast<unsigned char>('.') ||
               character == static_cast<unsigned char>('_') ||
               character == static_cast<unsigned char>('-');
    });
}

}  // namespace

bool NotificationQueue::push(std::string deduplication_key,
                             std::string message_key, std::string detail,
                             NotificationSeverity severity) {
    if (!valid_key(deduplication_key, kMaximumKeyBytes) ||
        !valid_key(message_key, kMaximumKeyBytes) ||
        detail.size() > kMaximumDetailBytes) {
        return false;
    }
    const auto existing = std::find_if(
        items_.begin(), items_.end(), [&](const ShellNotification& item) {
            return item.deduplication_key == deduplication_key;
        });
    if (existing != items_.end()) {
        existing->message_key = std::move(message_key);
        existing->detail = std::move(detail);
        existing->severity = severity;
        if (existing->occurrences < UINT32_MAX) {
            ++existing->occurrences;
        }
        ShellNotification updated = std::move(*existing);
        items_.erase(existing);
        items_.push_back(std::move(updated));
        return true;
    }
    if (items_.size() == kMaximumVisible) {
        items_.pop_front();
    }
    ++next_id_;
    items_.push_back({"notification." + std::to_string(next_id_),
                      std::move(deduplication_key), std::move(message_key),
                      std::move(detail), severity, 1});
    return true;
}

bool NotificationQueue::dismiss(std::string_view id) noexcept {
    const auto item = std::find_if(items_.begin(), items_.end(),
                                   [id](const ShellNotification& value) {
                                       return value.id == id;
                                   });
    if (item == items_.end()) {
        return false;
    }
    items_.erase(item);
    return true;
}

void NotificationQueue::clear() noexcept {
    items_.clear();
}

}  // namespace squiflow::shell
