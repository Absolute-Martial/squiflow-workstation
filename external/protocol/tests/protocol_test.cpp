// The protocol spine, checked. Everything here would be a silent bug in a
// running shop, so each one fails the build instead.
//
// Deliberately dependency-free: no test framework, no Qt, no database. It
// compiles and runs anywhere a C++ compiler exists.

#include <squiflow/protocol/module_graph.hpp>
#include <squiflow/protocol/operation_table.hpp>
#include <squiflow/protocol/protocol_version.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

void check(bool condition, const std::string& what) {
    ++g_checks;
    if (!condition) {
        std::cout << "  FAIL  " << what << "\n";
        ++g_failures;
    }
}

using namespace squiflow::protocol;

std::string name_of(ModuleId module) {
    return std::string(module_name(module));
}

bool contains(const std::vector<ModuleId>& list, ModuleId wanted) {
    return std::find(list.begin(), list.end(), wanted) != list.end();
}

ModuleId module_at(std::size_t index) {
    return static_cast<ModuleId>(index);
}

void test_module_graph() {
    std::cout << "module graph\n";

    const GraphCheck result = check_module_graph();
    check(result.ok, "graph is acyclic and core is closed: " + result.problem);

    // Core must be closed under dependency, stated again as a direct test so
    // the reason survives even if check_module_graph is rewritten.
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId module = module_at(i);
        if (module_tier(module) != ModuleTier::Core) {
            continue;
        }
        for (const ModuleId dep : module_requirements(module)) {
            check(module_tier(dep) == ModuleTier::Core,
                  name_of(module) + " is core but requires " + name_of(dep));
        }
    }

    // Every module has a distinct, non-empty name.
    std::vector<std::string_view> names;
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId module = module_at(i);
        check(!module_name(module).empty(),
              "module " + std::to_string(i) + " has a name");
        names.push_back(module_name(module));
    }
    std::sort(names.begin(), names.end());
    check(std::adjacent_find(names.begin(), names.end()) == names.end(),
          "module names are unique");

    check(kModuleCount == 12U, "there are twelve modules");
}

// module_dependents is the reverse of module_requirements. If the two ever
// disagree, the person is told the wrong thing about what goes dark when they
// switch a module off -- which is exactly the moment they are trusting it.
void test_dependents_mirror_requirements() {
    std::cout << "dependents mirror requirements\n";

    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId dependent = module_at(i);
        for (const ModuleId dependency : module_requirements(dependent)) {
            check(contains(module_dependents(dependency), dependent),
                  name_of(dependent) + " requires " + name_of(dependency) +
                      ", so it must appear among its dependents");
        }
    }

    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId dependency = module_at(i);
        for (const ModuleId dependent : module_dependents(dependency)) {
            check(contains(module_requirements(dependent), dependency),
                  name_of(dependent) + " is listed as a dependent of " +
                      name_of(dependency) + ", so it must require it");
        }
    }

    // Nothing may require itself. A self-edge is a cycle the eye skips over.
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId module = module_at(i);
        check(!contains(module_requirements(module), module),
              name_of(module) + " does not require itself");
    }

    // jobs is deliberately dependency-free: a job may exist with no order.
    check(module_requirements(ModuleId::jobs).empty(),
          "jobs requires nothing, so a job can exist with no order");

    // A module this build does not have has no edges, rather than reading
    // past the end of the table.
    check(module_requirements(ModuleId::Count).empty(),
          "an unknown module requires nothing");
    check(module_dependents(ModuleId::Count).empty(),
          "nothing depends on an unknown module");
}

void test_activation() {
    std::cout << "activation closure\n";

    // Nothing switched off: everything is on.
    {
        const ActivationResult all = resolve_activation({});
        check(all.ok, "an empty request succeeds");
        check(all.also_disabled.empty(),
              "an empty request switches nothing off as a consequence");
        for (std::size_t i = 0; i < kModuleCount; ++i) {
            const ModuleId module = module_at(i);
            check(all.activation.is_active(module),
                  name_of(module) + " is active by default");
        }
    }

    // Core cannot be switched off, and the refusal names the module.
    {
        const ActivationResult refused = resolve_activation({ModuleId::pricing});
        check(!refused.ok, "switching off a core module is refused");
        check(refused.error.find("pricing") != std::string::npos,
              "the refusal names the module");
    }

    // Every core module is refused, not just the one that was spot-checked.
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId module = module_at(i);
        if (module_tier(module) != ModuleTier::Core) {
            continue;
        }
        const ActivationResult refused = resolve_activation({module});
        check(!refused.ok, name_of(module) + " is core and cannot be switched off");
        check(!refused.error.empty(),
              "refusing " + name_of(module) + " comes with a reason");
    }

    // Switching off an extra leaves core untouched.
    {
        const ActivationResult r = resolve_activation({ModuleId::agreements});
        check(r.ok, "switching off an extra succeeds");
        check(!r.activation.is_active(ModuleId::agreements), "agreements is off");
        check(r.activation.is_active(ModuleId::receivables),
              "receivables is unaffected");
        check(r.activation.is_active(ModuleId::pricing), "pricing is unaffected");
    }

    // Every extra can be switched off on its own, and doing so never disturbs
    // a core module.
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId module = module_at(i);
        if (module_tier(module) != ModuleTier::Extra) {
            continue;
        }
        const ActivationResult r = resolve_activation({module});
        check(r.ok, name_of(module) + " is extra and can be switched off");
        check(!r.activation.is_active(module), name_of(module) + " is off");
        for (std::size_t j = 0; j < kModuleCount; ++j) {
            const ModuleId other = module_at(j);
            if (module_tier(other) == ModuleTier::Core) {
                check(r.activation.is_active(other),
                      "switching off " + name_of(module) + " left core module " +
                          name_of(other) + " alone");
            }
        }
    }

    // Anything switched off as a consequence must genuinely be extra, must
    // actually be off, and must not be the module that was asked for.
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        const ModuleId module = module_at(i);
        if (module_tier(module) != ModuleTier::Extra) {
            continue;
        }
        const ActivationResult r = resolve_activation({module});
        for (const ModuleId knocked_out : r.also_disabled) {
            check(module_tier(knocked_out) == ModuleTier::Extra,
                  "a core module was never switched off as a consequence");
            check(!r.activation.is_active(knocked_out),
                  name_of(knocked_out) + " is reported off and is off");
            check(knocked_out != module,
                  "the module asked for is not also reported as a consequence");
        }
    }

    // All extras at once. Every extra off, every core on.
    {
        std::vector<ModuleId> extras;
        for (std::size_t i = 0; i < kModuleCount; ++i) {
            const ModuleId module = module_at(i);
            if (module_tier(module) == ModuleTier::Extra) {
                extras.push_back(module);
            }
        }
        const ActivationResult r = resolve_activation(extras);
        check(r.ok, "switching off every extra at once succeeds");
        for (std::size_t i = 0; i < kModuleCount; ++i) {
            const ModuleId module = module_at(i);
            const bool expected = module_tier(module) == ModuleTier::Core;
            check(r.activation.is_active(module) == expected,
                  name_of(module) + " has the expected state with all extras off");
        }
    }

    // Asking for the same module twice is the same as asking once. A person
    // clicking twice must not produce a different shop.
    {
        const ActivationResult once = resolve_activation({ModuleId::sourcing});
        const ActivationResult twice =
            resolve_activation({ModuleId::sourcing, ModuleId::sourcing});
        check(twice.ok, "a duplicated request succeeds");
        for (std::size_t i = 0; i < kModuleCount; ++i) {
            const ModuleId module = module_at(i);
            check(once.activation.is_active(module) ==
                      twice.activation.is_active(module),
                  "asking twice for " + name_of(module) + " changes nothing");
        }
    }

    // One core module anywhere in the list poisons the whole request, rather
    // than being quietly dropped while the rest goes through.
    {
        const ActivationResult mixed =
            resolve_activation({ModuleId::sourcing, ModuleId::catalog});
        check(!mixed.ok, "a request containing a core module is refused whole");
        check(mixed.activation.is_active(ModuleId::sourcing),
              "a refused request applies none of itself");
    }

    // Today no extra requires another extra, so also_disabled is always empty
    // against the built-in graph. The closure is exercised properly against
    // synthetic graphs below; this records the state of the real data.
    {
        std::size_t extra_to_extra_edges = 0;
        for (std::size_t i = 0; i < kModuleCount; ++i) {
            const ModuleId module = module_at(i);
            if (module_tier(module) != ModuleTier::Extra) {
                continue;
            }
            for (const ModuleId dep : module_requirements(module)) {
                if (module_tier(dep) == ModuleTier::Extra) {
                    ++extra_to_extra_edges;
                }
            }
        }
        check(extra_to_extra_edges == 0U,
              "no extra requires another extra in the shipped graph");
    }
}

// Everything below is a value the type system permits but the data does not
// contain. Each one of these used to crash, corrupt memory, or answer with
// whatever happened to sit past the end of a table.
void test_values_from_outside() {
    std::cout << "values from outside this build\n";

    // Validity predicates.
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        check(is_valid(module_at(i)),
              "module " + std::to_string(i) + " is valid");
    }
    check(!is_valid(ModuleId::Count), "the module sentinel is not a module");
    check(!is_valid(static_cast<ModuleId>(kModuleCount + 1)),
          "a number past the end is not a module");
    check(!is_valid(static_cast<ModuleId>(255)), "255 is not a module");

    for (std::size_t i = 0; i < kRightCount; ++i) {
        check(is_valid(static_cast<RightId>(i)),
              "right " + std::to_string(i) + " is valid");
    }
    check(!is_valid(RightId::Count), "the right sentinel is not a right");
    check(!is_valid(static_cast<RightId>(65535)), "65535 is not a right");

    for (std::size_t i = 0; i < kOperationCount; ++i) {
        check(is_valid(static_cast<OperationId>(i)),
              "operation " + std::to_string(i) + " is valid");
    }
    check(!is_valid(OperationId::Count), "the operation sentinel is not an operation");
    check(!is_valid(static_cast<OperationId>(9999)), "9999 is not an operation");
    check(!is_valid(static_cast<OperationId>(65535)), "65535 is not an operation");

    // Numbers off the wire. A device on a newer build can name an operation
    // this one has never heard of; that must come back as nothing, not as a
    // read past the end of the table.
    for (std::size_t i = 0; i < kOperationCount; ++i) {
        const OperationInfo* found = find_operation(static_cast<std::uint32_t>(i));
        check(found != nullptr, "operation number " + std::to_string(i) + " resolves");
        if (found != nullptr) {
            check(static_cast<std::size_t>(found->id) == i,
                  "operation number " + std::to_string(i) + " resolves to itself");
            check(find_operation(found->name) == found,
                  "by number and by name agree for " + std::string(found->name));
        }
    }
    check(find_operation(static_cast<std::uint32_t>(kOperationCount)) == nullptr,
          "one past the last operation number is refused");
    check(find_operation(static_cast<std::uint32_t>(kOperationCount + 1)) == nullptr,
          "two past the last operation number is refused");
    check(find_operation(std::uint32_t{9999}) == nullptr,
          "a far out of range operation number is refused");
    check(find_operation(std::uint32_t{0xFFFFFFFFU}) == nullptr,
          "the largest possible number is refused");

    // try_operation is the non-aborting form of operation().
    for (std::size_t i = 0; i < kOperationCount; ++i) {
        const OperationId id = static_cast<OperationId>(i);
        const OperationInfo* got = try_operation(id);
        check(got != nullptr, "try_operation resolves a real operation");
        if (got != nullptr) {
            check(got->id == id, "try_operation resolves to the same operation");
            check(&operation(id) == got,
                  "try_operation and operation agree on a valid identifier");
        }
    }
    check(try_operation(OperationId::Count) == nullptr,
          "try_operation refuses the sentinel");
    check(try_operation(static_cast<OperationId>(9999)) == nullptr,
          "try_operation refuses a wire value this build does not know");

    // Turning a stored number back into an identifier.
    ModuleId module_out = ModuleId::administration;
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        check(module_from_number(static_cast<std::uint32_t>(i), module_out),
              "module number " + std::to_string(i) + " converts");
        check(static_cast<std::size_t>(module_out) == i,
              "module number " + std::to_string(i) + " converts to itself");
    }
    const ModuleId untouched = module_out;
    check(!module_from_number(static_cast<std::uint32_t>(kModuleCount), module_out),
          "one past the last module number is refused");
    check(module_out == untouched, "a refused conversion leaves the output alone");
    check(!module_from_number(std::uint32_t{0xFFFFFFFFU}, module_out),
          "the largest possible module number is refused");

    RightId right_out = static_cast<RightId>(0);
    for (std::size_t i = 0; i < kRightCount; ++i) {
        check(right_from_number(static_cast<std::uint32_t>(i), right_out),
              "right number " + std::to_string(i) + " converts");
        check(static_cast<std::size_t>(right_out) == i,
              "right number " + std::to_string(i) + " converts to itself");
    }
    check(!right_from_number(static_cast<std::uint32_t>(kRightCount), right_out),
          "one past the last right number is refused");
    check(!right_from_number(std::uint32_t{0xFFFFFFFFU}, right_out),
          "the largest possible right number is refused");

    // Asking whether a module this build does not have is active. This read
    // used to run past the end of the activation array.
    {
        const ActivationResult all = resolve_activation({});
        check(!all.activation.is_active(ModuleId::Count),
              "the module sentinel is not active");
        check(!all.activation.is_active(static_cast<ModuleId>(200)),
              "a module this build does not have is not active");
    }

    // Asking to switch off a module this build does not have. This used to
    // write past the end of the activation array and report success.
    {
        const ActivationResult r = resolve_activation({static_cast<ModuleId>(50)});
        check(!r.ok, "switching off an unknown module is refused");
        check(!r.error.empty(), "the refusal comes with a reason");
        check(r.also_disabled.empty(), "a refused request knocks nothing out");
    }
    {
        const ActivationResult r = resolve_activation({ModuleId::Count});
        check(!r.ok, "switching off the module sentinel is refused");
    }
    {
        const ActivationResult r = resolve_activation({static_cast<ModuleId>(255)});
        check(!r.ok, "switching off module 255 is refused");
    }
    // A bad entry after a good one still refuses the whole request, and the
    // good one is not left half-applied.
    {
        const ActivationResult r =
            resolve_activation({ModuleId::jobs, static_cast<ModuleId>(99)});
        check(!r.ok, "one unknown module refuses the whole request");
        check(r.activation.is_active(ModuleId::jobs),
              "the valid module in a refused request stays active");
    }
}

// The checks themselves, run against graphs built for the purpose. Until now
// check_graph had only ever seen one graph, and that graph a correct one, so
// every refusal it can produce was unreachable code.
void test_the_checks_themselves() {
    std::cout << "the graph checks, against graphs built to break them\n";

    const auto view = [](std::span<const ModuleTier> tiers,
                         std::span<const Edge> edges) {
        return GraphView{tiers, edges};
    };

    // A graph with nothing in it is vacuously fine.
    {
        const GraphCheck r = check_graph(view({}, {}));
        check(r.ok, "an empty graph passes: " + r.problem);
    }

    // Modules but no edges.
    {
        const std::array<ModuleTier, 3> tiers{ModuleTier::Core, ModuleTier::Extra,
                                              ModuleTier::Extra};
        const GraphCheck r = check_graph(view(tiers, {}));
        check(r.ok, "modules with no edges pass: " + r.problem);
    }

    // A plain correct graph.
    {
        const std::array<ModuleTier, 3> tiers{ModuleTier::Core, ModuleTier::Core,
                                              ModuleTier::Extra};
        const std::array<Edge, 2> edges{Edge{module_at(1), module_at(0)},
                                        Edge{module_at(2), module_at(1)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(r.ok, "a correct graph passes: " + r.problem);
    }

    // A module requiring itself.
    {
        const std::array<ModuleTier, 2> tiers{ModuleTier::Core, ModuleTier::Core};
        const std::array<Edge, 1> edges{Edge{module_at(0), module_at(0)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(!r.ok, "a module requiring itself is refused");
        check(r.problem.find("itself") != std::string::npos,
              "the refusal says the module requires itself: " + r.problem);
    }

    // Two modules requiring each other.
    {
        const std::array<ModuleTier, 2> tiers{ModuleTier::Core, ModuleTier::Core};
        const std::array<Edge, 2> edges{Edge{module_at(0), module_at(1)},
                                        Edge{module_at(1), module_at(0)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(!r.ok, "a two module cycle is refused");
        check(r.problem.find("cycle") != std::string::npos,
              "the refusal calls it a cycle: " + r.problem);
    }

    // A longer cycle, with an innocent module attached to it. The innocent one
    // must not be named as part of the cycle.
    {
        const std::array<ModuleTier, 4> tiers{ModuleTier::Core, ModuleTier::Core,
                                              ModuleTier::Core, ModuleTier::Core};
        const std::array<Edge, 4> edges{Edge{module_at(0), module_at(1)},
                                        Edge{module_at(1), module_at(2)},
                                        Edge{module_at(2), module_at(0)},
                                        Edge{module_at(3), module_at(0)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(!r.ok, "a three module cycle is refused");

        // The report has two halves: what is in the cycle, and what is merely
        // stuck behind it. Module 3 requires the cycle but is not part of it,
        // so it must be reported as affected without being accused.
        const std::string cycle_part = r.problem.substr(0, r.problem.find(';'));
        check(cycle_part.find(name_of(module_at(3))) == std::string::npos,
              "a module merely attached to a cycle is not named as part of it: " +
                  r.problem);
        check(r.problem.find(name_of(module_at(3))) != std::string::npos,
              "but it is still reported as unresolvable: " + r.problem);
        for (std::size_t i = 0; i < 3; ++i) {
            check(cycle_part.find(name_of(module_at(i))) != std::string::npos,
                  "the real cycle member " + name_of(module_at(i)) +
                      " is named in the cycle: " + r.problem);
        }
    }

    // An edge pointing at a module the graph does not have.
    {
        const std::array<ModuleTier, 2> tiers{ModuleTier::Core, ModuleTier::Core};
        const std::array<Edge, 1> edges{Edge{module_at(0), module_at(7)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(!r.ok, "an edge pointing at a missing module is refused");
    }

    // An edge starting at a module the graph does not have.
    {
        const std::array<ModuleTier, 2> tiers{ModuleTier::Core, ModuleTier::Core};
        const std::array<Edge, 1> edges{Edge{module_at(9), module_at(0)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(!r.ok, "an edge starting at a missing module is refused");
    }

    // Core requiring extra: the rule the whole tier system rests on.
    {
        const std::array<ModuleTier, 2> tiers{ModuleTier::Core, ModuleTier::Extra};
        const std::array<Edge, 1> edges{Edge{module_at(0), module_at(1)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(!r.ok, "a core module requiring an extra is refused");
        check(r.problem.find("core") != std::string::npos,
              "the refusal explains the tier problem: " + r.problem);
    }

    // Extra requiring core is the normal, allowed direction.
    {
        const std::array<ModuleTier, 2> tiers{ModuleTier::Core, ModuleTier::Extra};
        const std::array<Edge, 1> edges{Edge{module_at(1), module_at(0)}};
        const GraphCheck r = check_graph(view(tiers, edges));
        check(r.ok, "an extra requiring a core module passes: " + r.problem);
    }

    // More modules than an activation can hold.
    {
        std::array<ModuleTier, kModuleCount + 1> tiers{};
        tiers.fill(ModuleTier::Extra);
        const GraphCheck r = check_graph(view(tiers, {}));
        check(!r.ok, "a graph too wide for an activation is refused");
    }

    // Exactly as many modules as an activation can hold is still fine.
    {
        std::array<ModuleTier, kModuleCount> tiers{};
        tiers.fill(ModuleTier::Extra);
        const GraphCheck r = check_graph(view(tiers, {}));
        check(r.ok, "a graph exactly as wide as an activation passes: " + r.problem);
    }
}

// The deactivation closure, finally given data that exercises it. The shipped
// graph has no extra requiring an extra, so none of this has ever run against
// real tables.
void test_closure_against_synthetic_graphs() {
    std::cout << "deactivation closure, transitively\n";

    // Three extras in a chain: 0 requires 1, 1 requires 2. Switching off 2
    // must take 1 and then 0 with it.
    const std::array<ModuleTier, 4> tiers{ModuleTier::Extra, ModuleTier::Extra,
                                          ModuleTier::Extra, ModuleTier::Core};
    const std::array<Edge, 2> chain{Edge{module_at(0), module_at(1)},
                                    Edge{module_at(1), module_at(2)}};
    const GraphView graph{tiers, chain};

    check(check_graph(graph).ok, "the synthetic chain is a valid graph");

    {
        const ActivationResult r = resolve_activation_in(graph, {module_at(2)});
        check(r.ok, "switching off the end of the chain succeeds");
        check(!r.activation.is_active(module_at(2)), "the requested module is off");
        check(!r.activation.is_active(module_at(1)),
              "the module that needs it is off too");
        check(!r.activation.is_active(module_at(0)),
              "and the module that needs that one, transitively");
        check(r.activation.is_active(module_at(3)),
              "the unrelated core module is untouched");

        check(r.also_disabled.size() == 2U,
              "two modules are reported as consequences, not " +
                  std::to_string(r.also_disabled.size()));
        check(contains(r.also_disabled, module_at(1)),
              "the direct dependent is reported");
        check(contains(r.also_disabled, module_at(0)),
              "the indirect dependent is reported");
        check(!contains(r.also_disabled, module_at(2)),
              "the module that was asked for is not reported as a consequence");
    }

    // Switching off the middle takes only what is above it.
    {
        const ActivationResult r = resolve_activation_in(graph, {module_at(1)});
        check(r.ok, "switching off the middle of the chain succeeds");
        check(!r.activation.is_active(module_at(0)), "what needs it goes off");
        check(r.activation.is_active(module_at(2)),
              "what it needs stays on: dependencies do not run backwards");
        check(r.also_disabled.size() == 1U, "exactly one consequence");
    }

    // Switching off the top of the chain takes nothing else.
    {
        const ActivationResult r = resolve_activation_in(graph, {module_at(0)});
        check(r.ok, "switching off the top of the chain succeeds");
        check(r.activation.is_active(module_at(1)), "nothing below is disturbed");
        check(r.also_disabled.empty(), "nothing is reported as a consequence");
    }

    // Asking for a module that the closure would have switched off anyway.
    // It must not be reported twice, nor reported as a consequence.
    {
        const ActivationResult r =
            resolve_activation_in(graph, {module_at(2), module_at(1)});
        check(r.ok, "asking for a module the closure would take anyway succeeds");
        check(!r.activation.is_active(module_at(0)), "the closure still runs");
        check(r.also_disabled.size() == 1U,
              "only the module nobody asked for is reported, not " +
                  std::to_string(r.also_disabled.size()));
        check(contains(r.also_disabled, module_at(0)),
              "and it is the right one");
    }

    // A diamond: 0 and 1 both require 2. Switching off 2 takes both.
    {
        const std::array<ModuleTier, 3> diamond_tiers{
            ModuleTier::Extra, ModuleTier::Extra, ModuleTier::Extra};
        const std::array<Edge, 2> diamond{Edge{module_at(0), module_at(2)},
                                          Edge{module_at(1), module_at(2)}};
        const GraphView d{diamond_tiers, diamond};
        const ActivationResult r = resolve_activation_in(d, {module_at(2)});
        check(r.ok, "switching off a shared dependency succeeds");
        check(r.also_disabled.size() == 2U, "both dependents are reported");
        check(!r.activation.is_active(module_at(0)), "the first dependent is off");
        check(!r.activation.is_active(module_at(1)), "the second dependent is off");
    }

    // A core module anywhere in a synthetic graph is still refused.
    {
        const ActivationResult r = resolve_activation_in(graph, {module_at(3)});
        check(!r.ok, "a core module in a synthetic graph is still refused");
    }

    // A module outside the synthetic graph's own range is refused, even though
    // it is a perfectly valid module in the real build.
    {
        const ActivationResult r = resolve_activation_in(graph, {module_at(9)});
        check(!r.ok, "a module outside this graph's range is refused");
        check(r.activation.is_active(module_at(0)),
              "a refused request against a synthetic graph applies nothing");
    }
}

void test_rights() {
    std::cout << "rights\n";

    check(kRightCount == 44U, "there are forty-four rights");

    std::vector<std::string_view> names;
    for (std::size_t i = 0; i < kRightCount; ++i) {
        const auto right = static_cast<RightId>(i);
        check(!right_name(right).empty(),
              "right " + std::to_string(i) + " has a name");
        check(is_valid(right_module(right)),
              std::string(right_name(right)) + " is owned by a real module");
        names.push_back(right_name(right));
    }

    // A duplicate right name is a permission that can be granted twice and
    // revoked once.
    std::sort(names.begin(), names.end());
    const auto duplicate = std::adjacent_find(names.begin(), names.end());
    check(duplicate == names.end(),
          duplicate == names.end()
              ? "right names are unique"
              : "duplicate right name: " + std::string(*duplicate));
}

void test_operations() {
    std::cout << "operation table\n";

    check(kOperationCount == 67U, "there are sixty-seven operations");

    // The table and the enum must stay in the same order, or every lookup by
    // identifier silently returns the wrong operation.
    const auto ops = all_operations();
    check(ops.size() == kOperationCount, "the table holds every operation");
    for (std::size_t i = 0; i < ops.size(); ++i) {
        check(static_cast<std::size_t>(ops[i].id) == i,
              "operation at index " + std::to_string(i) +
                  " carries the matching identifier");
    }

    // Names are unique. A duplicate would make the sync router ambiguous.
    std::vector<std::string_view> names;
    for (const OperationInfo& info : ops) {
        check(!info.name.empty(), "every operation has a name");
        names.push_back(info.name);
    }
    std::sort(names.begin(), names.end());
    const auto duplicate = std::adjacent_find(names.begin(), names.end());
    check(duplicate == names.end(),
          duplicate == names.end()
              ? "operation names are unique"
              : "duplicate operation name: " + std::string(*duplicate));

    for (const OperationInfo& info : ops) {
        const std::string where = std::string(info.name) + ": ";

        // An operation that must reach the server cannot be allowed offline.
        if (info.sync_class == OperationClass::OnlineRequired) {
            check(info.offline == OfflineRule::OnlineOnly,
                  where + "OnlineRequired must be OnlineOnly");
        }

        // Something that never leaves the machine has no reason to demand a
        // connection.
        if (info.sync_class == OperationClass::LocalOnly) {
            check(info.offline == OfflineRule::OfflineAllowed,
                  where + "LocalOnly should be OfflineAllowed");
        }

        // The right and module must exist. The enum makes that a compile-time
        // fact; this checks the tables agree at run time.
        check(is_valid(info.right), where + "right is in range");
        check(is_valid(info.module), where + "module is in range");
        check(!right_name(info.right).empty(), where + "right has a name");

        // allowed_offline is the one answer the rest of the program asks for.
        // It must agree with the table it is derived from, or the screens and
        // the data will disagree about the same operation.
        check(allowed_offline(info.id) ==
                  (info.offline == OfflineRule::OfflineAllowed),
              where + "allowed_offline agrees with the table");

        // Round trip through the enum.
        check(operation(info.id).name == info.name,
              where + "lookup by identifier returns the same operation");
    }

    // Spot checks on decisions that were argued about, so a later edit that
    // quietly reverses one gets caught.
    check(!allowed_offline(OperationId::cancel_and_reissue),
          "cancelling an issued invoice is online-only");
    check(allowed_offline(OperationId::counter_sale),
          "a counter sale works with no connection");
    check(allowed_offline(OperationId::document_print),
          "printing works with no connection");
    check(operation(OperationId::module_activation_set).sync_class ==
              OperationClass::OnlineRequired,
          "module activation is shop-wide, so it needs the server");
}

// Lookup by name is how a sync payload from the server is resolved. It is the
// one entry point that is handed a string this build may never have seen, so
// it has to refuse rather than guess.
void test_lookup_by_name() {
    std::cout << "lookup by name\n";

    for (const OperationInfo& info : all_operations()) {
        const OperationInfo* found = find_operation(info.name);
        check(found != nullptr,
              std::string(info.name) + " is found by its own name");
        if (found != nullptr) {
            check(found->id == info.id,
                  std::string(info.name) + " resolves to itself, not a neighbour");
        }
    }

    check(find_operation("") == nullptr, "an empty name is refused");
    check(find_operation("no_such_operation") == nullptr,
          "an unknown name is refused");
    check(find_operation("counter_sale ") == nullptr,
          "a trailing space is not silently trimmed");
    check(find_operation(" counter_sale") == nullptr,
          "a leading space is not silently trimmed");
    check(find_operation("Counter_Sale") == nullptr,
          "the lookup is case sensitive");
    check(find_operation("counter") == nullptr,
          "a prefix of a real name is not a match");
    check(find_operation("counter_sale_extra") == nullptr,
          "a real name with something appended is not a match");

    // A name containing an embedded null must not match by accident. The
    // string_view carries its own length, and this proves the lookup respects
    // it rather than reading to the first null.
    const std::string embedded("counter_sale\0x", 14);
    check(find_operation(std::string_view(embedded)) == nullptr,
          "a name with an embedded null is refused");
}

// Staff are read-only when the device is offline. staff_offline.def lists the
// few exceptions and states that a test enforces that every entry is also
// OfflineAllowed. Until now that test did not exist.
void test_staff_offline_exceptions() {
    std::cout << "staff offline exceptions\n";

    std::size_t exceptions = 0;
    for (const OperationInfo& info : all_operations()) {
        if (!staff_offline_exception(info.id)) {
            continue;
        }
        ++exceptions;

        // The invariant staff_offline.def claims is enforced. An exception
        // that its own operations file marks online-only would let a staff
        // member start something at the counter that cannot be completed.
        check(info.offline == OfflineRule::OfflineAllowed,
              std::string(info.name) +
                  " is a staff offline exception, so it must be OfflineAllowed");
        check(info.sync_class != OperationClass::OnlineRequired,
              std::string(info.name) +
                  " is a staff offline exception, so it cannot be OnlineRequired");
        check(allowed_offline(info.id),
              std::string(info.name) +
                  " is a staff offline exception and is allowed offline");
    }

    check(exceptions == 5U, "there are five staff offline exceptions");

    // The five, named. A silent removal would quietly turn a customer away.
    check(staff_offline_exception(OperationId::counter_sale),
          "a staff member can take a counter sale offline");
    check(staff_offline_exception(OperationId::take_payment),
          "a staff member can take a payment offline");
    check(staff_offline_exception(OperationId::document_print),
          "a staff member can print offline");
    check(staff_offline_exception(OperationId::purchase_lookup),
          "a staff member can look up a purchase offline");
    check(staff_offline_exception(OperationId::file_search),
          "a staff member can search files offline");

    // The rule is an exception list, not a general permission. Something that
    // demands the server is never on it.
    for (const OperationInfo& info : all_operations()) {
        if (info.sync_class == OperationClass::OnlineRequired) {
            check(!staff_offline_exception(info.id),
                  std::string(info.name) +
                      " needs the server, so it is not a staff exception");
        }
    }

    check(!staff_offline_exception(OperationId::cancel_and_reissue),
          "cancelling an issued invoice is not a counter exception");

    // Safe for a value this build does not know: not an exception, which is
    // the refusing answer.
    check(!staff_offline_exception(OperationId::Count),
          "the sentinel is not a staff exception");
    check(!staff_offline_exception(static_cast<OperationId>(9999)),
          "an unknown wire value is not a staff exception");
}

void test_enum_text() {
    std::cout << "enum text\n";

    check(to_string(OperationClass::LocalOnly) == "LocalOnly", "LocalOnly prints");
    check(to_string(OperationClass::Synchronizable) == "Synchronizable",
          "Synchronizable prints");
    check(to_string(OperationClass::OnlineRequired) == "OnlineRequired",
          "OnlineRequired prints");
    check(to_string(OfflineRule::OfflineAllowed) == "OfflineAllowed",
          "OfflineAllowed prints");
    check(to_string(OfflineRule::OnlineOnly) == "OnlineOnly", "OnlineOnly prints");

    // A value outside the enumeration prints a marker rather than reading
    // past the end of a table.
    check(to_string(static_cast<OperationClass>(200)) == "?",
          "an unknown operation class prints a marker");
    check(to_string(static_cast<OfflineRule>(200)) == "?",
          "an unknown offline rule prints a marker");
}

void summarize() {
    std::cout << "\nsummary\n";
    std::cout << "  wire version   " << kWireVersionString << "\n";
    std::cout << "  modules        " << kModuleCount << "\n";
    std::cout << "  rights         " << kRightCount << "\n";
    std::cout << "  operations     " << kOperationCount << "\n";

    std::size_t offline_ok = 0;
    for (const OperationInfo& info : all_operations()) {
        if (info.offline == OfflineRule::OfflineAllowed) {
            ++offline_ok;
        }
    }
    std::cout << "  usable offline " << offline_ok << " of " << kOperationCount
              << "\n";

    std::size_t core = 0;
    for (std::size_t i = 0; i < kModuleCount; ++i) {
        if (module_tier(module_at(i)) == ModuleTier::Core) {
            ++core;
        }
    }
    std::cout << "  core modules   " << core << ", extra "
              << (kModuleCount - core) << "\n";
}

}  // namespace

int main() {
    test_module_graph();
    test_dependents_mirror_requirements();
    test_activation();
    test_values_from_outside();
    test_the_checks_themselves();
    test_closure_against_synthetic_graphs();
    test_rights();
    test_operations();
    test_lookup_by_name();
    test_staff_offline_exceptions();
    test_enum_text();
    summarize();

    std::cout << "\n" << g_checks << " checks, " << g_failures << " failed\n";
    return g_failures == 0 ? 0 : 1;
}
