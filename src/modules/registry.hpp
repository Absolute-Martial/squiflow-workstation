#pragma once

// The registry: the one place a request turns into work.
//
// Every operation in the application arrives here - from a screen, from a
// keyboard shortcut, from a workflow, from the sync orchestrator applying
// something the server sent. There is deliberately one door, because the way a
// system ends up permitting through one path what it forbids through another
// is by having two.
//
// What the registry guarantees, so that no module has to remember it:
//
//   * the rules are checked before the handler runs - signed in, module
//     active, right held, connection, offline rule;
//   * a write runs inside exactly one transaction, taken from the writer gate;
//   * a synchronisable change is placed in the outbox in that same
//     transaction. A module cannot forget to queue a change, and a change that
//     rolls back cannot leave a queued copy behind that would resurrect it on
//     the next sync;
//   * a module can only handle its own operations.

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/operation_table.hpp>
#include <squiflow/protocol/workflow_table.hpp>

#include "engine/identity/session.hpp"
#include "engine/storage/database.hpp"
#include "engine/storage/migration_runner.hpp"
#include "engine/sync/conflict.hpp"
#include "engine/sync/cursor.hpp"
#include "engine/sync/outbox.hpp"
#include "modules/context.hpp"
#include "modules/module.hpp"
#include "workflows/definition.hpp"

namespace squiflow::modules {

// Thrown for a mistake in how the application is put together: a module
// registering another module's operation, two handlers for one operation, two
// migrations with one number. These are programming errors found at startup,
// not conditions a person can do anything about, so they are loud and they
// happen before the window is shown.
class RegistryError : public std::runtime_error {
public:
    explicit RegistryError(const std::string& message);
};

class Registry {
public:
    using Clock = std::function<std::int64_t()>;

    // Migration numbers are global across the whole application. The low
    // numbers are reserved for the engine's own tables so that a module can
    // never be the thing that creates the outbox.
    static constexpr int kFirstModuleMigration = 10;

    explicit Registry(Clock clock);

    Registry(const Registry&) = delete;
    Registry& operator=(const Registry&) = delete;
    Registry(Registry&&) = delete;
    Registry& operator=(Registry&&) = delete;
    ~Registry();

    // --- putting the application together, at startup ---

    // Takes ownership and calls install(). Refused twice for the same module.
    void add(ModulePtr module);

    bool has(protocol::ModuleId module) const noexcept;
    std::vector<protocol::ModuleId> registered() const;
    std::size_t size() const noexcept;

    // Called from install(). The operation must belong to the module currently
    // installing, which is why these are not public in spirit even though they
    // have to be in language.
    void on_write(protocol::OperationId operation, WriteHandler handler);
    void on_read(protocol::OperationId operation, ReadHandler handler);
    void install_workflow(workflows::WorkflowDefinition definition);
    bool workflow_available(protocol::OperationId operation) const noexcept;

    bool handled(protocol::OperationId operation) const noexcept;

    // True only for a registered write-kind operation: an ordinary handler
    // installed with on_write, or a workflow (workflows only ever produce
    // writes). A gateway that only ever wants to accept commands can ask this
    // instead of building a Call and finding out from a thrown RegistryError
    // that the operation was a read, or was never handled at all.
    bool is_command(protocol::OperationId operation) const noexcept;

    // Every operation the protocol declares for a registered module, with no
    // handler. A screen would offer these and they would refuse for no reason
    // a person could act on, so startup fails rather than shipping a button
    // that does nothing.
    std::vector<std::string> unhandled() const;
    void require_complete() const;

    // Hands every registered module's migrations to the runner, in number
    // order, refusing a repeated number. Call before the database is opened.
    void collect_migrations(engine::MigrationRunner& runner) const;

    // --- activation ---

    // Activation is computed from the modules switched off, never chosen one
    // by one: switching a module off switches off everything that requires it.
    void set_disabled(const std::vector<protocol::ModuleId>& disabled);
    const protocol::Activation& activation() const noexcept { return activation_; }
    bool active(protocol::ModuleId module) const noexcept;

    // --- doing the work ---

    Outcome run(engine::Database& database, const Call& call,
                const engine::Session& session, engine::ConnectionState connection);

private:
    enum class Kind : std::uint8_t { Read, Write };

    struct Handler {
        Kind kind{Kind::Read};
        protocol::ModuleId owner{};
        WriteHandler write{};
        ReadHandler read{};
    };

    struct WorkflowEntry {
        std::vector<protocol::ModuleId> requirements{};
        workflows::WorkflowHandler handler{};
    };

    void register_handler(protocol::OperationId operation, Handler handler);
    static Outcome refuse(const engine::Decision& decision);

    Clock clock_;
    engine::Outbox outbox_;
    std::vector<ModulePtr> modules_;
    std::map<protocol::OperationId, Handler> handlers_;
    std::map<protocol::OperationId, WorkflowEntry> workflows_;
    protocol::Activation activation_{};

    // Set only while a module's install() is running, so a handler can be
    // attributed to the module that registered it rather than trusted to
    // declare itself honestly.
    bool installing_{false};
    protocol::ModuleId installing_module_{};
};

}  // namespace squiflow::modules
