#include "app/primary/record_page_service.hpp"
#include "support/check.hpp"

#include <stdexcept>
#include <utility>

namespace {
using namespace squiflow;

class Query final : public app::primary::RecordQueryPort {
  public:
    bool called{false};
    app::Result<app::primary::RecordSnapshot, app::DomainError> next =
        app::Result<app::primary::RecordSnapshot, app::DomainError>::failure(
            {app::DomainErrorCode::NotFound, "record.error.not_found", {}});

    app::Result<app::primary::RecordSnapshot, app::DomainError> load(
        app::primary::PageKind, std::string_view) override {
        called = true;
        return next;
    }
};

app::RequestContext context(engine::RightsSet rights) {
    auto created = app::RequestContext::create({{1, 2}}, {3, 4}, std::move(rights),
                                               "record-page-test", 7);
    if (!created) {
        throw std::logic_error("context creation failed");
    }
    return std::move(created).value();
}

app::primary::RecordSnapshot good_record() {
    app::primary::RecordSnapshot snapshot;
    snapshot.stable_id = "0123456789abcdef0123456789abcdef";
    snapshot.title = "Acme Works";
    snapshot.subtitle = "customer";
    snapshot.fields.push_back({"field.name", "record.name", "Acme Works",
                               std::nullopt, std::nullopt});
    snapshot.lines.push_back({"line.1", "Primary", "detail", "", "",
                              std::nullopt, std::nullopt});
    snapshot.history.push_back({"history.created", "record.history.created",
                                "owner", 10});
    snapshot.actions.push_back({"action.update", "record.action.party_update",
                                protocol::OperationId::party_update,
                                snapshot.stable_id});
    snapshot.actions.push_back({"action.terms", "record.action.party_terms_set",
                                protocol::OperationId::party_terms_set,
                                snapshot.stable_id});
    return snapshot;
}
}

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;
    Query query;
    app::primary::RecordPageService service(query);
    auto activation = protocol::resolve_activation({}).activation;

    t::section("invalid kinds and malformed record ids fail before provider use");
    t::check(!service.load(context({}), activation, app::primary::PageKind::Count,
                           "0123456789abcdef0123456789abcdef") &&
                 !query.called,
             "invalid kind stops before query");
    t::check(!service.load(context({}), activation, app::primary::PageKind::Parties,
                           "not-a-record-id") &&
                 !query.called,
             "malformed record id stops before query");

    t::section("activation and read rights gate record detail access");
    engine::RightsSet read;
    read.grant(protocol::RightId::right_party_read);
    read.grant(protocol::RightId::right_party_write);
    auto denied = service.load(context({}), activation, app::primary::PageKind::Parties,
                               "0123456789abcdef0123456789abcdef");
    t::check(!denied && !query.called, "missing read right is refused");
    activation.active[static_cast<std::size_t>(protocol::ModuleId::parties)] = false;
    auto inactive = service.load(context(read), activation, app::primary::PageKind::Parties,
                                 "0123456789abcdef0123456789abcdef");
    t::check(!inactive && !query.called, "inactive module is refused");
    activation = protocol::resolve_activation({}).activation;

    t::section("typed actions are filtered by exact operation rights");
    query.next = app::Result<app::primary::RecordSnapshot, app::DomainError>::success(
        good_record());
    query.called = false;
    auto filtered = service.load(context(read), activation, app::primary::PageKind::Parties,
                                 "0123456789abcdef0123456789abcdef");
    t::check(filtered && query.called && filtered.value().actions.size() == 1 &&
                 filtered.value().actions.front().operation ==
                     protocol::OperationId::party_update,
             "actions needing absent write rights are removed");

    t::section("malformed provider snapshots fail closed");
    auto bad = good_record();
    bad.actions.front().operation = protocol::OperationId::Count;
    query.next = app::Result<app::primary::RecordSnapshot, app::DomainError>::success(
        std::move(bad));
    t::check(!service.load(context(read), activation, app::primary::PageKind::Parties,
                           "0123456789abcdef0123456789abcdef"),
             "invalid action metadata rejects snapshot");
    bad = good_record();
    bad.actions.front().record_id = "ffffffffffffffffffffffffffffffff";
    query.next = app::Result<app::primary::RecordSnapshot, app::DomainError>::success(
        std::move(bad));
    t::check(!service.load(context(read), activation, app::primary::PageKind::Parties,
                           "0123456789abcdef0123456789abcdef"),
             "provider cannot swap record identity");

    t::section("provider not-found passes through");
    query.next = app::Result<app::primary::RecordSnapshot, app::DomainError>::failure(
        {app::DomainErrorCode::NotFound, "record.error.not_found", {}});
    auto missing = service.load(context(read), activation, app::primary::PageKind::Parties,
                                "0123456789abcdef0123456789abcdef");
    t::check(!missing && missing.error().code == app::DomainErrorCode::NotFound,
             "missing records stay stable");

    return t::report();
}
