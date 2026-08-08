#include "app/workspace_runtime.hpp"

#include "engine/records/payload.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "modules/administration/module.hpp"
#include "support/check.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace squiflow;

constexpr char kMissingPerson[] = "10000000000000010000000000000099";
constexpr char kSettingKey[] = "10000000000000010000000000000050";

struct Fixture final {
    std::int64_t clock_value{1'800'000'000'000};
    modules::Registry registry{[this] { return clock_value; }};
    std::unique_ptr<engine::Database> database;

    Fixture() { registry.add(modules::administration::make_module([this] { return clock_value; })); }

    void open() {
        engine::MigrationRunner runner{[this] { return clock_value; }};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
    }
};

app::RequestContext context(engine::RightsSet rights, std::uint64_t generation,
                           engine::PersonId user) {
    auto created = app::RequestContext::create(app::TenantId{{1, 2}}, user, std::move(rights),
                                               "workspace-runtime-test", generation);
    if (!created) {
        throw std::logic_error("context creation failed");
    }
    return std::move(created).value();
}

engine::Session live_session(engine::PersonId person, engine::RightsSet rights) {
    engine::Session session;
    session.person = person;
    session.device = {9, 9};
    session.display_name = "Owner";
    session.is_owner = true;
    session.rights = std::move(rights);
    return session;
}

protocol::Activation all_active() {
    protocol::Activation activation;
    activation.active.fill(true);
    return activation;
}

app::primary::CommandRequest setting_command() {
    engine::Row fields;
    fields.set("value", engine::Value::text("50"));
    app::primary::CommandRequest request;
    request.operation = protocol::OperationId::shop_setting_update;
    request.record_id = kSettingKey;
    request.payload = engine::encode_payload(fields);
    request.idempotency_key = "workspace-runtime-test-1";
    return request;
}

}  // namespace

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;

    const engine::PersonId owner_id{3, 4};
    const engine::PersonId other_id{5, 6};
    const auto activation = all_active();

    engine::RightsSet read_rights;
    read_rights.grant(protocol::RightId::right_person_manage);

    t::section("a fresh workspace starts signed out with generation zero");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        t::check(!workspace.signed_in(), "a new workspace is not signed in");
        t::check(workspace.session_generation() == 0, "a new workspace has generation zero");
    }

    t::section("reads are refused before any sign-in, even with an otherwise valid context");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        auto refused = workspace.record(context(read_rights, 1, owner_id), activation,
                                        app::primary::PageKind::Administration, kMissingPerson);
        t::check(!refused && refused.error().code == app::DomainErrorCode::Unauthorized,
                 "a context is refused when the workspace has never signed in");
    }

    t::section("a signed-in read reaches the real query layer");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        const std::uint64_t generation =
            workspace.sign_in(live_session(owner_id, read_rights));
        t::check(generation != 0, "sign-in returns a non-zero generation");
        t::check(workspace.signed_in(), "the workspace reports signed in after sign-in");
        t::check(workspace.session_generation() == generation,
                 "the reported generation matches the one sign-in returned");

        auto not_found = workspace.record(context(read_rights, generation, owner_id), activation,
                                          app::primary::PageKind::Administration, kMissingPerson);
        t::check(!not_found && not_found.error().code == app::DomainErrorCode::NotFound,
                 "an authorized read for a record that does not exist reaches the query and "
                 "comes back not-found, proving the request passed authorization");
    }

    t::section("signing in again invalidates a context captured under the earlier generation");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        const std::uint64_t first = workspace.sign_in(live_session(owner_id, read_rights));
        const auto stale_context = context(read_rights, first, owner_id);
        const std::uint64_t second = workspace.sign_in(live_session(owner_id, read_rights));
        t::check(second != first, "a repeated sign-in produces a new generation");

        auto refused = workspace.record(stale_context, activation,
                                        app::primary::PageKind::Administration, kMissingPerson);
        t::check(!refused && refused.error().code == app::DomainErrorCode::Unauthorized,
                 "a context captured before the latest sign-in is refused");
    }

    t::section("a context for a different user or with an escalated right is refused");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        const std::uint64_t generation =
            workspace.sign_in(live_session(owner_id, read_rights));

        auto wrong_user = workspace.record(context(read_rights, generation, other_id), activation,
                                           app::primary::PageKind::Administration, kMissingPerson);
        t::check(!wrong_user && wrong_user.error().code == app::DomainErrorCode::Unauthorized,
                 "a context built for a different person is refused");

        engine::RightsSet elevated = read_rights;
        elevated.grant(protocol::RightId::right_shop_settings);
        auto escalated = workspace.record(context(elevated, generation, owner_id), activation,
                                          app::primary::PageKind::Administration, kMissingPerson);
        t::check(!escalated && escalated.error().code == app::DomainErrorCode::Unauthorized,
                 "a context claiming a right the live session was never granted is refused");
    }

    t::section("signing out refuses every previously valid context");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        const std::uint64_t generation =
            workspace.sign_in(live_session(owner_id, read_rights));
        const auto valid_context = context(read_rights, generation, owner_id);
        workspace.sign_out();
        t::check(!workspace.signed_in(), "the workspace reports signed out");
        t::check(workspace.session_generation() == 0, "generation resets to zero on sign-out");

        auto refused = workspace.record(valid_context, activation,
                                        app::primary::PageKind::Administration, kMissingPerson);
        t::check(!refused && refused.error().code == app::DomainErrorCode::Unauthorized,
                 "a context captured before sign-out is refused afterwards");
    }

    t::section("an authorized, online command reaches the real registry and succeeds");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        engine::RightsSet write_rights;
        write_rights.grant(protocol::RightId::right_shop_settings);
        const std::uint64_t generation =
            workspace.sign_in(live_session(owner_id, write_rights));
        workspace.set_connection_state(engine::ConnectionState::Online);
        t::check(workspace.connection_state() == engine::ConnectionState::Online,
                 "the workspace reports the connection state it was given");

        auto ack = workspace.dispatch(context(write_rights, generation, owner_id),
                                      setting_command());
        t::check(bool(ack), "a fully authorized, online write is accepted by the real registry");
    }

    t::section("the same command is refused offline, proving connection state reaches dispatch");
    {
        Fixture fixture;
        fixture.open();
        app::AuthenticatedWorkspace workspace(fixture.registry, *fixture.database);
        engine::RightsSet write_rights;
        write_rights.grant(protocol::RightId::right_shop_settings);
        const std::uint64_t generation =
            workspace.sign_in(live_session(owner_id, write_rights));
        workspace.set_connection_state(engine::ConnectionState::Offline);

        auto refused = workspace.dispatch(context(write_rights, generation, owner_id),
                                          setting_command());
        t::check(!refused, "an online-only command is refused while the workspace is offline");
    }

    return t::report();
}
