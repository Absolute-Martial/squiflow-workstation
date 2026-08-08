#include "app/primary/command_gateway.hpp"

#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "support/check.hpp"

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
using namespace squiflow;

constexpr char kGoodRecord[] = "0123456789abcdef0123456789abcdef";
constexpr char kRefusedRecord[] = "badbadbadbadbadbadbadbadbadbadba";

// Owns administration for this test only: on_write for a synchronizable
// setting change and a local-only export, on_read for a stand-in read so the
// gateway's read-rejection can be exercised against a real registered
// operation rather than an imagined one.
class TestAdministration final : public modules::Module {
  public:
    protocol::ModuleId id() const noexcept override {
        return protocol::ModuleId::administration;
    }

    std::vector<engine::Migration> migrations() const override {
        engine::Migration schema;
        schema.number = 10;
        schema.name = "command gateway test tables";
        schema.schema = [](engine::Store& store) {
            store.define_table("shop_setting_log", "id");
            store.define_table("audit_export_log", "id");
        };
        return {schema};
    }

    void install(modules::Registry& registry) override {
        registry.on_write(protocol::OperationId::shop_setting_update,
                          [](engine::Transaction& transaction, const modules::Call& call) {
                              engine::Row row;
                              row.set("id", engine::Value::text(call.record_id));
                              transaction.insert("shop_setting_log", row);
                          });
        registry.on_write(protocol::OperationId::audit_export,
                          [](engine::Transaction& transaction, const modules::Call& call) {
                              if (call.record_id == kRefusedRecord) {
                                  throw modules::RuleViolation("audit export refused");
                              }
                              engine::Row row;
                              row.set("id", engine::Value::text(
                                                call.record_id.empty() ? "local" : call.record_id));
                              transaction.insert("audit_export_log", row);
                          });
        registry.on_read(protocol::OperationId::person_create,
                         [](const engine::Store&, const modules::Call&) {
                             return std::vector<engine::Row>{};
                         });
    }
};

struct Fixture final {
    std::int64_t clock_value{1'800'000'000'000};
    modules::Registry registry{[this] { return clock_value; }};
    std::unique_ptr<engine::Database> database;

    Fixture() { registry.add(std::make_unique<TestAdministration>()); }

    void open() {
        engine::MigrationRunner runner{[this] { return clock_value; }};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(
            std::make_unique<engine::MemoryStore>(), std::move(runner));
        database->open();
    }

    std::size_t count(const std::string& table) const {
        std::size_t rows = 0;
        database->read([&](const engine::Store& store) { rows = store.count(table); });
        return rows;
    }
};

app::RequestContext context(engine::RightsSet rights, std::uint64_t generation,
                           engine::PersonId user) {
    auto created = app::RequestContext::create({{1, 2}}, user, std::move(rights),
                                               "command-gateway-test", generation);
    if (!created) {
        throw std::logic_error("context creation failed");
    }
    return std::move(created).value();
}

engine::Session live_session(engine::PersonId person, engine::RightsSet rights,
                             bool owner = true) {
    engine::Session session;
    session.person = person;
    session.device = {9, 9};
    session.display_name = "Owner";
    session.is_owner = owner;
    session.rights = std::move(rights);
    return session;
}

engine::RightsSet full_rights() {
    engine::RightsSet rights;
    rights.grant(protocol::RightId::right_shop_settings);
    rights.grant(protocol::RightId::right_audit_read);
    return rights;
}

app::primary::CommandRequest setting_command(std::string record_id, std::string key) {
    app::primary::CommandRequest request;
    request.operation = protocol::OperationId::shop_setting_update;
    request.record_id = std::move(record_id);
    request.idempotency_key = std::move(key);
    return request;
}

app::primary::CommandRequest export_command(std::string record_id = {}) {
    app::primary::CommandRequest request;
    request.operation = protocol::OperationId::audit_export;
    request.record_id = std::move(record_id);
    return request;
}

}  // namespace

int main() {
    namespace t = squiflow::testing;
    using namespace squiflow;

    const engine::PersonId owner_id{3, 4};
    const engine::PersonId other_id{5, 6};

    t::section("a session snapshot that no longer matches the live session is refused");
    {
        Fixture fixture;
        fixture.open();
        app::primary::CommandGateway gateway(fixture.registry, *fixture.database);
        const engine::Session live = live_session(owner_id, full_rights());

        auto no_session = gateway.dispatch(context(full_rights(), 7, owner_id), live, 0,
                                           engine::ConnectionState::Online,
                                           export_command());
        t::check(!no_session && no_session.error().code == app::DomainErrorCode::Unauthorized,
                 "generation zero live session is unauthorized");

        auto stale_generation = gateway.dispatch(context(full_rights(), 7, owner_id), live, 8,
                                                 engine::ConnectionState::Online,
                                                 export_command());
        t::check(!stale_generation &&
                     stale_generation.error().code == app::DomainErrorCode::Unauthorized,
                 "mismatched session generation is unauthorized");

        auto wrong_user = gateway.dispatch(context(full_rights(), 7, other_id), live, 7,
                                           engine::ConnectionState::Online, export_command());
        t::check(!wrong_user && wrong_user.error().code == app::DomainErrorCode::Unauthorized,
                 "context built for a different user is unauthorized");

        engine::RightsSet elevated = full_rights();
        elevated.grant(protocol::RightId::right_device_manage);
        auto elevated_context = gateway.dispatch(context(elevated, 7, owner_id), live, 7,
                                                 engine::ConnectionState::Online,
                                                 export_command());
        t::check(!elevated_context &&
                     elevated_context.error().code == app::DomainErrorCode::Unauthorized,
                 "a snapshot claiming a right the live session lost is unauthorized");
        t::check(fixture.count("audit_export_log") == 0, "nothing was written for any refusal");
    }

    t::section("malformed or unhandled commands are refused before a call is built");
    {
        Fixture fixture;
        fixture.open();
        app::primary::CommandGateway gateway(fixture.registry, *fixture.database);
        const engine::Session live = live_session(owner_id, full_rights());
        auto ctx = context(full_rights(), 7, owner_id);

        auto invalid_operation = gateway.dispatch(
            ctx, live, 7, engine::ConnectionState::Online,
            app::primary::CommandRequest{protocol::OperationId::Count, {}, {}, {}});
        t::check(!invalid_operation &&
                     invalid_operation.error().code == app::DomainErrorCode::ValidationFailed,
                 "invalid operation enumerator is refused");

        auto unhandled_operation = gateway.dispatch(
            ctx, live, 7, engine::ConnectionState::Online,
            app::primary::CommandRequest{protocol::OperationId::person_disable, {}, {}, {}});
        t::check(!unhandled_operation &&
                     unhandled_operation.error().code == app::DomainErrorCode::ValidationFailed,
                 "an operation with no handler is refused, not thrown");

        auto read_operation = gateway.dispatch(
            ctx, live, 7, engine::ConnectionState::Online,
            app::primary::CommandRequest{protocol::OperationId::person_create, {}, {}, {}});
        t::check(!read_operation &&
                     read_operation.error().code == app::DomainErrorCode::ValidationFailed,
                 "a read-kind operation is refused by the command gateway");

        auto malformed_record = gateway.dispatch(
            ctx, live, 7, engine::ConnectionState::Online,
            setting_command("not-a-record-id", "key-1"));
        t::check(!malformed_record &&
                     malformed_record.error().code == app::DomainErrorCode::ValidationFailed,
                 "a non-canonical record id is refused");

        auto missing_key = gateway.dispatch(ctx, live, 7, engine::ConnectionState::Online,
                                            setting_command(kGoodRecord, ""));
        t::check(!missing_key &&
                     missing_key.error().code == app::DomainErrorCode::ValidationFailed,
                 "a synchronizable command with no idempotency key is refused");

        auto bad_key = gateway.dispatch(
            ctx, live, 7, engine::ConnectionState::Online,
            setting_command(kGoodRecord, std::string(200, 'a')));
        t::check(!bad_key && bad_key.error().code == app::DomainErrorCode::ValidationFailed,
                 "an oversized idempotency key is refused");

        auto stray_key = gateway.dispatch(
            ctx, live, 7, engine::ConnectionState::Online, export_command(""));
        auto with_key = export_command();
        with_key.idempotency_key = "unexpected";
        auto rejected_key = gateway.dispatch(ctx, live, 7, engine::ConnectionState::Online,
                                             with_key);
        t::check(!rejected_key &&
                     rejected_key.error().code == app::DomainErrorCode::ValidationFailed,
                 "an idempotency key on a non-synchronizable command is refused");
        t::check(bool(stray_key), "the same command without a key is otherwise well formed");
        t::check(fixture.count("shop_setting_log") == 0 && fixture.count("audit_export_log") == 1,
                 "only the well formed local command actually wrote a row");
    }

    t::section("a successful synchronizable command queues once and replays on retry");
    {
        Fixture fixture;
        fixture.open();
        app::primary::CommandGateway gateway(fixture.registry, *fixture.database);
        const engine::Session live = live_session(owner_id, full_rights());
        auto ctx = context(full_rights(), 7, owner_id);

        auto first = gateway.dispatch(ctx, live, 7, engine::ConnectionState::Online,
                                      setting_command(kGoodRecord, "attempt-1"));
        t::check(bool(first) && first.value().queued && !first.value().replayed,
                 "first attempt commits and queues");
        t::check(fixture.count("shop_setting_log") == 1, "one row written");

        auto retry = gateway.dispatch(ctx, live, 7, engine::ConnectionState::Online,
                                      setting_command(kGoodRecord, "attempt-1"));
        t::check(bool(retry) && retry.value().replayed && !retry.value().queued,
                 "retry with the same key replays instead of repeating");
        t::check(fixture.count("shop_setting_log") == 1, "replay writes nothing new");
    }

    t::section("capability refusals and domain rule refusals are classified differently");
    {
        Fixture fixture;
        fixture.open();
        app::primary::CommandGateway gateway(fixture.registry, *fixture.database);

        engine::RightsSet no_settings;
        no_settings.grant(protocol::RightId::right_audit_read);
        const engine::Session live_missing_right = live_session(owner_id, no_settings);
        auto ctx_missing_right = context(no_settings, 7, owner_id);
        auto missing_right = gateway.dispatch(
            ctx_missing_right, live_missing_right, 7, engine::ConnectionState::Online,
            setting_command(kGoodRecord, "attempt-2"));
        t::check(!missing_right &&
                     missing_right.error().code == app::DomainErrorCode::Unauthorized,
                 "a capability refusal (no right) is unauthorized");

        const engine::Session live = live_session(owner_id, full_rights());
        auto ctx = context(full_rights(), 7, owner_id);
        auto offline_refusal = gateway.dispatch(ctx, live, 7, engine::ConnectionState::Offline,
                                                setting_command(kGoodRecord, "attempt-3"));
        t::check(!offline_refusal &&
                     offline_refusal.error().code == app::DomainErrorCode::Unauthorized,
                 "an online-only command while offline is unauthorized");

        auto offline_local = gateway.dispatch(ctx, live, 7, engine::ConnectionState::Offline,
                                              export_command());
        t::check(bool(offline_local), "an offline-allowed local command still succeeds offline");

        auto rule_violation = gateway.dispatch(ctx, live, 7, engine::ConnectionState::Online,
                                               export_command(kRefusedRecord));
        t::check(!rule_violation && rule_violation.error().code == app::DomainErrorCode::Conflict,
                 "a domain rule refusal is a conflict, not an authorization failure");
        t::check(fixture.count("shop_setting_log") == 0, "the refused settings change wrote nothing");
    }

    return t::report();
}
