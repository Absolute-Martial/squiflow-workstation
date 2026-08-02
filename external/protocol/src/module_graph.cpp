#include <squiflow/protocol/module_graph.hpp>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>

namespace squiflow::protocol {
namespace {

constexpr Edge kEdges[] = {
#define SQF_REQUIRES(a, b) Edge{ModuleId::a, ModuleId::b},
#include <squiflow/protocol/module_requires.def>
#undef SQF_REQUIRES
};

constexpr std::string_view kNames[] = {
#define SQF_MODULE(name, tier) #name,
#include <squiflow/protocol/modules.def>
#undef SQF_MODULE
};

constexpr ModuleTier kTiers[] = {
#define SQF_MODULE(name, tier) ModuleTier::tier,
#include <squiflow/protocol/modules.def>
#undef SQF_MODULE
};

static_assert(std::size(kNames) == kModuleCount,
              "the name table and the module enum disagree");
static_assert(std::size(kTiers) == kModuleCount,
              "the tier table and the module enum disagree");

constexpr std::size_t index_of(ModuleId module) noexcept {
    return static_cast<std::size_t>(module);
}

// A module identifier that is not one of ours has already been used to index
// something by the time it gets here, or is about to be. Stopping is the only
// honest response: the alternative is reading whatever happens to sit past the
// end of the table and treating it as a permission or a tier.
[[noreturn]] void fail(const char* what, std::size_t index) noexcept {
    std::fprintf(stderr, "squiflow protocol: %s (value %zu, this build has %zu)\n",
                 what, index, kModuleCount);
    std::abort();
}

}  // namespace

GraphView builtin_graph() noexcept {
    return GraphView{std::span<const ModuleTier>(kTiers, std::size(kTiers)),
                     std::span<const Edge>(kEdges, std::size(kEdges))};
}

std::string_view module_name(ModuleId module) noexcept {
    if (!is_valid(module)) {
        fail("module_name on a module this build does not have", index_of(module));
    }
    return kNames[index_of(module)];
}

ModuleTier module_tier(ModuleId module) noexcept {
    if (!is_valid(module)) {
        fail("module_tier on a module this build does not have", index_of(module));
    }
    return kTiers[index_of(module)];
}

std::vector<ModuleId> module_requirements(ModuleId module) {
    std::vector<ModuleId> out;
    if (!is_valid(module)) {
        return out;
    }
    for (const Edge& edge : kEdges) {
        if (edge.from == module) {
            out.push_back(edge.to);
        }
    }
    return out;
}

std::vector<ModuleId> module_dependents(ModuleId module) {
    std::vector<ModuleId> out;
    if (!is_valid(module)) {
        return out;
    }
    for (const Edge& edge : kEdges) {
        if (edge.to == module) {
            out.push_back(edge.from);
        }
    }
    return out;
}

GraphCheck check_graph(const GraphView& graph) {
    const std::size_t count = graph.tiers.size();

    // An Activation is exactly kModuleCount wide. A graph with more modules
    // than that cannot be represented, so it is refused before anything
    // indexes into it.
    if (count > kModuleCount) {
        return {false, "graph has " + std::to_string(count) +
                           " modules, more than the " +
                           std::to_string(kModuleCount) + " an activation holds"};
    }

    // Every edge must point at a module the graph actually has. This is
    // checked first, because every check after it indexes by these endpoints.
    for (const Edge& edge : graph.edges) {
        if (index_of(edge.from) >= count) {
            return {false, "edge starts at module " +
                               std::to_string(index_of(edge.from)) +
                               ", which this graph does not have"};
        }
        if (index_of(edge.to) >= count) {
            return {false, "edge points at module " +
                               std::to_string(index_of(edge.to)) +
                               ", which this graph does not have"};
        }
    }

    auto name_at = [count](ModuleId module) -> std::string {
        const std::size_t index = index_of(module);
        if (index < count && index < kModuleCount) {
            return std::string(kNames[index]);
        }
        return std::to_string(index);
    };

    // A module requiring itself is a cycle of length one. The removal pass
    // below would catch it, but naming it plainly is more use than reporting
    // it as an unresolvable group.
    for (const Edge& edge : graph.edges) {
        if (edge.from == edge.to) {
            return {false, "module requires itself: " + name_at(edge.from)};
        }
    }

    // Core closure. A core module requiring an extra would break something
    // that cannot be switched off, so this is fatal rather than advisory.
    for (const Edge& edge : graph.edges) {
        if (graph.tiers[index_of(edge.from)] == ModuleTier::Core &&
            graph.tiers[index_of(edge.to)] == ModuleTier::Extra) {
            return {false, name_at(edge.from) + " is core but requires extra module " +
                               name_at(edge.to)};
        }
    }

    // Acyclicity, by repeatedly removing modules with no unsatisfied
    // requirement. Anything left over is in a cycle.
    std::array<bool, kModuleCount> removed{};
    std::size_t remaining = count;

    bool progress = true;
    while (remaining > 0 && progress) {
        progress = false;
        for (std::size_t i = 0; i < count; ++i) {
            if (removed[i]) {
                continue;
            }
            bool blocked = false;
            for (const Edge& edge : graph.edges) {
                if (index_of(edge.from) == i && !removed[index_of(edge.to)]) {
                    blocked = true;
                    break;
                }
            }
            if (!blocked) {
                removed[i] = true;
                --remaining;
                progress = true;
            }
        }
    }

    if (remaining > 0) {
        // Everything still standing is unresolvable, but not all of it is in
        // a cycle. A module that merely requires something caught in one is
        // stuck too, and naming it as part of the cycle sends whoever is
        // debugging this to the wrong file. A module is genuinely in a cycle
        // only if it can reach itself through modules that also survived.
        auto in_a_cycle = [&](std::size_t start) {
            std::array<bool, kModuleCount> seen{};
            std::vector<std::size_t> pending;
            for (const Edge& edge : graph.edges) {
                if (index_of(edge.from) == start && !removed[index_of(edge.to)]) {
                    pending.push_back(index_of(edge.to));
                }
            }
            while (!pending.empty()) {
                const std::size_t here = pending.back();
                pending.pop_back();
                if (here == start) {
                    return true;
                }
                if (seen[here]) {
                    continue;
                }
                seen[here] = true;
                for (const Edge& edge : graph.edges) {
                    if (index_of(edge.from) == here && !removed[index_of(edge.to)]) {
                        pending.push_back(index_of(edge.to));
                    }
                }
            }
            return false;
        };

        std::string cycle;
        std::string stuck_behind;
        for (std::size_t i = 0; i < count; ++i) {
            if (removed[i]) {
                continue;
            }
            std::string& target = in_a_cycle(i) ? cycle : stuck_behind;
            if (!target.empty()) {
                target += ", ";
            }
            target += name_at(static_cast<ModuleId>(i));
        }

        std::string problem = cycle.empty()
                                  ? "dependencies cannot be ordered among: " + stuck_behind
                                  : "dependency cycle among: " + cycle;
        if (!cycle.empty() && !stuck_behind.empty()) {
            problem += "; also unresolvable because they require it: " + stuck_behind;
        }
        return {false, problem};
    }

    return {true, {}};
}

GraphCheck check_module_graph() {
    return check_graph(builtin_graph());
}

ActivationResult resolve_activation_in(const GraphView& graph,
                                       const std::vector<ModuleId>& disabled) {
    ActivationResult result;
    const std::size_t count = std::min(graph.tiers.size(), kModuleCount);

    result.activation.active.fill(false);
    for (std::size_t i = 0; i < count; ++i) {
        result.activation.active[i] = true;
    }

    // Every requested module is checked before any of them is applied, so a
    // list with one bad entry leaves the activation untouched rather than
    // half-written. The out-of-range case is checked first because the tier
    // lookup that follows would otherwise index past the end.
    for (const ModuleId module : disabled) {
        const std::size_t index = index_of(module);
        if (index >= count) {
            result.ok = false;
            result.error = "no such module in this build: " + std::to_string(index);
            return result;
        }
        if (graph.tiers[index] == ModuleTier::Core) {
            result.ok = false;
            result.error =
                "core module cannot be switched off: " + std::string(kNames[index]);
            return result;
        }
    }

    for (const ModuleId module : disabled) {
        result.activation.active[index_of(module)] = false;
    }

    // Transitive closure. Switching one thing off switches off everything that
    // needs it; the person is told what those are before confirming.
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Edge& edge : graph.edges) {
            const std::size_t from = index_of(edge.from);
            const std::size_t to = index_of(edge.to);
            if (from >= count || to >= count) {
                continue;
            }
            if (result.activation.active[from] && !result.activation.active[to]) {
                result.activation.active[from] = false;
                changed = true;
                const bool asked =
                    std::find(disabled.begin(), disabled.end(), edge.from) !=
                    disabled.end();
                const bool already =
                    std::find(result.also_disabled.begin(),
                              result.also_disabled.end(),
                              edge.from) != result.also_disabled.end();
                if (!asked && !already) {
                    result.also_disabled.push_back(edge.from);
                }
            }
        }
    }

    return result;
}

ActivationResult resolve_activation(const std::vector<ModuleId>& disabled) {
    return resolve_activation_in(builtin_graph(), disabled);
}

}  // namespace squiflow::protocol
