# Header reference — the protocol surface everything builds against

Source page id: 04cc1a90df4f4ef6b9073f1becb20ad4

---

<callout icon="📐">
	Every header the rest of the project includes. Nothing here depends on Qt, on SQLite, or on Windows — which is why this layer can be compiled and executed on any machine, and why the server links the identical files.
</callout>
## `squiflow/protocol/module_id.hpp`
```c++
#pragma once

#include <cstdint>

namespace squiflow::protocol {

enum class ModuleTier : std::uint8_t { Core, Extra };

enum class ModuleId : std::uint8_t {
#define SQF_MODULE(name, tier) name,
#include <squiflow/protocol/modules.def>
#undef SQF_MODULE
    Count
};

inline constexpr std::size_t kModuleCount = static_cast<std::size_t>(ModuleId::Count);

}  // namespace squiflow::protocol
```
### `modules.def` — the whole module list
```c++
// SQF_MODULE(name, tier)

SQF_MODULE(administration, Core)
SQF_MODULE(parties,        Core)
SQF_MODULE(catalog,        Core)
SQF_MODULE(pricing,        Core)
SQF_MODULE(orders,         Core)
SQF_MODULE(receivables,    Core)

SQF_MODULE(jobs,           Extra)
SQF_MODULE(quotations,     Extra)
SQF_MODULE(agreements,     Extra)
SQF_MODULE(sourcing,       Extra)
SQF_MODULE(companion,      Extra)
SQF_MODULE(files,          Extra)
```
### `module_requires.def` — one dependency per line
```c++
// SQF_REQUIRES(dependent, dependency)

SQF_REQUIRES(pricing,     catalog)
SQF_REQUIRES(pricing,     parties)

SQF_REQUIRES(orders,      catalog)
SQF_REQUIRES(orders,      pricing)
SQF_REQUIRES(orders,      parties)

SQF_REQUIRES(receivables, pricing)
SQF_REQUIRES(receivables, parties)

SQF_REQUIRES(quotations,  catalog)
SQF_REQUIRES(quotations,  pricing)
SQF_REQUIRES(quotations,  parties)

SQF_REQUIRES(agreements,  parties)
SQF_REQUIRES(agreements,  pricing)

SQF_REQUIRES(sourcing,    parties)

// jobs deliberately requires nothing: a job may exist with no order at all.
// The order-to-jobs link is a workflow, not a module dependency.
```
---
## `squiflow/protocol/module_graph.hpp`
```c++
#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include <squiflow/protocol/module_id.hpp>

namespace squiflow::protocol {

std::string_view module_name(ModuleId module) noexcept;
ModuleTier module_tier(ModuleId module) noexcept;

std::vector<ModuleId> module_requirements(ModuleId module);

// Modules that need this one. Used to tell a person what else goes dark
// before they confirm switching something off.
std::vector<ModuleId> module_dependents(ModuleId module);

struct GraphCheck {
    bool ok = true;
    std::string problem;
};

// Acyclic, core closed under dependency, every edge points at a real module.
GraphCheck check_module_graph();

struct Activation {
    std::array<bool, kModuleCount> active{};

    bool is_active(ModuleId module) const noexcept {
        return active[static_cast<std::size_t>(module)];
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
// switched off at all.
ActivationResult resolve_activation(const std::vector<ModuleId>& disabled);

}  // namespace squiflow::protocol
```
---
## `squiflow/protocol/operation_table.hpp`
```c++
#pragma once

#include <cstdint>
#include <span>
#include <string_view>

#include <squiflow/protocol/module_id.hpp>
#include <squiflow/protocol/operation_class.hpp>
#include <squiflow/protocol/right_id.hpp>

namespace squiflow::protocol {

enum class OperationId : std::uint16_t {
#define SQF_OPERATION(name, module, right, cls, offline) name,
#include <squiflow/protocol/operations.def>
#undef SQF_OPERATION
    Count
};

inline constexpr std::size_t kOperationCount =
    static_cast<std::size_t>(OperationId::Count);

struct OperationInfo {
    OperationId id;
    std::string_view name;
    ModuleId module;
    RightId right;
    OperationClass sync_class;
    OfflineRule offline;
};

std::span<const OperationInfo> all_operations() noexcept;

const OperationInfo& operation(OperationId id) noexcept;

// Null when no operation carries that name. Used when a sync payload arrives
// naming something this build does not know about.
const OperationInfo* find_operation(std::string_view name) noexcept;

bool allowed_offline(OperationId id) noexcept;
bool staff_offline_exception(OperationId id) noexcept;

}  // namespace squiflow::protocol
```
### `operation_class.hpp`
```c++
// What happens to an operation's effect.
enum class OperationClass : std::uint8_t {
    // Never leaves the machine. Printing a receipt, scanning a folder.
    LocalOnly,
    // Written locally, queued in the outbox, applied on the server later.
    Synchronizable,
    // Must reach the server as it happens. Anything where a local decision
    // could be wrong by the time it arrives.
    OnlineRequired,
};

// Whether a person may do it with no connection.
enum class OfflineRule : std::uint8_t {
    OfflineAllowed,
    OnlineOnly,
};
```
### How an operation is declared — `operations/receivables.def`
```c++
// SQF_OPERATION(name, module, right, sync_class, offline_rule)

SQF_OPERATION(invoice_draft_create,   receivables, right_invoice_draft,         Synchronizable, OfflineAllowed)
SQF_OPERATION(invoice_draft_update,   receivables, right_invoice_draft,         Synchronizable, OfflineAllowed)
SQF_OPERATION(invoice_draft_discard,  receivables, right_invoice_draft,         Synchronizable, OfflineAllowed)
SQF_OPERATION(payment_allocate,       receivables, right_payment_allocate,      Synchronizable, OfflineAllowed)
SQF_OPERATION(credit_account_set,     receivables, right_credit_account_manage, OnlineRequired, OnlineOnly)
SQF_OPERATION(statement_prepare,      receivables, right_statement_send,        LocalOnly,      OfflineAllowed)
SQF_OPERATION(statement_send,         receivables, right_statement_send,        OnlineRequired, OnlineOnly)
SQF_OPERATION(document_print,         receivables, right_document_print,        LocalOnly,      OfflineAllowed)
```
<callout icon="🧱">
	**Why this shape.** One line reaches five consumers at once: the operation enum, the rights binding, the offline table, the sync router, and the completeness test. Because the right and the module are **enumeration members rather than strings**, an operation naming a right that does not exist **fails to compile**. There is no way to typo a permission into existence.
</callout>
---
## Including this from your own code
```javascript
# Anything that includes these headers needs exactly one include path
# and two translation units linked in.

INCLUDES := -Iexternal/protocol/include -Isrc

PROTOCOL_SRC := external/protocol/src/module_graph.cpp \
                external/protocol/src/operation_table.cpp
```
The engine adds `-Isrc` and its own eight source files. Both lists are already wired into `tools/sandbox/Makefile`, which builds and runs both test programs with one command:
```plain text
make -f tools/sandbox/Makefile check
```
