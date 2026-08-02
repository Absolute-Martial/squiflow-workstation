# SquiFlow — Quoting & Quote-to-Order: Full Plan

Source page id: 870d03e520494412aa17fa9ed6b73515

---

<callout icon="🧾">
	**This is the second service planned to full depth**, alongside design-file search. It covers **quoting and quote-to-order** end to end: parties, catalog, the pricing engine, immutable revisions, printed documents, acceptance to order, and the Nepal tax-compliance boundary. The other three services you selected — job-to-cash, customer proofing, and white-label onboarding — are sequenced at the end of this page rather than designed here, because all three consume objects this service creates.
</callout>
## Why this service goes first among the non-search four
It is Phase 2 in the existing plan, and it is the only one of the four with no upstream dependency other than identity and tenancy, which are already partly built. More importantly, **it creates the objects the other three need**: proofing approves something that was ordered, job-to-cash bills something that was priced, and white-label onboarding is meaningless until there is a document with a logo on it.
The existing status documents say the honest state clearly: quotations and orders exist as a thin slice with **one explicit line and no catalog or specification aggregate**, and **confidential cost/margin projection and discount approval do not exist at all**. So this is not a greenfield build — it is filling in the two hardest parts of a skeleton that already compiles.
---
## What "quote" means in a print shop, and why generic quoting software fails here
A print quotation is not a list of products with prices. It is a **specification that implies a manufacturing route**, and the price falls out of the route. The same customer sentence — "200 certificates, A4, both sides" — can produce three different costs depending on paper, whether it is offset or digital, and how many fit on a press sheet.
That has three consequences that shape the whole design.
1. **Price is computed, not chosen.** A price list of finished goods cannot express this. What is stable is the *inputs*: material rates, machine rates, finishing rates, and the rules that turn a specification into quantities of each.
2. **The same quote is re-quoted constantly.** Customers change quantity and paper and ask again. Re-entry is the actual daily pain, which is why revisions and cloning are first-class rather than a convenience feature.
3. **The shop already has a pricing habit, and it is partly informal.** Some of it is a rate card and some of it is the owner's judgement. The system has to accommodate the second without pretending it is the first — which is what the negotiated-price and discount-approval paths are for.
---
## Domain model
These are the aggregates. Each one is named because it has its own invariants, not because it deserves a table.
<table header-row="true">
<tr>
<td>Aggregate</td>
<td>Why it exists separately</td>
<td>Hard invariant</td>
</tr>
<tr>
<td>**Party** (person or organization, with roles)</td>
<td>The same organization is a customer, a supplier and a sponsor-on-a-banner. One identity, many roles.</td>
<td>A party is never duplicated by a spelling variant; aliases and romanized forms attach to one identity.</td>
</tr>
<tr>
<td>**Catalog item**  • **typed specification**</td>
<td>A banner and a certificate have genuinely different fields. A free-text description cannot be priced.</td>
<td>A specification is valid against its type's schema, or the quote line cannot be priced at all.</td>
</tr>
<tr>
<td>**Price book** (versioned, effective-dated)</td>
<td>Paper rates change. Historical quotes must keep quoting the old rate.</td>
<td>A price book version is immutable once used by any quotation.</td>
</tr>
<tr>
<td>**Quotation** → **Quotation revision**</td>
<td>Negotiation is a sequence of offers, not an editable record.</td>
<td>A revision is append-only and never mutated after being issued.</td>
</tr>
<tr>
<td>**Order**</td>
<td>An order is a commitment; a quote is an offer.</td>
<td>An order references **exactly one accepted revision**, by snapshot, not by pointer.</td>
</tr>
<tr>
<td>**Document template** (versioned)</td>
<td>The printed quotation is a legal-ish artifact the customer keeps.</td>
<td>Re-rendering an old quotation uses the template version it was issued with.</td>
</tr>
<tr>
<td>**Cost/margin projection**</td>
<td>Confidentiality, not convenience.</td>
<td>Cost and margin never appear in a staff-role DTO, and never in the customer document.</td>
</tr>
</table>
### The one modelling decision that matters most
**A quotation revision stores a complete priced snapshot, not references.** Material rates, machine rates, the specification, the discount, the tax treatment, the template version — all copied in at issue time.
This is more storage and it is unambiguously correct. The alternative — resolving rates at render time — means a paper price change silently rewrites history, and the acceptance test in the existing plan is precisely that *"later template changes do not alter history."* A referenced design fails that test by construction.
---
## The pricing engine
This is the part that is worth building carefully and the part most likely to be got wrong by making it clever.
### It must be deterministic and replayable
Given the same specification, the same price book version and the same discount decision, the engine returns the same total, forever, on every machine. That means:
- **No floating point anywhere in money.** NPR in integer minor units throughout, as the existing foundation already establishes.
- **Rounding is a declared policy, applied at declared points**, not wherever the code happens to divide. Where the shop rounds — per line, per subtotal, on tax — is a *business* decision to be confirmed with the owner and then encoded once. This is exactly the "approved rounding policy" the existing Phase 2 lists as a dependency, and it is a blocker, not a detail.
- **The engine is a pure function.** No clock, no database reads, no locale. Inputs in, priced result out. This is what makes golden-file tests possible and what makes historical replay possible.
### The cost model, in the order it computes
1. **Specification → material demand.** How many press sheets, how much roll width and length, how much wastage. This is the step where a real shop's knowledge lives, and it is arithmetic, not AI: sheets-per-parent, items-per-sheet, and a **spoilage allowance** that is itself a rate.
2. **Material demand → material cost**, at the price book version's rate, with minimum-purchase rules where they exist.
3. **Process cost.** Setup plus run, per process, from machine rates. Setup dominates at small quantities, which is exactly why a shop's instinct that "50 and 200 cost nearly the same" is correct and must be reproducible.
4. **Finishing and outwork.** Lamination, binding, eyelets, delivery. Some of these are bought outside, so they carry a supplier reference and a markup rather than a machine rate.
5. **Margin, then discount.** In that order and never merged. Margin is internal; discount is a customer-facing, authorized decision. Merging them destroys the ability to answer "did we make money on this job."
6. **Tax.** Last, on the discounted taxable amount, per the tax treatment recorded on the revision.
### Where human judgement is allowed in, safely
The shop will override prices. Pretending otherwise produces a system people bypass.
- **A line may carry a negotiated price**, which replaces the computed price but **never erases it**. Both are stored. The variance is what later tells the owner which jobs are quietly unprofitable.
- **Discounts above a threshold require an approval**, with the approver, the timestamp and the reason recorded as evidence. Thresholds are tenant configuration.
- **Suggest, human confirms.** Consistent with the product vision: the system may propose a price from a near-identical historical quote, with the evidence shown. It never posts it.
---
## Immutable revisions and the acceptance boundary
The state machine is deliberately small.
- **Draft** revisions are editable and are not offers. Nothing outside the shop sees them.
- **Issued** revisions are frozen and printable. Editing means creating the next revision.
- **Superseded** happens automatically when a later revision is issued.
- **Accepted** applies to exactly one revision, and **acceptance is the event that creates the order** — carrying a snapshot, not a reference.
- **Declined / expired** are recorded, with the reason where known. This is the data that later answers "why do we lose quotes," and it costs nothing to capture now.
**Optimistic concurrency on every mutation**, and **idempotency keys on issue and accept.** Two staff on two devices accepting the same quotation must produce one order, and a retried request after a timeout must not produce a second one. The existing plan already calls for both; they belong here rather than being retrofitted.
### Repeat orders are the highest-value cheap feature
A print shop's work is substantially repeat work. **Cloning a past quotation into a fresh draft**, with the specification intact and rates re-resolved to the *current* price book, converts the most common daily task into two clicks. It is a small feature that will be used more than anything else on this page.
---
## The Nepal tax and compliance boundary
This needs to be got right at design time because it is structural, and I want to be precise about what is established versus what needs confirming with your accountant.
<table header-row="true">
<tr>
<td>Fact</td>
<td>Design consequence</td>
</tr>
<tr>
<td>**VAT is 13%**, and VAT-registered businesses must issue compliant tax invoices; abbreviated tax invoices have their own prescribed field set.</td>
<td>Tax treatment is a field on the revision, not a global setting. A quotation is **not** a tax invoice and must never be formatted like one.</td>
</tr>
<tr>
<td>**IRD prescribes invoice numbering** and expects sequential, gap-free series with an audit trail.</td>
<td>Invoice numbering is a server-side, per-tenant, per-fiscal-year sequence with no client involvement and no reuse. Never derived from a UUID or a timestamp.</td>
</tr>
<tr>
<td>**CBMS real-time invoice reporting** to the IRD is mandatory for electronic invoicing above an annual turnover threshold (reported as Rs 10 crore, effective from 2083/03/29), and billing software is expected to be IRD-verified.</td>
<td>**This is why quoting ships before invoicing.** A quotation carries no CBMS obligation. Design the invoice module behind a port from day one so a CBMS submission adapter can be added without touching the ledger.</td>
</tr>
<tr>
<td>Reporting and returns run on the **Bikram Sambat fiscal year**.</td>
<td>BS is a first-class calendar in the domain, not a display formatting concern. Store both, derive neither at render time.</td>
</tr>
<tr>
<td>Corrections are made by **credit and debit notes**, not by editing an issued invoice.</td>
<td>Reinforces append-only. The same discipline already applied to revisions applies to money documents.</td>
</tr>
</table>
<callout icon="⚠️">
	**What I am not doing here:** claiming this service is tax-compliant. Compliance depends on your PAN/VAT status, turnover, and IRD verification of the software itself — the existing Phase 0 already lists *"validate PAN/VAT, numbering, payment, rounding and accounting boundaries with qualified advisers"* as a gate, and it is a real gate. **This plan's job is to make compliance addable without a redesign**, by keeping numbering server-authoritative, documents versioned, records append-only, and the submission path behind a port.
</callout>
---
## Payments: relevant, but deliberately not in this service
On the Nepali rails — eSewa, Khalti, Fonepay QR, connectIPS — the integration facts worth recording now: Khalti publishes open documentation and needs no paid demo account, while eSewa's merchant API is reported to carry a one-off integration fee around Rs 25,000 plus per-transaction percentages. Fonepay QR is reachable through aggregators.
Two design rules regardless of which is chosen:
- **The gateway is never the ledger authority.** It supplies a verifiable payment reference; the AR subledger remains the source of truth. This is already stated in your existing task list and it is correct.
- **Reference capture and reconciliation are idempotent**, because notification retries are guaranteed and a duplicated receipt is a real accounting error.
For a single shop collecting mostly cash and QR-in-person, **a scanned static QR plus manual reconciliation is honestly sufficient for v1**, and a paid gateway integration is a white-label-era feature. Recorded so it is a decision rather than an omission.
---
## Documents
- **Versioned templates**, with the version stamped on every issued revision. Re-rendering a two-year-old quotation reproduces the two-year-old document exactly.
- **Rendered PDFs are stored as immutable objects** through the existing `IObjectStore` boundary, keyed by content hash, tenant-private.
- **Devanagari and English in one document**, which makes font embedding and line breaking a real requirement rather than a styling preference. Test with the actual customer names in the archive, not Latin placeholders.
- **Golden-file tests on rendered output.** A template change that shifts a total's position is cosmetic; one that changes a total is a defect, and only a golden test catches the difference.
---
## Milestones
<table header-row="true">
<tr>
<td>Milestone</td>
<td>Ends with</td>
<td>Blocked on</td>
</tr>
<tr>
<td>**Q0 — Collect the real thing**</td>
<td>20 redacted historical quotations, 10 negotiated-price examples, the rate card, and the rounding policy written down and signed off</td>
<td>Owner's time. Nothing else starts honestly without it.</td>
</tr>
<tr>
<td>**Q1 — Parties**</td>
<td>People and organizations with roles, aliases, romanized forms, and no duplicate identities</td>
<td>Existing identity/tenant boundary</td>
</tr>
<tr>
<td>**Q2 — Catalog and typed specifications**</td>
<td>Two or three real product types with validated specification schemas</td>
<td>Q0's samples</td>
</tr>
<tr>
<td>**Q3 — The pricing engine**</td>
<td>A pure, deterministic engine reproducing Q0's approved quotations to the rupee, under golden tests</td>
<td>Q2, and the approved rounding policy</td>
</tr>
<tr>
<td>**Q4 — Revisions and documents**</td>
<td>Issue, supersede, clone, and a printed quotation that matches the approved sample</td>
<td>Q3, template versioning</td>
</tr>
<tr>
<td>**Q5 — Acceptance to order**</td>
<td>One accepted revision becomes one order, by snapshot, idempotently</td>
<td>Q4</td>
</tr>
<tr>
<td>**Q6 — Confidential pricing and approvals**</td>
<td>Role-limited cost/margin projections, discount thresholds, approval evidence, and cross-role leakage tests that fail closed</td>
<td>Q3, Q5, the staff permission matrix</td>
</tr>
</table>
**Q3 is the milestone that decides whether this service is trusted.** If the engine reproduces the owner's own past quotations exactly, adoption is easy. If it is off by rupees, every quote gets checked by hand forever and the product has failed even though it works.
---
## Acceptance for the whole service
Taken from the existing plan and made concrete: **the owner reproduces the approved quotation samples, finds a prior customer and job, prints a correct quotation, and converts one exact accepted revision into an order — and a later template or price-book change does not alter any of it.**
---
## The other three services, sequenced
Not designed here. Each gets its own full-depth page when its turn comes.
1. **Job-to-cash** — directly after this, because it consumes the order. Its hard parts are the append-only AR ledger with allocations and aging (already largely implemented), the cancellation accounting and the single transaction boundary spanning the order write and the AR mutation (both explicitly unfinished), and the CBMS-ready invoice port described above.
2. **Customer proofing and approval** — after job-to-cash, and it is where the design-file search work and this service meet: a proof is a derivative of an asset, attached to an order, approved by a customer, with the approval as evidence. The AVIF derivative pipeline from the search plan is directly reusable for proof previews. Approval is always a human act, never inferred.
3. **White-label tenant onboarding** — last of the four, because it packages the other three. Provisioning, branding, entitlements, export and restore. It is a productization milestone, and attempting it before there is a product to brand inverts the dependency.
---
## Open risks specific to this service
- **The rounding and tax policy is unconfirmed**, and it is upstream of the pricing engine. Guessing it means rewriting Q3.
- **The shop's pricing may not be fully expressible as rules.** If Q0 reveals that a material share of quotes are pure judgement, the engine becomes an assistant with a negotiated-price field rather than an authority — a legitimate outcome, but it must be discovered in Q0, not in Q4.
- **CBMS applicability depends on turnover and IRD verification of the software itself**, neither of which is under this plan's control. The port keeps the cost of being wrong low; it does not eliminate it.
- **Spoilage and imposition rules are shop-specific tacit knowledge** and are the most likely source of "the computer's price is wrong." They need to be captured as data with the owner present, not inferred from invoices.
- **Confidential cost/margin leakage is a fail-closed requirement**, and the existing status says no confidential projection exists yet. Until Q6 lands, any margin field in any DTO is a live confidentiality risk.
