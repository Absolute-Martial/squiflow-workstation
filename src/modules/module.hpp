#pragma once

// What a module is, in code.
//
// A module owns business entities - parties, invoices, jobs. That is the whole
// distinction between a module and an engine mechanism: a mechanism owns
// nothing and is borrowed by everybody.
//
// A module declares three things and nothing else:
//
//   1. which module it is, from the protocol enumeration;
//   2. the migrations that create and change its tables;
//   3. the handlers for its operations.
//
// It does not declare its own tier or its own dependencies. Those live in the
// protocol .def files, shared with the server, so that the workstation and the
// server cannot disagree about what "orders requires pricing" means. A module
// that could name its own dependencies in C++ would be a module that could
// quietly grant itself one.

#include <memory>
#include <string_view>
#include <vector>

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/module_id.hpp>

#include "engine/storage/migration_runner.hpp"

namespace squiflow::modules {

class Registry;

class Module {
public:
    Module() = default;
    Module(const Module&) = delete;
    Module& operator=(const Module&) = delete;
    Module(Module&&) = delete;
    Module& operator=(Module&&) = delete;
    virtual ~Module();

    // Which module this is. Everything else about it - its name, its tier, its
    // dependencies, its rights, its operations - is looked up from this.
    virtual protocol::ModuleId id() const noexcept = 0;

    // Schema, in order. Numbers are global across the application rather than
    // per module, so two modules written in the same week cannot both claim
    // migration 7 and leave the order of application to chance.
    virtual std::vector<engine::Migration> migrations() const = 0;

    // Register operation handlers. Called once, at startup, before the
    // database is open.
    virtual void install(Registry& registry) = 0;

    std::string_view name() const noexcept { return protocol::module_name(id()); }
    protocol::ModuleTier tier() const noexcept { return protocol::module_tier(id()); }
    bool is_core() const noexcept { return tier() == protocol::ModuleTier::Core; }
};

using ModulePtr = std::unique_ptr<Module>;

}  // namespace squiflow::modules
