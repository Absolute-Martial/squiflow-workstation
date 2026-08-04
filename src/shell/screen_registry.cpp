#include "shell/screen_registry.hpp"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace squiflow::shell {
namespace {

bool safe_identifier(std::string_view value, std::size_t maximum) noexcept {
    if (value.empty() || value.size() > maximum || value.front() == '.' ||
        value.back() == '.') {
        return false;
    }
    return std::all_of(value.begin(), value.end(), [](char raw) {
        const auto c = static_cast<unsigned char>(raw);
        return (c >= static_cast<unsigned char>('a') &&
                c <= static_cast<unsigned char>('z')) ||
               (c >= static_cast<unsigned char>('0') &&
                c <= static_cast<unsigned char>('9')) ||
               c == static_cast<unsigned char>('.') ||
               c == static_cast<unsigned char>('_') ||
               c == static_cast<unsigned char>('-');
    });
}

bool safe_key(std::string_view value) noexcept {
    return safe_identifier(value, ScreenRegistry::kMaximumTextKeyBytes);
}

bool safe_icon(std::string_view value) noexcept {
    return value.empty() || safe_identifier(value, ScreenRegistry::kMaximumStableIdBytes);
}

bool safe_component(std::string_view value) noexcept {
    constexpr std::string_view prefix{"qrc:/"};
    constexpr std::string_view suffix{".qml"};
    return value.size() > prefix.size() + suffix.size() &&
           value.size() <= ScreenRegistry::kMaximumComponentBytes &&
           value.starts_with(prefix) && value.ends_with(suffix) &&
           value.find("..") == std::string_view::npos &&
           std::none_of(value.begin(), value.end(), [](char raw) {
               return static_cast<unsigned char>(raw) < static_cast<unsigned char>(' ');
           });
}

}  // namespace

bool NavigationAccess::module_registered(protocol::ModuleId module) const noexcept {
    if (!protocol::is_valid(module)) {
        return false;
    }
    return registered[static_cast<std::size_t>(module)];
}

bool NavigationAccess::allows(const ScreenContribution& screen) const noexcept {
    if (!protocol::is_valid(screen.owner) || !module_registered(screen.owner) ||
        !activation.is_active(screen.owner)) {
        return false;
    }
    return !screen.required_right ||
           (protocol::is_valid(*screen.required_right) && rights.has(*screen.required_right));
}

NavigationAccess make_navigation_access(
    const protocol::Activation& activation,
    const engine::RightsSet& rights,
    const std::vector<protocol::ModuleId>& registered,
    std::uint64_t session_generation,
    std::uint64_t navigation_revision) {
    NavigationAccess access;
    access.activation = activation;
    access.rights = rights;
    access.session_generation = session_generation;
    access.navigation_revision = navigation_revision;
    for (const protocol::ModuleId module : registered) {
        if (!protocol::is_valid(module)) {
            throw std::invalid_argument("navigation snapshot contains an invalid module");
        }
        const std::size_t index = static_cast<std::size_t>(module);
        if (access.registered[index]) {
            throw std::invalid_argument("navigation snapshot repeats a module");
        }
        access.registered[index] = true;
    }
    return access;
}

const ScreenContribution* ScreenRegistry::find(std::string_view id) const noexcept {
    const auto found = std::find_if(screens_.begin(), screens_.end(),
                                    [id](const ScreenContribution& screen) {
                                        return screen.id == id;
                                    });
    return found == screens_.end() ? nullptr : &*found;
}

void ScreenRegistry::add(ScreenContribution screen) {
    if (screens_.size() >= kMaximumScreens) {
        throw std::length_error("screen registry full");
    }
    if (!protocol::is_valid(screen.owner) ||
        !safe_identifier(screen.id, kMaximumStableIdBytes) ||
        !safe_key(screen.title_key) || !safe_icon(screen.icon_name) ||
        !safe_component(screen.component_url) || !safe_key(screen.group_key) ||
        !screen.create_bridge) {
        throw std::invalid_argument("invalid screen contribution");
    }
    if (screen.required_right) {
        if (!protocol::is_valid(*screen.required_right)) {
            throw std::invalid_argument("screen requires an invalid right");
        }
        if (protocol::right_module(*screen.required_right) != screen.owner) {
            throw std::invalid_argument("screen right belongs to another module");
        }
    }
    if (find(screen.id) != nullptr) {
        throw std::logic_error("duplicate screen id");
    }
    screens_.push_back(std::move(screen));
}

std::vector<const ScreenContribution*> ScreenRegistry::visible(
    const NavigationAccess& access) const {
    std::vector<const ScreenContribution*> result;
    result.reserve(screens_.size());
    for (const ScreenContribution& screen : screens_) {
        if (access.allows(screen)) {
            result.push_back(&screen);
        }
    }
    std::sort(result.begin(), result.end(),
              [](const ScreenContribution* left, const ScreenContribution* right) {
                  return std::tie(left->group_rank, left->screen_rank, left->id) <
                         std::tie(right->group_rank, right->screen_rank, right->id);
              });
    return result;
}

std::unique_ptr<PresentationBridge> ScreenRegistry::create(
    std::string_view id, const NavigationAccess& access) const {
    const ScreenContribution* screen = find(id);
    if (screen == nullptr || !access.allows(*screen)) {
        return {};
    }
    return screen->create_bridge();
}

}  // namespace squiflow::shell
