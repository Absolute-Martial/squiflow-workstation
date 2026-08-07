#include "shell/shell_state.hpp"

#include <algorithm>
#include <utility>

namespace squiflow::shell {
namespace {

bool valid_text(std::string_view value, std::size_t maximum) noexcept {
    return !value.empty() && value.size() <= maximum &&
           std::none_of(value.begin(), value.end(), [](const char raw) {
               const auto character = static_cast<unsigned char>(raw);
               return character < static_cast<unsigned char>(' ') &&
                      character != static_cast<unsigned char>('\t');
           });
}

bool valid_route(std::string_view value) noexcept {
    if (value.empty() || value.size() > ShellState::kMaximumRouteBytes ||
        value.front() == '.' || value.back() == '.') {
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

bool ShellState::set_identity(std::string tenant_name, std::string user_name) {
    if (!valid_text(tenant_name, kMaximumIdentityBytes) ||
        !valid_text(user_name, kMaximumIdentityBytes)) {
        return false;
    }
    tenant_name_ = std::move(tenant_name);
    user_name_ = std::move(user_name);
    return true;
}

void ShellState::mark_dirty(bool dirty) noexcept {
    dirty_ = dirty;
    if (!dirty_) {
        pending_route_.reset();
    }
}

bool ShellState::request_route(std::string route_id) {
    if (!valid_route(route_id)) {
        return false;
    }
    if (!dirty_) {
        return true;
    }
    pending_route_ = std::move(route_id);
    return false;
}

std::optional<std::string> ShellState::resolve_unsaved(bool discard) {
    if (!pending_route_) {
        return std::nullopt;
    }
    if (!discard) {
        pending_route_.reset();
        return std::nullopt;
    }
    dirty_ = false;
    std::optional<std::string> result = std::move(pending_route_);
    pending_route_.reset();
    return result;
}

}  // namespace squiflow::shell
