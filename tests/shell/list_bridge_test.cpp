#include "shell/list_bridge.hpp"
#include "support/check.hpp"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

std::vector<squiflow::shell::ListColumn> columns() {
    return {{"name", "list.column.name", true, true},
            {"status", "list.column.status", true, true},
            {"amount", "list.column.amount", false, false}};
}

std::vector<squiflow::shell::RowInput> rows(std::size_t count,
                                             std::size_t offset = 0) {
    std::vector<squiflow::shell::RowInput> result;
    result.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        result.push_back({std::to_string(offset + index),
                          "row " + std::to_string(offset + index), "detail"});
    }
    return result;
}

template <class Exception, class Action>
void rejects(Action&& action, const std::string& message) {
    try {
        action();
        squiflow::testing::check(false, message);
    } catch (const Exception&) {
        squiflow::testing::check(true, message);
    }
}

}  // namespace

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow::shell;

    t::section("column and query validation");
    rejects<std::invalid_argument>([] { ListBridge bridge({}); },
                                   "empty column set rejected");
    rejects<std::invalid_argument>([] {
        ListBridge bridge({{"name", "name", true, true},
                           {"name", "again", true, true}});
    }, "duplicate column id rejected");
    rejects<std::invalid_argument>([] {
        ListBridge bridge({{"Bad field", "name", true, true}});
    }, "unsafe column id rejected");

    ListBridge bridge(columns());
    auto bad_sort = bridge.begin_refresh("missing");
    t::check(!bad_sort && bad_sort.error().code == ListErrorCode::InvalidColumn,
             "unknown sort field refused");
    auto unsortable = bridge.begin_refresh("amount");
    t::check(!unsortable && unsortable.error().code == ListErrorCode::InvalidColumn,
             "unsortable field refused");
    auto missing_filter = bridge.begin_refresh({}, SortDirection::Ascending, {}, "open");
    t::check(!missing_filter &&
                 missing_filter.error().code == ListErrorCode::InvalidFilter,
             "filter text requires an explicit field");
    auto unfilterable = bridge.begin_refresh({}, SortDirection::Ascending, "amount", "1");
    t::check(!unfilterable &&
                 unfilterable.error().code == ListErrorCode::InvalidColumn,
             "unfilterable field refused");
    auto long_filter = bridge.begin_refresh({}, SortDirection::Ascending, "name",
                                             std::string(257, 'x'));
    t::check(!long_filter && long_filter.error().code == ListErrorCode::InvalidFilter,
             "oversized filter refused");

    t::section("normal paging, selection and state");
    auto first = bridge.begin_refresh("name", SortDirection::Descending,
                                      "status", "open");
    t::check(first && first.value().generation > 0 && first.value().offset == 0 &&
                 first.value().limit == PagedListCache::kMaximumPageRows,
             "refresh produces bounded first-page request");
    t::check(state_kind(bridge.state()) == ViewStateKind::Loading,
             "refresh enters loading state");
    const auto generation = first.value().generation;
    t::check(bridge.apply_page(generation, rows(3), true).has_value(),
             "first page applies");
    t::check(bridge.cache().row_count() == 3 &&
                 state_kind(bridge.state()) == ViewStateKind::Ready,
             "applied page owns exactly returned rows and becomes ready");
    t::check(bridge.select("1").has_value() && bridge.cache().selected() == "1",
             "selection uses stable row id");
    auto missing_row = bridge.select("missing");
    t::check(!missing_row && missing_row.error().code == ListErrorCode::UnknownRow,
             "stale or unknown row selection refused");

    auto next = bridge.next_page();
    t::check(next && next.value().offset == 3 && next.value().generation == generation,
             "next request continues authoritative query generation");
    auto concurrent_next = bridge.next_page();
    t::check(!concurrent_next &&
                 concurrent_next.error().code == ListErrorCode::NoRequestInFlight,
             "second page request cannot overlap one already running");
    t::check(bridge.apply_page(generation, rows(2, 3), false).has_value(),
             "final page applies");
    t::check(bridge.cache().row_count() == 5,
             "row count matches two returned pages exactly");
    auto no_more = bridge.next_page();
    t::check(!no_more && no_more.error().code == ListErrorCode::NoMorePages,
             "next page refused after authoritative has-more false");

    t::section("stale concurrency, malformed pages and cancellation");
    auto old = bridge.begin_refresh();
    auto current = bridge.begin_refresh("status");
    t::check(current.value().generation > old.value().generation,
             "new refresh supersedes in-flight generation");
    auto stale = bridge.apply_page(old.value().generation, rows(1), false);
    t::check(!stale && stale.error().code == ListErrorCode::StaleGeneration,
             "obsolete worker result cannot overwrite new refresh");
    t::check(bridge.apply_page(current.value().generation, rows(101), false).error().code ==
                 ListErrorCode::InvalidPage,
             "oversized page maps to explicit page error");
    // Invalid data does not consume the in-flight request, so the caller may
    // fail it explicitly or retry with a corrected authoritative page.
    t::check(bridge.apply_page(current.value().generation, rows(100), true).has_value(),
             "maximum-size page accepted after malformed retry");

    auto pending = bridge.next_page();
    bridge.cancel();
    t::check(state_kind(bridge.state()) == ViewStateKind::Idle &&
                 bridge.cache().row_count() == 0,
             "cancel invalidates work and clears owned page snapshots");
    auto after_cancel = bridge.apply_page(pending.value().generation, rows(1), false);
    t::check(!after_cancel && after_cancel.error().code == ListErrorCode::StaleGeneration,
             "worker completion after close/cancel is dropped as stale");

    t::section("failure and bounded large-list behavior");
    auto failing = bridge.begin_refresh();
    t::check(bridge.fail(failing.value().generation, "list.error.offline").has_value() &&
                 state_kind(bridge.state()) == ViewStateKind::Failed,
             "current load failure becomes explicit failed state");
    auto duplicate_fail = bridge.fail(failing.value().generation, "again");
    t::check(!duplicate_fail &&
                 duplicate_fail.error().code == ListErrorCode::NoRequestInFlight,
             "failure completion cannot be applied twice");

    auto bounded = bridge.begin_refresh();
    const auto bounded_generation = bounded.value().generation;
    t::check(bridge.apply_page(bounded_generation, rows(100, 0), true).has_value(),
             "bounded page one applied");
    t::check(bridge.next_page().has_value() &&
                 bridge.apply_page(bounded_generation, rows(100, 100), true).has_value(),
             "bounded page two applied");
    t::check(bridge.next_page().has_value() &&
                 bridge.apply_page(bounded_generation, rows(100, 200), true).has_value(),
             "bounded page three applied");
    t::check(bridge.select("0").has_value(), "row on oldest page selectable");
    t::check(bridge.next_page().has_value() &&
                 bridge.apply_page(bounded_generation, rows(100, 300), false).has_value(),
             "fourth page applied with oldest-page eviction");
    t::check(bridge.cache().page_count() == 3 && bridge.cache().row_count() == 300,
             "large list retains hard maximum of three pages");
    t::check(!bridge.cache().selected(),
             "selection is cleared when its owning page is evicted");
    const auto snapshot = bridge.cache().snapshot();
    t::check(snapshot.size() == 300 && snapshot.front().id == "100" &&
                 snapshot.back().id == "399",
             "owned snapshots exactly match retained page window");

    return t::report();
}
