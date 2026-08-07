#include "app/primary/primary_page_service.hpp"
#include "support/check.hpp"

#include <stdexcept>
#include <utility>

namespace {
using namespace squiflow;

class Query final : public app::primary::QueryPort {
  public:
    app::primary::ListPage page{};
    app::Result<app::primary::ListPage, app::DomainError> load(
        app::primary::PageKind, const app::primary::ListRequest&) override {
        return app::Result<app::primary::ListPage, app::DomainError>::success(page);
    }
};

app::RequestContext context(engine::RightsSet rights) {
    auto result = app::RequestContext::create({{1, 2}}, {3, 4}, std::move(rights),
                                               "primary-page-test", 1);
    if (!result) throw std::logic_error("test context failed");
    return std::move(result).value();
}
}

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;
    using app::primary::PageKind;

    Query query;
    query.page.rows = {{"0123456789abcdef0123456789abcdef", "Acme", "Customer"}};
    app::primary::PrimaryPageService service(query);
    auto activation = protocol::resolve_activation({}).activation;
    engine::RightsSet rights;
    rights.grant(protocol::RightId::right_party_read);

    t::section("authorized bounded list");
    auto result = service.list(context(rights), activation, PageKind::Parties, {});
    t::check(result && result.value().rows.size() == 1,
             "authorized party snapshot is returned");

    t::section("rights and activation fail closed");
    t::check(!service.list(context({}), activation, PageKind::Parties, {}),
             "missing read right is refused before query use");
    activation.active[static_cast<std::size_t>(protocol::ModuleId::parties)] = false;
    t::check(!service.list(context(rights), activation, PageKind::Parties, {}),
             "inactive module is refused");
    activation = protocol::resolve_activation({}).activation;

    t::section("query and provider bounds");
    app::primary::ListRequest request;
    request.limit = 101;
    t::check(!service.list(context(rights), activation, PageKind::Parties, request),
             "unbounded request is refused");
    request = {};
    request.filter_text = "Acme";
    t::check(!service.list(context(rights), activation, PageKind::Parties, request),
             "filter text requires an approved field");
    request.filter_field = "unknown";
    t::check(!service.list(context(rights), activation, PageKind::Parties, request),
             "unknown filter field is refused");

    t::section("malformed provider snapshots fail closed");
    query.page.rows = {{"bad id", "Acme", ""}};
    t::check(!service.list(context(rights), activation, PageKind::Parties, {}),
             "unsafe stable identity is rejected");
    query.page.rows = {{"same", "One", ""}, {"same", "Two", ""}};
    t::check(!service.list(context(rights), activation, PageKind::Parties, {}),
             "duplicate stable identity is rejected");
    query.page.rows.assign(101, {"row", "Title", ""});
    t::check(!service.list(context(rights), activation, PageKind::Parties, {}),
             "oversized provider page is rejected");

    return t::report();
}
