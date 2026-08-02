# Phase 2 — Engine domain: money, lifecycle, numbering, capability (93 checks pass)

Source page id: a597def902aa42bb9e873585da2c7bea

---

<callout icon="✅">
	**Compiled with warnings as errors and executed. 93 checks, 0 failed.** Two real compile errors were hit and fixed on the way: a missing include, and a function the test used that had never been written. Both are recorded below rather than quietly patched.
</callout>
## Verified output
```plain text
== engine ==
record identity
quantity
money
document lifecycle
numbering
signatures and approvals
rights
capability
staff offline exceptions

93 checks, 0 failed
```
---
## `engine/records/money.hpp`
Integer only, checked arithmetic, and rounding that matches what a person does by hand.
```c++
#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "engine/records/quantity.hpp"

namespace squiflow::engine {

// Money, in the smallest unit of the currency. One currency, decided already:
// there is no multi-currency support and adding it later is a schema change,
// not a setting.
//
// Integer only. Every arithmetic operation is checked and returns whether it
// succeeded, because an overflow that silently wraps is a wrong invoice.
struct Money {
    std::int64_t minor = 0;

    static constexpr std::int64_t kMinorPerUnit = 100;

    static constexpr Money from_units(std::int64_t units) noexcept {
        return Money{units * kMinorPerUnit};
    }

    constexpr bool is_zero() const noexcept { return minor == 0; }
    constexpr bool is_negative() const noexcept { return minor < 0; }

    friend constexpr auto operator<=>(const Money&, const Money&) = default;
    friend constexpr bool operator==(const Money&, const Money&) = default;
};

struct MoneyResult {
    bool ok = false;
    Money value;
};

MoneyResult money_add(Money a, Money b) noexcept;
MoneyResult money_subtract(Money a, Money b) noexcept;
MoneyResult money_negate(Money a) noexcept;

// rate x quantity, rounded half away from zero to the smallest unit.
//
// Half away from zero is the rule a shopkeeper applies by hand, and matching
// the hand calculation matters more than matching a banker's convention that
// nobody at the counter would recognise.
MoneyResult money_multiply(Money rate, Quantity quantity) noexcept;

// Always two decimals, always grouped, never a currency symbol: the symbol is
// a display decision belonging to the branding package.
std::string format(Money value);

// Accepts "1200", "1200.50", "-40.05". Refuses more than two decimals.
MoneyResult parse_money(std::string_view text);

}  // namespace squiflow::engine
```
**Proved by test, not by assertion:** three at 33.33 is 99.99 and not 100.00; a hundred lines of 33.33 total exactly 3,333.00; overflow is reported instead of wrapping; and 0.025 rounds to 0.03 on both the positive and the negative side.
---
## `engine/records/lifecycle.hpp`
One lifecycle shared by quotations, invoices and agreements, so those three cannot drift into three different ideas of what cancelled means.
```c++
enum class DocumentState : std::uint8_t {
    Draft,      // freely editable, no number taken, invisible to the customer
    Issued,     // an explicit human act; frozen from here on
    Cancelled,  // the number is burned and never reused
    Replaced,   // cancelled and superseded by a linked reissue
    Discarded,  // a draft thrown away; it never became anything
};

// The only permitted moves:
//   Draft     -> Issued | Discarded
//   Issued    -> Cancelled
//   Cancelled -> Replaced
//
// There is deliberately no path back to Draft. An issued document is evidence
// the customer may already be holding.
bool transition_allowed(DocumentState from, DocumentState to) noexcept;

constexpr bool is_editable(DocumentState state) noexcept {
    return state == DocumentState::Draft;
}

// Drafts and discarded drafts never took a number, which is why abandoned
// work leaves no gap in the sequence.
constexpr bool has_number(DocumentState state) noexcept {
    return state != DocumentState::Draft && state != DocumentState::Discarded;
}
```
Asking to edit an issued document does not merely fail — the refusal reads **"an issued document cannot be edited; cancel and reissue"**, which is tested.
---
## `engine/identity/capability.hpp`
The single gate. Every screen, shortcut, sync handler and workflow asks this one function.
```c++
enum class DenialReason : std::uint8_t {
    None,
    NotSignedIn,
    ModuleInactive,
    NoRight,
    RequiresConnection,
    ReadOnlyOffline,
};

struct Decision {
    bool allowed = false;
    DenialReason reason = DenialReason::None;
    // Written for the person at the counter, not for a log file. A refusal
    // nobody understands gets worked around, and the workaround is worse than
    // whatever the rule was protecting.
    std::string explanation;
};

Decision may_run(protocol::OperationId operation, const Session& session,
                 ConnectionState connection,
                 const protocol::Activation& activation);
```
**The order of the checks is itself a decision.** Module activation is tested before permission, so a switched-off module reads *"this shop does not use agreements"* rather than *"you do not have permission"* — otherwise people go and ask for a right they do not need.
<table header-row="true">
<tr>
<td>Situation</td>
<td>Result</td>
</tr>
<tr>
<td>Owner, counter sale, offline</td>
<td>Allowed</td>
</tr>
<tr>
<td>**Staff, counter sale, offline**</td>
<td>**Allowed** — the counter never stops</td>
</tr>
<tr>
<td>Staff, create a customer, offline</td>
<td>Refused: read-only offline</td>
</tr>
<tr>
<td>Owner, create a customer, offline</td>
<td>Allowed</td>
</tr>
<tr>
<td>**Owner, grant a right, offline**</td>
<td>**Refused** — needs the server, and not because of who they are</td>
</tr>
<tr>
<td>Anyone, metered or weak connection</td>
<td>Allowed — a slow line changes how sync behaves, not what a person may do</td>
</tr>
</table>
### The staff exception is data, not code
```c++
// staff_offline.def — the whole policy, readable without reading a function.
SQF_STAFF_OFFLINE(counter_sale)
SQF_STAFF_OFFLINE(take_payment)
SQF_STAFF_OFFLINE(document_print)
SQF_STAFF_OFFLINE(purchase_lookup)
SQF_STAFF_OFFLINE(file_search)
```
A test walks this list and fails if any entry is not itself marked usable offline — a contradiction there would be a rule that can never fire.
---
## Two decisions worth arguing with
<table header-row="true">
<tr>
<td>Decision</td>
<td>Why, and what it costs</td>
</tr>
<tr>
<td>**Four decimals in a quantity is refused, not rounded**</td>
<td>Rounding behind someone's back is how a total nobody can explain gets printed. The cost: a person typing `1.2345` sees a refusal instead of `1.235`</td>
</tr>
<tr>
<td>**Signatures may never be stored in a lossy format** — including the one bill photos use</td>
<td>Lossy compression destroys exactly what a signature is: thin strokes on a plain background. Bill photos are converted on purpose; evidence is not. Enforced by a function and tested against `jpg`, `avif`, `webp` and `heic`</td>
</tr>
</table>
<callout icon="🔢">
	**Numbering gaps are legitimate and now proven.** One device holds 1–500 and the other 501–1000, so the shop will see 12, 13, 504, 14. A gapless sequence would need a server round trip per document, meaning no invoice could be written while the line is down. **Gaps are the cheaper price**, and a block running low raises an attention item before it runs out.
</callout>
