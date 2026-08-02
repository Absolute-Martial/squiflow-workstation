# SquiFlow — Feature Set & Usage Specification

Source page id: f955d6b8687b499ba779a042688757e6

---

<callout icon="📌">
	**What this page is.** The feature-level and usage-level specification for SquiFlow, decided in planning conversation and locked here. The parent page covers *how the system is built*; this page covers **what the features are and exactly how they are used at the counter**. Every decision recorded here was made by the shopkeeper, not inferred. Where something is postponed, it says so and says why.
</callout>
## Operating context — the constraints every feature is designed against
These are facts about the shop, not preferences. They kill more features than they create, which is the point.
<table header-row="true">
<tr>
<td>Fact</td>
<td>Direct consequence for the feature set</td>
</tr>
<tr>
<td>**Two people: the shop owner and one staff member, on two devices**</td>
<td>**Revised.** Permissions are now a shipping feature, not a postponed one. The owner holds every right; the staff member holds only what is explicitly granted</td>
</tr>
<tr>
<td>**One machine**</td>
<td>No machine profiles, no press selection, no capacity scheduling, no crossover optimisation</td>
</tr>
<tr>
<td>**Everything managed manually**</td>
<td>The system remembers and prepares; it never decides. No automatic pricing, no automatic posting, no automatic sending</td>
</tr>
<tr>
<td>**Paper usage is dynamic and parameter-dependent**</td>
<td>No material accounting, no wastage calculation, no consumption tracking of any kind</td>
</tr>
<tr>
<td>**No refunds, ever**</td>
<td>No credit note, no refund document. Corrections are rate, quantity or time period only</td>
</tr>
<tr>
<td>**Product mix: banner and flex, ID cards, certificates**</td>
<td>Specification-heavy, quantity-light work. Job specs matter more than line-item maths</td>
</tr>
<tr>
<td>**Native desktop, low resource**</td>
<td>Performance targets are acceptance criteria with numbers, not aspirations</td>
</tr>
<tr>
<td>**Hardware baseline (stated, not assumed): shop workstation is 8 GB DDR4 with an Intel Core i5-7500; a second desktop of similar specification is available as the server**</td>
<td>There is real headroom for a local database server. Budgets are set for responsiveness and for sharing the machine with design software — not because memory is scarce</td>
</tr>
<tr>
<td>**Mixed customer types**</td>
<td>Individuals pay instantly; organizations run on credit accounts or agreed rates; combinations are normal</td>
</tr>
</table>
---
## Locked decisions
The authoritative list. If any later document disagrees with this table, this table wins until it is explicitly revised.
<table header-row="true">
<tr>
<td>#</td>
<td>Decision</td>
<td>Detail</td>
</tr>
<tr>
<td>1</td>
<td>**Billing terms are per customer, not a system mode**</td>
<td>Settlement (instant / credit / advance) and pricing source (catalog / agreed rate / negotiated) are properties of the customer, and they combine freely</td>
</tr>
<tr>
<td>2</td>
<td>**"Issued" is an explicit human act**</td>
<td>A document is a freely editable draft until the shopkeeper marks it issued. Marking it issued locks it and assigns the final number</td>
</tr>
<tr>
<td>3</td>
<td>**Correction after issue = cancel and reissue**</td>
<td>The cancelled document is never deleted, always retains its number, and is permanently linked to its replacement</td>
</tr>
<tr>
<td>4</td>
<td>**No refunds. No credit notes.**</td>
<td>The only corrections are rate, quantity, or time period. If a cancelled invoice was already paid, the money becomes an unallocated amount on the customer, applied by hand later</td>
</tr>
<tr>
<td>5</td>
<td>**Payment allocation is manual**</td>
<td>The shopkeeper decides which payment covers which job. Nothing is auto-matched, nothing is auto-applied</td>
</tr>
<tr>
<td>6</td>
<td>**No hardcoded "owner only"**</td>
<td>Every sensitive action has a *named right*, and those rights are now actually **enforced** rather than merely named. The owner holds all of them; the staff member holds a granted subset</td>
</tr>
<tr>
<td>7</td>
<td>**Any step in the chain can be skipped**</td>
<td>Inquiry → quotation → order → job → invoice → payment. The shopkeeper may jump straight to a job or straight to a counter sale. The system records which links were skipped</td>
</tr>
<tr>
<td>8</td>
<td>**Price is remembered, never computed**</td>
<td>No formulas from material or machine. The system shows what was last charged and to whom; the shopkeeper accepts or overrides</td>
</tr>
<tr>
<td>9</td>
<td>**Outbound messages are always human-confirmed**</td>
<td>The system prepares the message; nothing leaves the shop unread. No scheduled sending, no send-on-event</td>
</tr>
<tr>
<td>10</td>
<td>**Staff roles and permissions are in scope** — *revised decision*</td>
<td>Previously postponed on the basis of one operator and no staff. Now there are two people on two devices, so sign-in, enforced rights, and permission editing ship as real features. Every record records **who** did it, not just which device</td>
</tr>
<tr>
<td>11</td>
<td>**Automated estimation is a future direction, not a feature**</td>
<td>The data being captured now (purchase prices, job specs, quantities) would feed it later. Nothing is built for it now</td>
</tr>
<tr>
<td>12</td>
<td>**Suppliers are a memory feature, not an inventory feature**</td>
<td>Directory + purchase logbook + "where did we get this" lookup. No stock, no balances, no purchase orders</td>
</tr>
<tr>
<td>13</td>
<td>**Supplier credit exists**</td>
<td>The shop does buy on credit. Every purchase record carries paid or still owed, with the settle date when it clears. A flag and a list — still no supplier ledger, no aging engine, no automatic payables</td>
</tr>
<tr>
<td>14</td>
<td>**Agreements can cap quantity**</td>
<td>An agreement line may carry a quantity cap as well as a rate and a validity period. Consumption is tracked against the cap and warned on, never auto-enforced</td>
</tr>
<tr>
<td>15</td>
<td>**Product identity is by name, and rates follow the name**</td>
<td>The same physical product may exist under several names, and the shopkeeper may set a different agreed rate for each name. The name is what the customer agreed to, so the name carries the rate</td>
</tr>
<tr>
<td>16</td>
<td>**Approval and signature are first-class**</td>
<td>Any document may be emailed for approval when email is configured — the shopkeeper chooses, per document, and reads it first. Approvals and agreements are recorded as signed, with the signature or signed copy retained</td>
</tr>
</table>
---
## The four ways work enters the shop
Every flow below is a *complete* path. None is a degraded version of another, and the system must not push the shopkeeper toward the long one.
### 1. Counter sale — walk-in, pays now
The most common path and the one that must be fastest.
1. Shopkeeper opens the app to the day view, presses the shortcut for a new counter sale.
2. Types the product (autocompletes from catalog), quantity, and the price — the last-charged price is already filled in.
3. Optionally types a customer name; a phone number is enough. **No customer record is required.**
4. Records payment in the same motion — cash or wallet reference.
5. Prints or shares a receipt.
**Done means:** the sale, the payment and the receipt exist as one action, no invoice cycle is opened, no credit exposure is created, and the whole thing took under 30 seconds.
### 2. Quoted job — customer asks, shop quotes, customer accepts
1. Inquiry is captured (or skipped entirely).
2. Quotation is drafted: products, specifications, quantities, prices, validity date.
3. Quotation is printed or emailed **after the shopkeeper reads it**.
4. Customer accepts → the accepted revision converts to an order, carrying its prices forward as a snapshot.
5. Order becomes a job with a printable job ticket.
6. Invoice is drafted from the job, edited freely, then marked issued.
**Done means:** the accepted version is pinned exactly as accepted. Later edits to templates, catalog prices or specs never rewrite what the customer agreed to.
### 3. Organization job — credit account or agreed rate
1. Organization already has a billing profile: credit limit, credit period, cycle day, and/or a rate agreement.
2. Job is created directly — no quotation needed, because the rate is already agreed.
3. Job pulls the **agreed rate** automatically and locks it for that job.
4. Jobs accumulate through the period.
5. On the cycle day, the shopkeeper generates one consolidated invoice covering the period's jobs, or per-phase invoices where the work is phased.
6. A payout document pack is produced when the organization's accounts department needs paperwork.
**Done means:** the organization never receives a surprise rate, the shopkeeper can see live exposure against the credit limit, and the paperwork the organization's finance team asks for can be regenerated on demand.
### 4. Fast path — take the job now, tidy up later
The intake that respects a busy counter.
1. Shopkeeper records the absolute minimum: who is paying, and the price.
2. The record is marked as **thin** — visibly incomplete, not silently broken.
3. Work proceeds. Files get designed, printing happens.
4. Later, the record is filled in and promoted to a normal job.
**Done means:** a thin record can be worked, printed and invoiced without ever being completed, but it always *looks* thin so it is never mistaken for a finished record.
---
## Feature areas
Each feature states what the shopkeeper does, what the system does, and what counts as done.
### A — Shop identity and access *(reduced scope)*
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**A1 Shop profile**</td>
<td>Shop name, logo, address, PAN, contact details entered once; they appear on every printed document</td>
<td>Changing the logo changes every future document and no past one</td>
</tr>
<tr>
<td>**A2 Document numbering patterns**</td>
<td>Choose the prefix and pattern per document type — quotation, invoice, receipt, job ticket</td>
<td>Numbers are gapless per series, and a cancelled number is never reused</td>
</tr>
<tr>
<td>**A3 Two users, two devices**</td>
<td>Owner and staff member each sign in on their own device. The session identifies the person, not just the machine</td>
<td>Every record shows who created it and who issued it. Sign-in stays lightweight — a local account and a PIN or password, not enterprise identity</td>
</tr>
<tr>
<td>**A4 Rights and roles**</td>
<td>Every sensitive action is a named right, now enforced: issue document, cancel and reissue, deviate from agreed rate, exceed credit limit, override credit hold, mark production complete, manage agreements, edit catalog prices, delete permanently, manage users. Roles group rights; the owner edits them</td>
<td>A right the staff member does not hold is either hidden or clearly blocked with a reason — never a silent failure. Attempting a blocked action can raise a request to the owner instead of dead-ending</td>
</tr>
<tr>
<td>**A5 Feature toggles**</td>
<td>Turn off what the shop does not use — agreements, credit accounts, phases</td>
<td>A disabled feature disappears from the interface without deleting its history</td>
</tr>
</table>
<callout icon="✅">
	**No longer postponed — revised.** Roles and permission editing were deferred on the basis of "one shopkeeper, no staff". That condition no longer holds: there is now a staff device alongside the owner's. Creating and editing roles, granting rights, permission-aware navigation, and per-person audit all move into Wave 1. The named-rights groundwork specified earlier is exactly what makes this a configuration change rather than a rewrite.
</callout>
### B — Customers and inquiries
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**B1 Customer record**</td>
<td>Individual or organization. Phone number is sufficient to start</td>
<td>A customer can be created in one field and enriched later</td>
</tr>
<tr>
<td>**B2 Contacts within an organization**</td>
<td>Several people per organization, one marked as the billing contact</td>
<td>The right person is on the invoice and the proof, without retyping</td>
</tr>
<tr>
<td>**B3 Billing profile**</td>
<td>Per customer: settlement type (instant / credit / advance), pricing source (catalog / agreed / negotiated), and defaults by customer type</td>
<td>Terms are visible on the customer screen before any job is priced. Changing terms applies **forward only**</td>
</tr>
<tr>
<td>**B4 Customer history**</td>
<td>One screen: every job, quotation, invoice, payment, outstanding balance and file for this customer</td>
<td>Answers "what did we do for them and what do they owe" without a search</td>
</tr>
<tr>
<td>**B5 Inquiry capture**</td>
<td>A phone enquiry noted in seconds, convertible to a quotation or a job later</td>
<td>An inquiry that never converts is still findable, and never clutters the job list</td>
</tr>
</table>
### C — Catalog and pricing
<callout icon="💰">
	**The pricing principle.** The shopkeeper decides the price. The system's only job is to make sure that price never has to be remembered or recalculated from scratch. There is no formula, no material cost input, and no computed estimate.
</callout>
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**C1 Product entries**</td>
<td>Named products with typed specifications — banner size and material, card type and finish, certificate paper and size</td>
<td>Choosing a product pre-fills the specification fields it always needs</td>
</tr>
<tr>
<td>**C2 Remembered price**</td>
<td>When quoting, the last charged price appears, with when it was used and to whom — both the overall last price and this customer's last price</td>
<td>The shopkeeper never types a price from memory</td>
</tr>
<tr>
<td>**C3 Price note**</td>
<td>A short free-text line explaining the number: "80gsm, 500 sheets, lamination extra"</td>
<td>Captured reasoning, not a calculation. Printed nowhere unless chosen</td>
</tr>
<tr>
<td>**C4 Bulk price editing**</td>
<td>One list of every product's current price, editable in place, when paper costs move</td>
<td>Updating twenty prices is one screen and a few minutes — not twenty screens</td>
</tr>
<tr>
<td>**C5 Customer-specific price**</td>
<td>A remembered price for a particular customer that overrides the general one</td>
<td>It applies silently when that customer is selected</td>
</tr>
<tr>
<td>**C6 Price resolution order**</td>
<td>Agreed rate → customer-specific price → catalog price → manual entry</td>
<td>The shopkeeper always sees *which* source produced the shown number</td>
</tr>
<tr>
<td>**C7 Variance flag**</td>
<td>When the entered price differs noticeably from the agreed rate or last price, the system says so before the document is committed</td>
<td>It warns; it never blocks. Nothing is priced down silently</td>
</tr>
<tr>
<td>**C8 Off-catalog line item**</td>
<td>A description and a price, with no catalog entry required — for one-off customer requests</td>
<td>Optionally promoted to the catalog afterwards; marked so it does not distort product reports</td>
</tr>
</table>
### D — Quotations and orders
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**D1 Quotation draft**</td>
<td>Customer, products, specs, quantities, prices, validity date, terms text</td>
<td>Freely editable while draft; nothing is numbered yet</td>
</tr>
<tr>
<td>**D2 Revisions**</td>
<td>Each time the customer asks for a change, a new revision is created; earlier ones remain readable</td>
<td>The shopkeeper can show the customer exactly what changed between revisions</td>
</tr>
<tr>
<td>**D3 Issue a quotation**</td>
<td>Explicit act: locks the revision, assigns the number, makes it printable and sendable</td>
<td>A sent quotation cannot silently change afterwards</td>
</tr>
<tr>
<td>**D4 Print and send**</td>
<td>Print for the counter, or email — the message is prepared and the shopkeeper presses send</td>
<td>Nothing is emailed without being read first</td>
</tr>
<tr>
<td>**D5 Validity and expiry**</td>
<td>Quotations carry a validity date; expiring ones surface as attention items</td>
<td>An expired quotation cannot be accepted without a deliberate revive</td>
</tr>
<tr>
<td>**D6 Acceptance**</td>
<td>Mark exactly which revision the customer accepted</td>
<td>Prices, specs and quantities are snapshotted at that moment</td>
</tr>
<tr>
<td>**D7 Accept → order**</td>
<td>One action converts the accepted revision into an order carrying the snapshot</td>
<td>The order never re-reads current catalog prices</td>
</tr>
<tr>
<td>**D8 Clone from history**</td>
<td>Start a new quotation or job from a previous one for the same customer</td>
<td>Repeat work — annual certificates, the same banner in a new size — costs a few keystrokes</td>
</tr>
<tr>
<td>**D9 Skip the chain**</td>
<td>Create an order or a job with no quotation at all</td>
<td>The record shows the quotation step was skipped, rather than showing a fake one</td>
</tr>
</table>
### E — Jobs and production
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**E1 Job record**</td>
<td>The unit of work: customer, products, specifications, quantities, dates, price, notes</td>
<td>One screen answers "what exactly are we making"</td>
</tr>
<tr>
<td>**E2 Printable job ticket**</td>
<td>The paper that goes to the machine: specs, quantity, deadline, customer, job number, and a barcode or QR to find the record again</td>
<td>Printed in one keystroke and readable at arm's length</td>
</tr>
<tr>
<td>**E3 Independent progress axes**</td>
<td>Commercial, design, production, fulfilment and payment progress move separately</td>
<td>A job can be printed but unpaid, or paid but undelivered, without a contradictory single "status"</td>
</tr>
<tr>
<td>**E4 Work list**</td>
<td>One ordered list: on the machine now → next → waiting on something</td>
<td>With one machine, this is the whole scheduling story</td>
</tr>
<tr>
<td>**E5 Milestones**</td>
<td>Only for genuinely phased work — a phase has a name, a deadline, and a done state</td>
<td>A phase turns red when its deadline passes with work outstanding. It is also the trigger for phase billing</td>
</tr>
<tr>
<td>**E6 Proof approval**</td>
<td>Show the proof in person, print it for signing, or — when email is configured — email it for approval. The shopkeeper chooses the channel per proof and reads any message before it goes. The approval is recorded with channel, date, who approved, and a captured signature or the signed sheet attached</td>
<td>The approved version and its signature are retained together, and the reference can be attached to the invoice pack</td>
</tr>
<tr>
<td>**E7 Delivery or pickup record**</td>
<td>Who collected it, when, and a captured **signature**</td>
<td>Ends "we never received it" disputes with evidence, not memory</td>
</tr>
<tr>
<td>**E8 Reprint**</td>
<td>Record a reprint against the original job, with the reason</td>
<td>Visible as a reprint, not as new revenue</td>
</tr>
<tr>
<td>**E9 Material reference (optional)**</td>
<td>Note which named material a job used, as a reference</td>
<td>Free-text-level detail. **No quantity, no deduction, no accounting**</td>
</tr>
</table>
### F — Money
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**F1 Invoice draft**</td>
<td>Built from one job or many. Lines, rates, quantities and off-catalog items all freely editable</td>
<td>Everything is changeable **before** issue — including adding items that are not in the catalog at all</td>
</tr>
<tr>
<td>**F2 Mark issued**</td>
<td>Explicit act. Locks the document, assigns the final number, makes it printable and sendable</td>
<td>After this point, the only correction is cancel and reissue</td>
</tr>
<tr>
<td>**F3 Cancel**</td>
<td>Cancel an issued invoice with a recorded reason</td>
<td>The document remains, keeps its number, and is visibly cancelled. Never deleted</td>
</tr>
<tr>
<td>**F4 Reissue**</td>
<td>A new invoice pre-filled from the cancelled one, with the rate, quantity or period corrected, permanently linked to the original</td>
<td>The trail reads: issued → cancelled (reason) → replaced by</td>
</tr>
<tr>
<td>**F5 Payment record**</td>
<td>Amount, date, method, reference</td>
<td>Recorded independently of any invoice</td>
</tr>
<tr>
<td>**F6 Manual allocation**</td>
<td>The shopkeeper decides which payment covers which invoice or job, including splitting one payment across several</td>
<td>Nothing is auto-matched. Unallocated money is visible and waiting, never hidden</td>
</tr>
<tr>
<td>**F7 Advance**</td>
<td>Money taken before work starts, held unallocated until there is something to apply it to</td>
<td>Visible as an advance on the customer screen</td>
</tr>
<tr>
<td>**F8 Receipt**</td>
<td>Printed or shared immediately on payment</td>
<td>Every receipt is reproducible later</td>
</tr>
<tr>
<td>**F9 Statement**</td>
<td>Everything charged, paid and outstanding for a customer over a period</td>
<td>Handed to an organization's accounts department without assembling it by hand</td>
</tr>
<tr>
<td>**F10 Aging view**</td>
<td>Who owes what, and for how long</td>
<td>The shopkeeper can see the oldest exposure first</td>
</tr>
<tr>
<td>**F11 Shop expense log**</td>
<td>Simple dated expenses with categories</td>
<td>A logbook for seeing money out against money in. **Explicitly not accounting**</td>
</tr>
</table>
<callout icon="🚫">
	**No refunds. No credit notes.** If an issued invoice is wrong, it is cancelled and reissued with the corrected rate, quantity or period. If it had already been paid, the payment simply becomes unallocated money on that customer, which the shopkeeper later applies to another job by hand. Money never flows back out of the shop through the system, and nothing re-allocates itself.
</callout>
### G — Arrangements and billing terms
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**G1 Rate agreement**</td>
<td>Built from an accepted quotation: which named products, at which rates, up to which quantity, valid for which period — with a fallback rule for anything outside the listed scope</td>
<td>Jobs for that customer automatically pull the agreed rate for the agreed name</td>
</tr>
<tr>
<td>**G1a Quantity cap**</td>
<td>An agreement line may cap total quantity — for example 5,000 cards at this rate. Each job under the agreement consumes against the cap</td>
<td>The agreement screen shows agreed / consumed / remaining per line. Nearing the cap raises an attention item; passing it needs an explicit override with a reason, or falls back to the catalog price</td>
</tr>
<tr>
<td>**G1b Named variants at differentiated rates**</td>
<td>The same physical product can be listed under several names — and the shopkeeper can give each name its own rate. Two names may share specifications and still be priced differently on purpose</td>
<td>The name the customer agreed to is the name on the job, the invoice and the agreement line. Nothing merges two names because their specs match</td>
</tr>
<tr>
<td>**G2 Rate lock**</td>
<td>A job created while the agreement is open keeps that rate even if the agreement later closes</td>
<td>A long-running job never silently reprices</td>
</tr>
<tr>
<td>**G3 Agreement lifecycle**</td>
<td>Draft → open → expiring → closed → superseded, with the reason recorded at each change</td>
<td>Closing an agreement asks explicitly what happens to jobs still running under it</td>
</tr>
<tr>
<td>**G4 New quotation revises or supersedes**</td>
<td>When new rates are agreed, the shopkeeper chooses: revise the existing agreement, or supersede it with a new one</td>
<td>The chain of agreements is readable end to end</td>
</tr>
<tr>
<td>**G5 Renewal**</td>
<td>Renewing pre-fills a new agreement from the expiring one; the renewal history is retained</td>
<td>Renewal is the same motion as reissue — one pattern, reused</td>
</tr>
<tr>
<td>**G6 Expiry attention**</td>
<td>Agreements approaching expiry appear as attention items; open-ended agreements do not nag</td>
<td>The shopkeeper is warned before a rate lapses, not after</td>
</tr>
<tr>
<td>**G7 Terms snapshot**</td>
<td>The agreement stores its own terms text at the time it was made</td>
<td>Editing a template later never rewrites a signed agreement</td>
</tr>
<tr>
<td>**G7a Signature and signed copy**</td>
<td>An agreement is recorded as signed: who signed, on what date, and either a captured signature or a photo/scan of the signed paper attached to the record</td>
<td>The signed artefact is retrievable from the agreement forever, and appears in the payout document pack</td>
</tr>
<tr>
<td>**G8 Credit account**</td>
<td>Per organization: credit limit, credit period, cycle day, and live running exposure</td>
<td>The shopkeeper knows the exposure before accepting the next job</td>
</tr>
<tr>
<td>**G9 Credit hold**</td>
<td>When the limit or period is breached, the customer goes on hold; taking the job anyway is an explicit override with a reason</td>
<td>The override is recorded, not silent, and the reason is retained</td>
</tr>
<tr>
<td>**G10 Cycle invoice**</td>
<td>On the cycle day, one consolidated invoice covering the period's jobs</td>
<td>Consolidation is a single action, and each source job remains traceable</td>
</tr>
<tr>
<td>**G11 Phase billing**</td>
<td>Bill per phase, per milestone, monthly, per delivery, or on completion</td>
<td>Billed and unbilled work are always distinguishable on the job</td>
</tr>
<tr>
<td>**G12 Payout document pack**</td>
<td>Invoice + job list + delivery proofs + approved-proof references + agreement reference + period + total, produced together</td>
<td>Regenerable on demand when an organization's finance team asks again</td>
</tr>
</table>
### H — Suppliers and sourcing memory
<callout icon="🧠">
	**Memory, not inventory.** The outcome is "when I need the same material again, I know exactly where it came from." Nothing here tracks a balance, deducts a quantity, or values stock.
</callout>
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**H1 Supplier profile**</td>
<td>Name, contact person, phone/WhatsApp, address, importer or local dealer, what they supply, notes on reliability and lead time</td>
<td>A supplier is found in two keystrokes with everything known about them</td>
</tr>
<tr>
<td>**H2 Named materials**</td>
<td>"300gsm matte card", "10oz flex", "PVC card blank", "certificate paper" — a name and a description</td>
<td>**No balance, no stock quantity, no consumption**</td>
</tr>
<tr>
<td>**H3 Purchase record**</td>
<td>Date, supplier, material, quantity, price paid, optional photo of the bill</td>
<td>Logged in under 30 seconds and it affects nothing else in the system</td>
</tr>
<tr>
<td>**H4 Sourcing lookup**</td>
<td>Open a material, see every purchase of it: supplier, date, quantity, price, newest first</td>
<td>Reorder identical material without phoning around to remember who had it</td>
</tr>
<tr>
<td>**H5 Cost-paid history**</td>
<td>What a material has cost over time</td>
<td>Feeds the shopkeeper's own decision on the bulk price edit screen. Never changes a selling price automatically</td>
</tr>
<tr>
<td>**H6 Paid / owed flag**</td>
<td>Confirmed: the shop buys on credit. Every purchase record carries paid or still owed, with the settle date once it clears</td>
<td>One screen answers "what do we still owe suppliers". A flag and a list, never a ledger</td>
</tr>
</table>
### I — Files and finding
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**I1 Content-based file identity**</td>
<td>Files are identified by device, volume, file ID and content hash — not by path</td>
<td>Renaming, moving or restoring a design file does not lose it</td>
</tr>
<tr>
<td>**I2 Asset versions**</td>
<td>The lineage of a design file over its revisions</td>
<td>The shopkeeper can find the version that was actually printed</td>
</tr>
<tr>
<td>**I3 Job ↔ file link**</td>
<td>Attach design files to a job or quotation</td>
<td>"Find the file for that job" is a lookup, not a search</td>
</tr>
<tr>
<td>**I4 Tags and specs as search keys**</td>
<td>Paper, size, finish and product type recorded on the job become findable later</td>
<td>"All the 300gsm card jobs for that school" is answerable</td>
</tr>
<tr>
<td>**I5 Reconciliation report**</td>
<td>Missing files, unlinked files, duplicates, offline volumes</td>
<td>The system says honestly when a file is unavailable rather than pretending</td>
</tr>
</table>
### J — Companion and attention
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**J1 Attention items**</td>
<td>Deterministic, rule-based: overdue jobs, expiring quotations, expiring agreements, credit limits approached, unpaid aged invoices, unbilled completed work</td>
<td>Same input always produces the same item. No duplicates from the same rule</td>
</tr>
<tr>
<td>**J2 Follow-up ladder**</td>
<td>Staged reminders by days overdue, with an explicit "do not chase this customer" flag</td>
<td>Collection becomes a routine, not a memory exercise</td>
</tr>
<tr>
<td>**J3 Manual reminder on any record**</td>
<td>Set a reminder with a date and a note on a customer, quotation, job, invoice or purchase</td>
<td>Works on anything, from anywhere</td>
</tr>
<tr>
<td>**J4 Recurring shop tasks**</td>
<td>Machine maintenance, monthly statement runs</td>
<td>Repeats on a schedule without being re-entered</td>
</tr>
<tr>
<td>**J5 Personal to-dos**</td>
<td>A plain list for the shopkeeper's own items</td>
<td>No structure imposed</td>
</tr>
<tr>
<td>**J6 Prepared messages**</td>
<td>The system drafts rate lists, statements, reminders and invoice emails — recipient, subject, attachment, correct references</td>
<td>**Nothing is sent until read and confirmed.** No scheduled sending, no send-on-event</td>
</tr>
<tr>
<td>**J7 Snooze**</td>
<td>Push an attention item to later with a reason</td>
<td>The item comes back; it does not disappear</td>
</tr>
</table>
### K — Interface and finding things
<table header-row="true">
<tr>
<td>Feature</td>
<td>Usage</td>
<td>Done means</td>
</tr>
<tr>
<td>**K1 Command palette**</td>
<td>One keystroke opens a box that creates records, finds anything, and jumps anywhere</td>
<td>The single most-used navigation surface. Replaces most menu traversal, and costs almost nothing in memory</td>
</tr>
<tr>
<td>**K2 Keyboard shortcuts**</td>
<td>Go-to shortcuts per record type, plus action shortcuts for the daily motions</td>
<td>A full counter sale is completable without touching the mouse</td>
</tr>
<tr>
<td>**K3 Saved views**</td>
<td>Filtered lists kept by name — "my overdue", "today's printing", "unbilled jobs"</td>
<td>The shopkeeper's own working lists, not fixed reports</td>
</tr>
<tr>
<td>**K4 Local search**</td>
<td>Search across customers, jobs, quotations, invoices and specifications</td>
<td>Returns in well under a second, offline</td>
</tr>
<tr>
<td>**K5 Soft delete and restore**</td>
<td>Deleting moves a record to a trash view where it can be restored</td>
<td>Permanent deletion is a separate, deliberate act</td>
</tr>
<tr>
<td>**K6 Dark mode**</td>
<td>Toggle</td>
<td>Shops run late</td>
</tr>
<tr>
<td>**K7 Honest degradation**</td>
<td>The interface states plainly when it is offline, when a volume is missing, when data is stale</td>
<td>The shopkeeper is never misled about what the system knows</td>
</tr>
</table>
---
## Document lifecycles
One pattern, reused everywhere. Learn it once, and quotations, invoices and agreements all behave the same way.
### The universal pattern
```plain text
draft  ──(mark issued)──>  issued  ──(cancel + reason)──>  cancelled
  │                          │                                │
  │ freely editable          │ locked, numbered               │ retained forever
  │ no number yet            │ printable, sendable            │ linked to replacement
  ▼                          ▼                                ▼
deleted (soft)          reissue ──> new draft, pre-filled, linked back
```
### Quotation
`draft → issued → (revision 2, 3 …) → accepted → converted to order`, or `→ expired`, or `→ declined (with reason)`.
Revisions stack rather than overwrite. The accepted revision is pinned exactly and never re-reads current prices.
### Invoice
`draft → issued → cancelled → replaced by`. There is no edit-after-issue and no refund branch. Corrections change the rate, the quantity or the period, and nothing else.
### Agreement
`draft → open → expiring → closed`, or `→ superseded by`. Renewal pre-fills a new agreement and keeps the history. Jobs created while open retain their locked rate regardless of what happens to the agreement afterwards.
### Job
Not a single status. Five independent axes — commercial, design, production, fulfilment, payment — each with its own progression, so the record can say "printed, delivered, unpaid" without contradiction.
---
## Native desktop performance — acceptance criteria, not aspirations
A desktop UI framework does not make an application light by default; a careless build can idle at several hundred megabytes. So the low-resource goal is written as numbers that a build can fail against.
<table header-row="true">
<tr>
<td>#</td>
<td>Budget</td>
<td>Why it is on this list</td>
</tr>
<tr>
<td>1</td>
<td>**Cold start under 1.5 seconds** to a usable window; warm start under 500 ms</td>
<td>The shopkeeper opens this app dozens of times a day</td>
</tr>
<tr>
<td>2</td>
<td>**Idle memory at or under 150 MB** with the local cache open — a hard ceiling</td>
<td>Shared with a design application on the same machine</td>
</tr>
<tr>
<td>3</td>
<td>**Fully usable with no server and no internet**</td>
<td>Offline is the default state, not a degraded mode</td>
</tr>
<tr>
<td>4</td>
<td>**Any list handles 100,000 rows** via virtualization and paged queries</td>
<td>No screen ever loads a whole table</td>
</tr>
<tr>
<td>5</td>
<td>**Local search under 100 ms**, and typing never blocks the interface</td>
<td>Search is the primary way things are found</td>
</tr>
<tr>
<td>6</td>
<td>**Runs acceptably on the stated hardware: 8 GB DDR4, Intel Core i5-7500 (4 cores, 4 threads, Kaby Lake), Intel HD 630 integrated graphics**, with a software-rendering fallback</td>
<td>Stated by the shopkeeper. A second desktop of similar specification is available to act as the server</td>
</tr>
<tr>
<td>7</td>
<td>**Idles at approximately 0% CPU** — event-driven only, no polling, no idle animation</td>
<td>Nobody should hear the fan because of a shop app</td>
</tr>
<tr>
<td>8</td>
<td>**Self-contained Windows deliverable** — an executable handed over directly, with its libraries alongside it. Nothing downloaded at run time, no embedded browser engine, nothing compiled on shop hardware</td>
<td>A shop PC must not be able to half-break the install. Note: Qt ships dynamically linked under LGPL, so the deliverable is a self-contained folder or installer rather than literally one file</td>
</tr>
</table>
**Design consequences that follow directly:** load screens lazily and unload hidden ones; keep data models in native code rather than the scripting layer; avoid heavy visual effects; never define records or fields at runtime through a metadata layer, because that trades a fast small binary for flexibility this shop does not need.
---
## Comparative research — what was taken from where
Feature research across Odoo, ERPNext, Perfex CRM, RISE CRM, Twenty CRM, and the print-MIS category. Recorded so a later reader knows which ideas were considered and rejected, not just which were adopted.
### Adopted
<table header-row="true">
<tr>
<td>Source</td>
<td>Feature</td>
<td>How it lands in SquiFlow</td>
</tr>
<tr>
<td>ERPNext</td>
<td>Draft → submitted → cancelled, with *amend* creating a new linked document</td>
<td>The universal document lifecycle above</td>
</tr>
<tr>
<td>ERPNext</td>
<td>Payment terms templates with instalments and milestones</td>
<td>G11 phase billing, F-series payment schedules</td>
</tr>
<tr>
<td>ERPNext</td>
<td>Credit limit with a precedence chain</td>
<td>G8, resolved customer → customer category → shop default</td>
</tr>
<tr>
<td>ERPNext</td>
<td>Payment entry: advances, part payments, one payment across many invoices</td>
<td>F6, F7 — but always allocated by hand</td>
</tr>
<tr>
<td>ERPNext</td>
<td>Job card as the record of what actually happened, separate from the plan</td>
<td>**Simplified away** — one machine, so the job ticket is the plan and there is no station log</td>
</tr>
<tr>
<td>ERPNext</td>
<td>Naming series with prefixes and date parts</td>
<td>A2 numbering patterns</td>
</tr>
<tr>
<td>ERPNext</td>
<td>Community insistence that the middle step be skippable</td>
<td>Decision 7 — any link in the chain can be skipped</td>
</tr>
<tr>
<td>Odoo</td>
<td>"Prices are suggested and always overridable on the order"</td>
<td>C6 resolution order plus C7 variance flag</td>
</tr>
<tr>
<td>Odoo</td>
<td>Blanket order — an agreement bounded by quantity as well as date, with consumption tracked</td>
<td>**Adopted** as G1a — quantity caps confirmed, with agreed / consumed / remaining shown per line</td>
</tr>
<tr>
<td>Odoo</td>
<td>Agreement built from a confirmed quotation, terms copied in</td>
<td>G1, G7 terms snapshot</td>
</tr>
<tr>
<td>Odoo</td>
<td>Follow-up ladder by days overdue, with an exclusion flag</td>
<td>J2</td>
</tr>
<tr>
<td>Perfex</td>
<td>Contract renewal that pre-fills and keeps renewal history</td>
<td>G5</td>
</tr>
<tr>
<td>Perfex</td>
<td>Contract expiry notifications; open-ended contracts do not nag</td>
<td>G6</td>
</tr>
<tr>
<td>Perfex</td>
<td>Reminders attachable to any record</td>
<td>J3</td>
</tr>
<tr>
<td>Perfex</td>
<td>Merging invoices</td>
<td>G10 cycle invoice</td>
</tr>
<tr>
<td>RISE</td>
<td>Milestones that turn red when a deadline passes with work outstanding</td>
<td>E5</td>
</tr>
<tr>
<td>RISE</td>
<td>Task cloning, recurring tasks, personal to-do list, per-event notification preferences, feature toggles</td>
<td>D8, J4, J5, A5</td>
</tr>
<tr>
<td>Twenty</td>
<td>Command palette and go-to shortcuts</td>
<td>K1, K2 — the highest-value UX import in the whole survey</td>
</tr>
<tr>
<td>Twenty</td>
<td>Saved filtered views; soft delete with a restore view; dark mode</td>
<td>K3, K5, K6</td>
</tr>
<tr>
<td>Print MIS</td>
<td>Printable job ticket with barcode; signature on collection</td>
<td>E2, E7</td>
</tr>
</table>
### Rejected, and why
<table header-row="true">
<tr>
<td>Rejected</td>
<td>Reason</td>
</tr>
<tr>
<td>Machine profiles, press selection, spoilage percentages</td>
<td>One machine. Nothing to compare or optimise</td>
</tr>
<tr>
<td>Material-driven pricing, wastage calculators</td>
<td>Paper consumption is parameter-dependent; a formula that is wrong half the time is worse than none</td>
</tr>
<tr>
<td>Station time logs, estimated-vs-actual job costing</td>
<td>No material or time capture exists to build it on</td>
</tr>
<tr>
<td>Inventory, stock valuation, purchase orders, goods receipt</td>
<td>Suppliers are a memory feature, not a supply chain</td>
</tr>
<tr>
<td>Credit notes, refunds, debit notes</td>
<td>The shop does not give money back</td>
</tr>
<tr>
<td>Automatic payment matching and reconciliation</td>
<td>Contradicts the human-supervised rule outright</td>
</tr>
<tr>
<td>Full accounting, general ledger, multi-currency, payroll, tax localizations</td>
<td>Each doubles the domain and serves nothing the shop asked for</td>
</tr>
<tr>
<td>Runtime-defined custom objects and fields</td>
<td>Destroys domain guarantees and is hostile to a small fast binary</td>
</tr>
<tr>
<td>Client portal, client chat, customer self-service, public request forms</td>
<td>Desktop-first shop counter. Proof approval is the only external touchpoint that earns its keep</td>
</tr>
<tr>
<td>Support tickets, knowledge base, surveys, goals tracking</td>
<td>Not a print shop's problem</td>
</tr>
<tr>
<td>Three separate documents for proposal, estimate and invoice</td>
<td>One lineage: quotation revision → order → invoice</td>
</tr>
<tr>
<td>Imposition, gang-run nesting, variable data printing, web-to-print storefront</td>
<td>Dedicated software territory</td>
</tr>
<tr>
<td>Mobile app, franchise reporting, third-party extension marketplace</td>
<td>Out of scope by decision</td>
</tr>
<tr>
<td>Autonomous AI actions on money, pricing, permissions, deletion or production completion</td>
<td>The shopkeeper supervises every necessary decision</td>
</tr>
</table>
---
## Delivery waves
Ordered so that each wave is independently useful at the counter.
<table header-row="true">
<tr>
<td>Wave</td>
<td>Contents</td>
</tr>
<tr>
<td>**W1 — Shop setup**</td>
<td>Shop profile, numbering patterns, **owner and staff users with sign-in, roles and enforced rights**, customers and organizations, contacts, billing profiles, supplier profiles, command palette and shortcuts, soft delete and restore</td>
</tr>
<tr>
<td>**W2 — Quote to job**</td>
<td>Catalog with specifications, remembered prices, bulk price editing, price notes, quotation revisions, issue/cancel/reissue, accept → order, fast-path intake, counter sale, clone from history, printable job ticket, local search, saved views</td>
</tr>
<tr>
<td>**W3A — Work**</td>
<td>Job records, the five progress axes, milestones for phased work, the single work list, proof approval, delivery and pickup with signature, reprints</td>
</tr>
<tr>
<td>**W3B — Money**</td>
<td>Invoice draft → issue → cancel → reissue, payment schedules, advances, manual allocation, receipts, statements, aging, rate agreements with rate lock, renewal chain, credit accounts and credit hold, cycle and phase invoices, payout packs, off-catalog lines, expense log</td>
</tr>
<tr>
<td>**W4 — Sourcing and files**</td>
<td>Named materials, purchase records, sourcing lookup, cost history, file identity, asset versions, tags, reconciliation</td>
</tr>
<tr>
<td>**W5 — Companion**</td>
<td>Attention rules, follow-up ladder, manual reminders, recurring tasks, personal to-dos, human-confirmed message drafts, richer search</td>
</tr>
<tr>
<td>**W6 — Resilience**</td>
<td>Offline queue, human conflict resolution, audit review, backup and restore, diagnostics</td>
</tr>
<tr>
<td>**W7 — Later**</td>
<td>Retainer and recurring billing, shop reporting, and only then automated estimation. *(Staff roles and permission editing moved out of this wave and into W1.)*</td>
</tr>
</table>
---
## Previously open questions — now answered
All three remaining feature questions are decided. There are no open feature questions.
1. **Agreement limits — answered: quantity caps exist.** An agreement line carries a rate, a validity period, and optionally a quantity cap. Consumption is tracked against the cap and shown as agreed / consumed / remaining. Going over needs an explicit override with a reason, or falls back to the catalog price. The same physical product may be listed under different names, and the shopkeeper may price each name differently on purpose — the name carries the rate. *(G1, G1a, G1b)*
2. **Proof and document approval — answered: email is an option, and signature is required.** When email is configured, the shopkeeper may choose to email a proof, quotation or agreement for approval; the message is still read before it is sent. Whatever the channel, the approval is recorded with a captured signature or the signed sheet attached, and the signed artefact stays with the record. *(E6, G7a)*
3. **Supplier credit — answered: yes.** Purchase records carry paid or still owed, plus the settle date once cleared, and one screen lists what is still owed to suppliers. Still a flag and a list — no supplier ledger, no payables aging, no automatic payment. *(H6)*
<callout icon="🧭">
	**Reading rule for anyone who inherits this page.** Where this page and any other SquiFlow document disagree about *what a feature does or how it is used*, this page wins. Where they disagree about *how something is built*, the parent implementation plan wins.
</callout>
