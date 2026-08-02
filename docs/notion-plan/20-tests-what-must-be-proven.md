# tests/ — what must be proven

Source page id: 640282be8de647869b44553fe750d107

---

<callout icon="🧪">
	**Purpose.** Module tests live inside their modules. This directory holds the tests that cross boundaries — the ones no module can own.
</callout>
## Contents
<table header-row="true">
<tr>
<td>Directory</td>
<td>What it proves</td>
</tr>
<tr>
<td>`architecture/`</td>
<td>**The rules are real.** No module includes another module's header; the requires graph is acyclic; no core module requires an extra; every module in the composition root exists and every module on disk is registered; migration numbers are unique across all modules</td>
</tr>
<tr>
<td>`workflows/`</td>
<td>Each cross-module workflow end to end against a real temporary database</td>
</tr>
<tr>
<td>`sync/`</td>
<td>The scenarios that ruin data if wrong: replay of an already-applied operation, both devices editing one record, an interrupted batch, a cursor that jumped, an outbox surviving a kill</td>
</tr>
<tr>
<td>`migration/`</td>
<td>Every released schema version upgrades to current, on a database with real rows in it</td>
</tr>
<tr>
<td>`fuzz/`</td>
<td>The message decoder, fed malformed bytes. It parses input from the network, so it is the one component an attacker reaches first</td>
</tr>
<tr>
<td>`performance/`</td>
<td>Startup time, a large list, a long sync — measured against recorded budgets, failing if they regress</td>
</tr>
<tr>
<td>`packaging/`</td>
<td>The **staged, pruned folder actually launches**. This catches an over-eager prune before the shop does</td>
</tr>
</table>
## Rules
- Domain tests use no database. Service tests use a real temporary one — **never a mock of the database**, because the queries are the part that breaks.
- Platform interfaces are exercised through their fakes.
- **A bug fixed without a test is a bug scheduled to return.**
- The performance budgets are numbers recorded from the real machine, not numbers invented in advance.
## Done when
The architecture tests fail when a rule is deliberately broken, and the packaging test fails when a required file is deliberately pruned.
