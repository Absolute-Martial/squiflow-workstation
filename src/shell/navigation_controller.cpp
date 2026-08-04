#include "shell/navigation_controller.hpp"

#include <algorithm>
#include <exception>
#include <utility>

namespace squiflow::shell {
namespace {

NavigationRow row_from(const ScreenContribution& screen) {
    return {screen.owner,
            screen.id,
            screen.title_key,
            screen.icon_name,
            screen.component_url,
            screen.group_key,
            screen.group_rank,
            screen.screen_rank,
            false};
}

}  // namespace

NavigationController::NavigationController(const ScreenRegistry& registry) noexcept
    : registry_(registry) {}

NavigationError NavigationController::error(NavigationErrorCode code,
                                             std::string_view route_id,
                                             std::string message_key) {
    return {code, std::string(route_id), std::move(message_key)};
}

std::uint64_t NavigationController::session_generation() const noexcept {
    return has_access_ ? access_.session_generation : 0;
}

std::uint64_t NavigationController::navigation_revision() const noexcept {
    return has_access_ ? access_.navigation_revision : 0;
}

bool NavigationController::route_exists(std::string_view stable_id) const noexcept {
    return std::any_of(registry_.all().begin(), registry_.all().end(),
                       [stable_id](const ScreenContribution& screen) {
                           return screen.id == stable_id;
                       });
}

bool NavigationController::route_visible(std::string_view stable_id) const noexcept {
    return std::any_of(rows_.begin(), rows_.end(),
                       [stable_id](const NavigationRow& row) {
                           return row.stable_id == stable_id;
                       });
}

void NavigationController::update_selected_flags() noexcept {
    for (NavigationRow& row : rows_) {
        row.selected = row.stable_id == current_route_;
    }
}

void NavigationController::prune_history() {
    const auto hidden = [this](const std::string& route) {
        return !route_visible(route);
    };
    std::erase_if(back_history_, hidden);
    std::erase_if(forward_history_, hidden);
}

app::Result<void, NavigationError> NavigationController::activate(
    std::string_view stable_id, bool record_history) {
    if (!route_exists(stable_id)) {
        return app::Result<void, NavigationError>::failure(
            error(NavigationErrorCode::UnknownRoute, stable_id,
                  "navigation.error.unknown_route"));
    }
    if (!has_access_ || !route_visible(stable_id)) {
        return app::Result<void, NavigationError>::failure(
            error(NavigationErrorCode::RouteNotVisible, stable_id,
                  "navigation.error.route_not_visible"));
    }
    if (current_route_ == stable_id && active_bridge_ != nullptr) {
        return app::Result<void, NavigationError>::success();
    }

    // Construct before changing the current route. A failed factory therefore
    // leaves the previously authorized route alive and selected.
    std::unique_ptr<PresentationBridge> next;
    try {
        next = registry_.create(stable_id, access_);
    } catch (const std::exception&) {
        return app::Result<void, NavigationError>::failure(
            error(NavigationErrorCode::BridgeConstructionFailed, stable_id,
                  "navigation.error.bridge_construction_failed"));
    } catch (...) {
        return app::Result<void, NavigationError>::failure(
            error(NavigationErrorCode::BridgeConstructionFailed, stable_id,
                  "navigation.error.bridge_construction_failed"));
    }
    if (next == nullptr) {
        return app::Result<void, NavigationError>::failure(
            error(NavigationErrorCode::BridgeConstructionFailed, stable_id,
                  "navigation.error.bridge_construction_failed"));
    }

    if (record_history && !current_route_.empty()) {
        back_history_.push_back(current_route_);
        if (back_history_.size() > kMaximumHistoryEntries) {
            back_history_.pop_front();
        }
        forward_history_.clear();
    }
    active_bridge_ = std::move(next);
    current_route_ = stable_id;
    update_selected_flags();
    return app::Result<void, NavigationError>::success();
}

app::Result<void, NavigationError> NavigationController::apply_access(
    NavigationAccess access) {
    if (has_access_) {
        if (access.session_generation < access_.session_generation ||
            (access.session_generation == access_.session_generation &&
             access.navigation_revision < access_.navigation_revision) ||
            (access.session_generation == access_.session_generation &&
             access.navigation_revision == access_.navigation_revision &&
             !(access == access_))) {
            return app::Result<void, NavigationError>::failure(
                error(NavigationErrorCode::StaleSnapshot, {},
                      "navigation.error.stale_snapshot"));
        }
        if (access == access_) {
            return app::Result<void, NavigationError>::success();
        }
    }

    const bool session_replaced = has_access_ &&
        access.session_generation != access_.session_generation;
    if (session_replaced) {
        // Destroy the old session's bridge before publishing any row from the
        // new session, even when both sessions can see the same route.
        active_bridge_.reset();
        current_route_.clear();
        back_history_.clear();
        forward_history_.clear();
    }

    access_ = std::move(access);
    has_access_ = true;
    const auto visible = registry_.visible(access_);
    rows_.clear();
    rows_.reserve(visible.size());
    for (const ScreenContribution* screen : visible) {
        rows_.push_back(row_from(*screen));
    }

    if (!current_route_.empty() && !route_visible(current_route_)) {
        // Authorization is gone. Destroy before any fallback bridge is made.
        active_bridge_.reset();
        current_route_.clear();
    }
    prune_history();

    if (current_route_.empty() && !rows_.empty()) {
        const auto selected = activate(rows_.front().stable_id, false);
        if (!selected) {
            update_selected_flags();
            return selected;
        }
    } else {
        update_selected_flags();
    }
    return app::Result<void, NavigationError>::success();
}

app::Result<void, NavigationError> NavigationController::select(
    std::string_view stable_id) {
    return activate(stable_id, true);
}

app::Result<void, NavigationError> NavigationController::go_back() {
    prune_history();
    if (back_history_.empty()) {
        return app::Result<void, NavigationError>::failure(
            error(NavigationErrorCode::NoHistory, {}, "navigation.error.no_history"));
    }
    const std::string destination = std::move(back_history_.back());
    back_history_.pop_back();
    const std::string previous = current_route_;
    auto result = activate(destination, false);
    if (result && !previous.empty()) {
        forward_history_.push_back(previous);
        if (forward_history_.size() > kMaximumHistoryEntries) {
            forward_history_.pop_front();
        }
    }
    return result;
}

app::Result<void, NavigationError> NavigationController::go_forward() {
    prune_history();
    if (forward_history_.empty()) {
        return app::Result<void, NavigationError>::failure(
            error(NavigationErrorCode::NoHistory, {}, "navigation.error.no_history"));
    }
    const std::string destination = std::move(forward_history_.back());
    forward_history_.pop_back();
    const std::string previous = current_route_;
    auto result = activate(destination, false);
    if (result && !previous.empty()) {
        back_history_.push_back(previous);
        if (back_history_.size() > kMaximumHistoryEntries) {
            back_history_.pop_front();
        }
    }
    return result;
}

void NavigationController::shutdown() noexcept {
    active_bridge_.reset();
    rows_.clear();
    current_route_.clear();
    back_history_.clear();
    forward_history_.clear();
    access_ = {};
    has_access_ = false;
}

}  // namespace squiflow::shell
