#include "shell/notification_queue.hpp"
#include "shell/shell_state.hpp"
#include "support/check.hpp"

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow::shell;

    t::section("identity theme connectivity and dirty guard");
    ShellState state;
    t::check(state.set_identity("Print shop", "Operator"),
             "bounded identity accepted");
    t::check(!state.set_identity("", "Operator"), "empty identity rejected");
    state.set_theme(ThemeChoice::Dark);
    state.set_high_contrast(true);
    state.set_reduced_motion(true);
    state.set_connectivity(ConnectivityState::Syncing);
    t::check(state.theme() == ThemeChoice::Dark &&
                 state.connectivity() == ConnectivityState::Syncing &&
                 state.high_contrast() && state.reduced_motion(),
             "theme, accessibility and connectivity have one portable owner");
    t::check(state.request_route("orders.list"), "clean route change approved");
    state.mark_dirty(true);
    t::check(!state.request_route("jobs.list") && state.awaiting_unsaved_decision(),
             "dirty route change pauses for confirmation");
    t::check(!state.resolve_unsaved(false) && state.dirty(),
             "cancel keeps edits and drops pending navigation");
    t::check(!state.request_route("bad route"), "unsafe route id rejected");
    t::check(!state.request_route("jobs.list"), "dirty route can be requested again");
    auto approved = state.resolve_unsaved(true);
    t::check(approved && *approved == "jobs.list" && !state.dirty(),
             "discard approves exactly the pending stable route");

    t::section("bounded deduplicated notifications");
    NotificationQueue queue;
    t::check(queue.push("sync.offline", "notification.offline", "cached",
                        NotificationSeverity::Warning),
             "valid notification accepted");
    t::check(queue.push("sync.offline", "notification.offline", "still cached",
                        NotificationSeverity::Warning) &&
                 queue.items().size() == 1 &&
                 queue.items().front().occurrences == 2,
             "same dedupe key updates rather than flooding");
    for (int index = 0; index < 6; ++index) {
        (void)queue.push("notice." + std::to_string(index), "notification.info", "",
                         NotificationSeverity::Information);
    }
    t::check(queue.items().size() == NotificationQueue::kMaximumVisible,
             "visible notification count is hard bounded");
    const auto id = queue.items().front().id;
    t::check(queue.dismiss(id) && !queue.dismiss("missing"),
             "dismissal uses stable notification identity");
    t::check(!queue.push("bad key", "notification.info", "",
                         NotificationSeverity::Information),
             "unsafe deduplication key rejected");

    return t::report();
}
