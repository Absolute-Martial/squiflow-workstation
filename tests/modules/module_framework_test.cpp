// The module framework: what a module may do, and what it may not.
//
// The interesting tests here are the refusals. A framework that only proves it
// can run a handler proves nothing - the value is in what it makes impossible.

#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/operation_table.hpp>

#include "engine/storage/database.hpp"
#include "engine/storage/memory_store.hpp"
#include "engine/storage/store.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/registry.hpp"
#include "support/check.hpp"

using squiflow::testing::check;
using squiflow::testing::section;

namespace engine = squiflow::engine;
namespace modules = squiflow::modules;
namespace protocol = squiflow::protocol;

namespace {

std::int64_t g_now = 1'700'000'000'000;
std::int64_t now() { return g_now; }

engine::RecordId some_id(std::uint64_t low) { return engine::RecordId{1, low}; }

// A module that handles every operation the protocol declares for it. Reads
// for the local-only ones, writes for the rest - which is what a real module
// does, minus the business logic.
class HonestModule final : public modules::Module {
public:
    HonestModule(protocol::ModuleId id, std::string table, int migration_number)
        : id_(id), table_(std::move(table)), migration_number_(migration_number) {}

    protocol::ModuleId id() const noexcept override { return id_; }

    std::vector<engine::Migration> migrations() const override {
        engine::Migration m;
        m.number = migration_number_;
        m.name = std::string(protocol::module_name(id_));
        const std::string table = table_;
        m.schema = [table](engine::Store& store) { store.define_table(table, "id"); };
        return {m};
    }

    void install(modules::Registry& registry) override {
        for (const protocol::OperationInfo& info : protocol::all_operations()) {
            if (info.module != id_) {
                continue;
            }
            if (info.sync_class == protocol::OperationClass::LocalOnly) {
                registry.on_read(info.id, [this](const engine::Store& store,
                                                 const modules::Call&) {
                    ++reads;
                    std::vector<engine::Row> rows;
                    const std::optional<engine::Row> row = store.find(table_, "kept");
                    if (row) {
                        rows.push_back(*row);
                    }
                    return rows;
                });
            } else {
                registry.on_write(info.id, [this](engine::Transaction& transaction,
                                                  const modules::Call& call) {
                    if (fail_next) {
                        throw engine::StoreError("the handler refused");
                    }
                    engine::Row row;
                    row.set("id", engine::Value::text(call.record_id));
                    row.set("note", engine::Value::text("written"));
                    transaction.insert(table_, row);
                    ++writes;
                });
            }
        }
    }

    int writes = 0;
    int reads = 0;
    bool fail_next = false;

private:
    protocol::ModuleId id_;
    std::string table_;
    int migration_number_;
};

// A module that reaches for an operation belonging to somebody else.
class TrespassingModule final : public modules::Module {
public:
    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::catalog; }
    std::vector<engine::Migration> migrations() const override { return {}; }
    void install(modules::Registry& registry) override {
        registry.on_write(protocol::OperationId::party_create,
                          [](engine::Transaction&, const modules::Call&) {});
    }
};

// A module that registers the same operation twice.
class RepetitiveModule final : public modules::Module {
public:
    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::parties; }
    std::vector<engine::Migration> migrations() const override { return {}; }
    void install(modules::Registry& registry) override {
        registry.on_write(protocol::OperationId::party_create,
                          [](engine::Transaction&, const modules::Call&) {});
        registry.on_write(protocol::OperationId::party_create,
                          [](engine::Transaction&, const modules::Call&) {});
    }
};

// A module claiming a migration number reserved for the engine.
class GreedyModule final : public modules::Module {
public:
    protocol::ModuleId id() const noexcept override { return protocol::ModuleId::parties; }
    std::vector<engine::Migration> migrations() const override {
        engine::Migration m;
        m.number = 2;
        m.name = "too low";
        m.schema = [](engine::Store&) {};
        return {m};
    }
    void install(modules::Registry&) override {}
};

engine::Session owner_session() {
    engine::Session session;
    session.person = some_id(1);
    session.device = some_id(2);
    session.display_name = "the shopkeeper";
    session.is_owner = true;
    session.rights.grant_all();
    return session;
}

struct Fixture {
    modules::Registry registry{now};
    HonestModule* parties = nullptr;
    std::unique_ptr<engine::Database> database;

    void build(bool with_files = false) {
        auto parties_module = std::make_unique<HonestModule>(protocol::ModuleId::parties,
                                                             "party", 10);
        parties = parties_module.get();
        registry.add(std::move(parties_module));
        if (with_files) {
            registry.add(std::make_unique<HonestModule>(protocol::ModuleId::files, "design_file", 20));
        }

        engine::MigrationRunner runner{now};
        registry.collect_migrations(runner);
        database = std::make_unique<engine::Database>(std::make_unique<engine::MemoryStore>(),
                                                      std::move(runner));
        database->open();
    }

    engine::OutboxCounts outbox_counts() const {
        const engine::Outbox outbox{now};
        engine::OutboxCounts counts;
        database->read([&](const engine::Store& store) { counts = outbox.counts(store); });
        return counts;
    }

    bool row_exists(const std::string& table, const std::string& key) const {
        bool found = false;
        database->read([&](const engine::Store& store) {
            found = store.find(table, key).has_value();
        });
        return found;
    }
};

}  // namespace

int main() {
    section("a module registers the operations it owns");
    {
        Fixture fixture;
        fixture.build();
        check(fixture.registry.size() == 1, "one module registered");
        check(fixture.registry.has(protocol::ModuleId::parties), "parties is registered");
        check(!fixture.registry.has(protocol::ModuleId::orders), "orders is not");
        check(fixture.registry.handled(protocol::OperationId::party_create),
              "party_create has a handler");
        check(fixture.registry.unhandled().empty(),
              "no declared parties operation is left without a handler");
        bool complete = true;
        try {
            fixture.registry.require_complete();
        } catch (const modules::RegistryError&) {
            complete = false;
        }
        check(complete, "require_complete passes when every operation is handled");
    }

    section("a module cannot handle another module's operation");
    {
        modules::Registry registry{now};
        bool refused = false;
        std::string message;
        try {
            registry.add(std::make_unique<TrespassingModule>());
        } catch (const modules::RegistryError& error) {
            refused = true;
            message = error.what();
        }
        check(refused, "catalog cannot handle a parties operation");
        check(message.find("belongs to") != std::string::npos,
              "the refusal says who the operation belongs to");
    }

    section("one operation, one handler");
    {
        modules::Registry registry{now};
        bool refused = false;
        try {
            registry.add(std::make_unique<RepetitiveModule>());
        } catch (const modules::RegistryError&) {
            refused = true;
        }
        check(refused, "a second handler for the same operation is refused");
    }

    section("a module cannot be registered twice");
    {
        modules::Registry registry{now};
        registry.add(std::make_unique<HonestModule>(protocol::ModuleId::parties, "party", 10));
        bool refused = false;
        try {
            registry.add(std::make_unique<HonestModule>(protocol::ModuleId::parties, "party", 11));
        } catch (const modules::RegistryError&) {
            refused = true;
        }
        check(refused, "the same module twice is refused");
    }

    section("handlers can only be registered while installing");
    {
        modules::Registry registry{now};
        bool refused = false;
        try {
            registry.on_write(protocol::OperationId::party_create,
                              [](engine::Transaction&, const modules::Call&) {});
        } catch (const modules::RegistryError&) {
            refused = true;
        }
        check(refused, "registration outside install() is refused");
    }

    section("migration numbering is global, and the engine owns the low numbers");
    {
        modules::Registry registry{now};
        registry.add(std::make_unique<GreedyModule>());
        engine::MigrationRunner runner{now};
        bool refused = false;
        try {
            registry.collect_migrations(runner);
        } catch (const modules::RegistryError&) {
            refused = true;
        }
        check(refused, "a module claiming migration 2 is refused");

        modules::Registry clashing{now};
        clashing.add(std::make_unique<HonestModule>(protocol::ModuleId::parties, "party", 10));
        clashing.add(std::make_unique<HonestModule>(protocol::ModuleId::catalog, "product", 10));
        engine::MigrationRunner second{now};
        bool clash_refused = false;
        try {
            clashing.collect_migrations(second);
        } catch (const modules::RegistryError&) {
            clash_refused = true;
        }
        check(clash_refused, "two modules claiming one number is refused");
    }

    section("the engine's own tables exist without any module creating them");
    {
        Fixture fixture;
        fixture.build();
        bool outbox_defined = false;
        bool cursor_defined = false;
        bool conflict_defined = false;
        fixture.database->read([&](const engine::Store& store) {
            outbox_defined = store.has_table(engine::Outbox::table_name());
            cursor_defined = store.has_table(engine::Cursor::table_name());
            conflict_defined = store.has_table(engine::ConflictLog::table_name());
        });
        check(outbox_defined, "the outbox table exists");
        check(cursor_defined, "the sync cursor table exists");
        check(conflict_defined, "the conflict log exists");
        check(fixture.database->version() >= 10, "module migrations ran too");
    }

    section("the rules are checked before the handler runs");
    {
        Fixture fixture;
        fixture.build();

        engine::Session no_rights = owner_session();
        no_rights.rights.clear();

        modules::Call call;
        call.operation = protocol::OperationId::party_create;
        call.record_id = "party-1";
        call.idempotency_key = "key-1";

        const modules::Outcome outcome =
            fixture.registry.run(*fixture.database, call, no_rights, engine::ConnectionState::Online);
        check(!outcome.ok, "refused without the right");
        check(outcome.reason == engine::DenialReason::NoRight, "and says which rule refused it");
        check(fixture.parties->writes == 0, "the handler never ran");
        check(fixture.outbox_counts().total() == 0, "and nothing was queued");

        engine::Session nobody;
        const modules::Outcome signed_out =
            fixture.registry.run(*fixture.database, call, nobody, engine::ConnectionState::Online);
        check(signed_out.reason == engine::DenialReason::NotSignedIn,
              "nobody signed in is refused first of all");
    }

    section("a change that will be sent is queued in the same transaction");
    {
        Fixture fixture;
        fixture.build();

        modules::Call call;
        call.operation = protocol::OperationId::party_create;
        call.record_id = "party-1";
        call.idempotency_key = "key-1";

        const modules::Outcome outcome = fixture.registry.run(
            *fixture.database, call, owner_session(), engine::ConnectionState::Online);
        check(outcome.ok, "the change was made");
        check(outcome.queued, "and it was queued to be sent");
        check(fixture.parties->writes == 1, "the handler ran once");
        check(fixture.row_exists("party", "party-1"), "the record is there");
        check(fixture.outbox_counts().total() == 1, "exactly one outbox entry");

        // The same key again: the change is already queued, and enqueuing is
        // idempotent, so this is not an error and not a second entry.
        const modules::Outcome again = fixture.registry.run(
            *fixture.database, call, owner_session(), engine::ConnectionState::Online);
        check(again.ok, "a repeat with the same key is not an error");
        check(again.replayed, "and is recognised as the same change");
        check(!again.queued, "so nothing is queued a second time");
        check(fixture.parties->writes == 1, "and the handler did not run again");
        check(fixture.outbox_counts().total() == 1, "still one outbox entry");
    }

    section("a change that rolls back is not left queued");
    {
        Fixture fixture;
        fixture.build();
        fixture.parties->fail_next = true;

        modules::Call call;
        call.operation = protocol::OperationId::party_create;
        call.record_id = "party-2";
        call.idempotency_key = "key-2";

        bool threw = false;
        try {
            fixture.registry.run(*fixture.database, call, owner_session(),
                                 engine::ConnectionState::Online);
        } catch (const engine::StoreError&) {
            threw = true;
        }
        check(threw, "the handler's refusal reaches the caller");
        check(!fixture.row_exists("party", "party-2"), "the record was not written");
        check(fixture.outbox_counts().total() == 0,
              "and nothing is queued to send a change that never happened");
    }

    section("a synchronised change needs a record and a key");
    {
        Fixture fixture;
        fixture.build();

        modules::Call no_key;
        no_key.operation = protocol::OperationId::party_create;
        no_key.record_id = "party-3";

        bool refused = false;
        try {
            fixture.registry.run(*fixture.database, no_key, owner_session(),
                                 engine::ConnectionState::Online);
        } catch (const modules::RegistryError&) {
            refused = true;
        }
        check(refused, "no idempotency key is refused");

        modules::Call no_record;
        no_record.operation = protocol::OperationId::party_create;
        no_record.idempotency_key = "key-3";
        bool record_refused = false;
        try {
            fixture.registry.run(*fixture.database, no_record, owner_session(),
                                 engine::ConnectionState::Online);
        } catch (const modules::RegistryError&) {
            record_refused = true;
        }
        check(record_refused, "no record to order against is refused");
        check(fixture.parties->writes == 0, "and neither reached the handler");
    }

    section("a read runs against the store and returns rows");
    {
        Fixture fixture;
        fixture.build(true);

        modules::Call call;
        call.operation = protocol::OperationId::file_search;

        const modules::Outcome outcome = fixture.registry.run(
            *fixture.database, call, owner_session(), engine::ConnectionState::Online);
        check(outcome.ok, "the search ran");
        check(outcome.rows.empty(), "and found nothing, which is not an error");
        check(fixture.outbox_counts().total() == 0, "a read queues nothing");
    }

    section("a switched-off module looks absent, not forbidden");
    {
        Fixture fixture;
        fixture.build(true);
        fixture.registry.set_disabled({protocol::ModuleId::files});
        check(!fixture.registry.active(protocol::ModuleId::files), "files is off");
        check(fixture.registry.active(protocol::ModuleId::parties), "parties, being core, is not");

        modules::Call call;
        call.operation = protocol::OperationId::file_search;
        const modules::Outcome outcome = fixture.registry.run(
            *fixture.database, call, owner_session(), engine::ConnectionState::Online);
        check(!outcome.ok, "the operation is refused");
        check(outcome.reason == engine::DenialReason::ModuleInactive,
              "because the shop does not use that module");

        bool core_refused = false;
        try {
            fixture.registry.set_disabled({protocol::ModuleId::parties});
        } catch (const modules::RegistryError&) {
            core_refused = true;
        }
        check(core_refused, "a core module cannot be switched off at all");
    }

    section("staff are read-only offline, and the framework enforces it");
    {
        Fixture fixture;
        fixture.build();

        engine::Session staff = owner_session();
        staff.is_owner = false;

        modules::Call call;
        call.operation = protocol::OperationId::party_create;
        call.record_id = "party-4";
        call.idempotency_key = "key-4";

        const modules::Outcome outcome = fixture.registry.run(
            *fixture.database, call, staff, engine::ConnectionState::Offline);
        check(!outcome.ok, "staff cannot create a party with the line down");
        check(outcome.reason == engine::DenialReason::ReadOnlyOffline, "for the stated reason");

        const modules::Outcome by_owner = fixture.registry.run(
            *fixture.database, call, owner_session(), engine::ConnectionState::Offline);
        check(by_owner.ok, "the owner can");
        check(by_owner.queued, "and it waits in the outbox until the line is back");
    }

    return squiflow::testing::report();
}
