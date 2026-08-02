#pragma once

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <squiflow/protocol/module_id.hpp>

namespace squiflow::protocol {

// Both abort rather than read out of bounds when handed an invalid module.
// Validate with is_valid or module_from_number at the point the value enters
// the program.
std::string_view module_name(ModuleId module) noexcept;
ModuleTier module_tier(ModuleId module) noexcept;

// Modules this one needs. Direct edges only. Empty for an invalid module.
std::vector<ModuleId> module_requirements(ModuleId module);

// Modules that need this one. Direct edges only. Used to tell a person what
// else goes dark before they confirm switching something off. Empty for an
// invalid module.
std::vector<ModuleId> module_dependents(ModuleId module);

// One dependency edge: from requires to.
struct Edge {
    ModuleId from;
    ModuleId to;
};

// A module graph the checks can be run against. The tables compiled into this
// build are one instance of it, returned by builtin_graph().
//
// This exists so the checks below can be tested. A validator that can only
// ever see one graph, and that graph a correct one, has no proven failure
// behaviour at all: its refusals are unreachable code. Tests supply cycles,
// self-edges, dangling edges and core-requires-extra graphs through here.
//
// tiers is indexed by module, so tiers.size() is how many modules the graph
// has. It must not exceed kModuleCount, because an Activation is that wide.
struct GraphView {
    std::span<const ModuleTier> tiers;
    std::span<const Edge> edges;
};

GraphView builtin_graph() noexcept;

struct GraphCheck {
    bool ok = true;
    std::string problem;
};

// Acyclic, core closed under dependency, every edge points at a real module,
// and no more modules than an Activation can hold.
GraphCheck check_graph(const GraphView& graph);

// The same check applied to this build's own tables.
GraphCheck check_module_graph();

struct Activation {
    std::array<bool, kModuleCount> active{};

    // False for a module this build does not have, rather than reading past
    // the end of the array.
    bool is_active(ModuleId module) const noexcept {
        const std::size_t index = static_cast<std::size_t>(module);
        return index < kModuleCount && active[index];
    }
};

struct ActivationResult {
    bool ok = true;
    std::string error;
    Activation activation;
    // Switched off as a consequence rather than by request. This is the list
    // shown to the person before they confirm.
    std::vector<ModuleId> also_disabled;
};

// Activation is computed, never chosen item by item. Switching a module off
// switches off everything that requires it, transitively. Core cannot be
// switched off at all, and a module this graph does not have is refused
// rather than written past the end of the array.
//
// The whole request is refused or none of it is. A list containing one bad
// entry does not half-apply.
ActivationResult resolve_activation_in(const GraphView& graph,
                                       const std::vector<ModuleId>& disabled);

// The same resolution against this build's own tables.
ActivationResult resolve_activation(const std::vector<ModuleId>& disabled);

}  // namespace squiflow::protocol
