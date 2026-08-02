# src/modules/ — the twelve

Source page id: 4af4e249f8714486b2946f7ebd578c97

---

<callout icon="🧹">
	**Purpose.** Twelve modules, every one laid out identically. Sameness is the point: opening an unfamiliar module should require no orientation.
</callout>
## The canonical shape
Every module directory, without exception:
```plain text
<module>/
  CMakeLists.txt        declares sources, tier, requires, tests
  module.cpp            registration: screens, rights, sync handlers, attention rules
  operations.def        the operation list (lives in the protocol submodule)
  domain/               entities and rules, no database, no Qt
  service/              use cases; the only entry point other layers may call
  data/                 tables, queries, mappers
    migrations/         numbered from the one global sequence
  sync/                 how this module's rows go out and come back
  view/                 view models exposed to the interface
  tests/                domain tests without a database, service tests with one
```
**Layering: view → service → data → domain. Never upward, never sideways.** A module never includes a header from another module; that is what `src/workflows/` is for.
## The twelve, with tier
<table header-row="true">
<tr>
<td>Module</td>
<td>Tier</td>
<td>Owns</td>
</tr>
<tr>
<td>`administration`</td>
<td>Core</td>
<td>People, rights grants, devices, shop settings, module activation</td>
</tr>
<tr>
<td>`parties`</td>
<td>Core</td>
<td>Customers and suppliers as one identity, contacts, per-customer billing terms</td>
</tr>
<tr>
<td>`catalog`</td>
<td>Core</td>
<td>What the shop sells. Product identity by name</td>
</tr>
<tr>
<td>`pricing`</td>
<td>Core</td>
<td>**Rate resolution, sole owner.** A rate is a relation between product, party, agreement and time — not an attribute of a product</td>
</tr>
<tr>
<td>`orders`</td>
<td>Core</td>
<td>What was agreed to be done. Several jobs may hang off one order</td>
</tr>
<tr>
<td>`receivables`</td>
<td>Core</td>
<td>Invoices, payments, allocations, credit accounts, statements. The strongest boundary in the system</td>
</tr>
<tr>
<td>`jobs`</td>
<td>Extra *(flagged — may belong in core)*</td>
<td>Job tickets and their states. Its own number block; the link to an order is optional and one-way</td>
</tr>
<tr>
<td>`quotations`</td>
<td>Extra</td>
<td>Quotations and revisions, acceptance, expiry</td>
</tr>
<tr>
<td>`agreements`</td>
<td>Extra</td>
<td>Rate agreements, periods, quantity caps, open and closed states</td>
</tr>
<tr>
<td>`sourcing`</td>
<td>Extra</td>
<td>Supplier directory, purchase logbook, which supplier a material came from, paid-or-owed and settle date</td>
</tr>
<tr>
<td>`companion`</td>
<td>Extra</td>
<td>Tasks and reminders attached to anything, through the generic reference</td>
</tr>
<tr>
<td>`files`</td>
<td>Extra</td>
<td>Design-file identity and search. Files stay on shop volumes</td>
</tr>
</table>
## Tier rules, enforced by the build
- **A core module may never require an extra.** Core is closed under dependency.
- The requires graph is **acyclic**, checked at configure time.
- Deactivation **hides features, never deletes data**, is **shop-wide and synced**, and is **refused while an active record still depends on the module**.
- Rights granted for an inactive module persist and simply do not appear.
## Against boilerplate
A `templates/module/` directory plus a generator produces the whole skeleton, so adding a module is a command rather than an afternoon. **Twelve modules cost nothing at run time** — static linking with whole-program optimisation makes module count a source-organisation choice, not a performance one.
## Done when
Each module compiles alone, tests alone, and can be removed by deleting its directory and one line in the composition root.
