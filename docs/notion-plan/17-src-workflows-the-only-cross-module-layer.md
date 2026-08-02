# src/workflows/ — the only cross-module layer

Source page id: 3bc5585b746040f0904bc68cd59901aa

---

<callout icon="🔀">
	**Purpose.** Real work crosses modules; modules may not see each other. This layer exists so that crossing has a legal home instead of leaking into whichever module was opened first. It was added because the structure was attacked and this hole was found.
</callout>
## The rule
**This is the only place permitted to depend on more than one module.** Modules stay leaves. A workflow may call several module services; a module may never call a workflow.
Each workflow **declares the modules it requires**. If any of them is deactivated, the workflow is simply not registered — which is how the core and extra tiers stay coherent without a dependency resolver.
## Files
<table header-row="true">
<tr>
<td>Workflow</td>
<td>What it spans</td>
<td>Requires</td>
</tr>
<tr>
<td>`quote_to_order.*`</td>
<td>Copies an accepted quotation into an order, snapshotting rates so a later change cannot rewrite it</td>
<td>quotations, orders, pricing</td>
</tr>
<tr>
<td>`order_to_jobs.*`</td>
<td>Creates jobs from an order. **Several jobs per order is normal, and a job may exist with no order at all** — so this is a cause, not an ownership link</td>
<td>orders, jobs</td>
</tr>
<tr>
<td>`issue_invoice.*`</td>
<td>The explicit human act: freeze the draft, take a number, snapshot the lines, write the audit row — all in one transaction</td>
<td>receivables, orders, pricing</td>
</tr>
<tr>
<td>`cancel_and_reissue.*`</td>
<td>The only correction mechanism. Cancels, burns the number, creates a linked replacement with values carried across for editing. **No refunds, no credit notes**</td>
<td>receivables</td>
</tr>
<tr>
<td>`apply_agreement.*`</td>
<td>Resolves whether an agreement's rate and quantity cap apply, and consumes the quantity</td>
<td>agreements, pricing, receivables</td>
</tr>
<tr>
<td>`take_payment.*`</td>
<td>Records a payment and allocates it. **Allocation is always a human decision**, never inferred</td>
<td>receivables, parties</td>
</tr>
<tr>
<td>`counter_sale.*`</td>
<td>The whole chain skipped: take the work, price it, take payment, print. The ordinary case, not a special one</td>
<td>orders, pricing, receivables</td>
</tr>
<tr>
<td>`record_purchase.*`</td>
<td>Logs a supplier purchase, its paid-or-owed state, and the bill image reference</td>
<td>sourcing</td>
</tr>
</table>
## Rules
- **A workflow owns no data.** It orchestrates; the modules store. If a workflow needs its own table, the design is wrong.
- **One transaction per workflow where money is involved.** Issuing an invoice must not be able to half-happen.
- **Every workflow is an operation** in the declaration list, so it has a right, an offline rule and audit — exactly like a module operation.
- Workflows are where the *sequence* lives; rules stay in module services, so they hold no matter which path reaches them.
## Done when
A counter sale runs end to end through `counter_sale`, and no module includes a header from another module anywhere in the tree.
