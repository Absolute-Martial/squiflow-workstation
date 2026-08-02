# SquiFlow — Full Implementation Plan

Source page id: 2e5a2b73cdde442cade0845eba62ef9d

---

<callout icon="🧭">
	**One project, three names.** *SquiFlow* is the current product name, *JobWright* was the previous name, and *octo-umbrella* is the GitHub repository name. This page consolidates every planning document across those names into a single implementation plan. Anything marked **Not implemented** is a plan, not a claim that it already runs.
</callout>
## Product framing
SquiFlow is a greenfield print-shop management system, built as a **shop companion** rather than a traditional management system that asks users to maintain it. The promise: **Craft less admin. Keep the work flowing.**
<callout icon="🏠">
	**Scope: the family business is the first tenant, not the only one.** Peer print shops run near-identical workflows, so the system is built to be adopted by them — but the family shop is the design partner, the pilot, and the veto. Multi-tenancy (RLS, tenant IDs, audit) is already in the schema and stays load-bearing. What is deferred is *commercial* machinery: billing, entitlement enforcement, self-serve signup, HA topology. What is **not** deferred is anything that would be expensive to retrofit — tenant isolation, per-tenant branding, per-tenant configuration and clean data export. Rule of thumb: build the seam now, build the storefront later. Where a decision trades "correct for many tenants" against "working for our shop this month," pick the shop — but never in a way that hardcodes the shop.
</callout>
It reduces effort by reusing verified customer, specification and file information; preserving context across quotation → order → production → delivery → payment; showing the next useful action beside the work; creating deterministic reminders; drafting messages and tasks for confirmation; making offline/degraded states visible; keeping cost and margin data out of staff projections.
### Companion, not autopilot
<table header-row="true">
<tr>
<td>Level</td>
<td>Behaviour</td>
</tr>
<tr>
<td>1. Observe</td>
<td>Organize verified records and surface attention items</td>
</tr>
<tr>
<td>2. Suggest</td>
<td>Recommend a next action with evidence</td>
</tr>
<tr>
<td>3. Draft</td>
<td>Prepare a task, message, quotation text or checklist for review</td>
</tr>
<tr>
<td>4. Execute</td>
<td>Only pre-approved, reversible housekeeping (indexing, notification retry)</td>
</tr>
</table>
Financial posting, pricing approval, customer proof approval, permissions, deletion and workflow override **always** require an authorized human plus normal application validation.
### White labeling and multi-business adaptation
A second shop must be onboarded by **configuration and a branding package — never by a code fork**. One build, many identities.
<table header-row="true">
<tr>
<td>Layer</td>
<td>What varies per business</td>
<td>Mechanism</td>
</tr>
<tr>
<td>Brand</td>
<td>Name, logo, colours, typography, favicon, installer name, window title, email/PDF letterhead</td>
<td>A signed *branding package* (manifest + assets) resolved at runtime; the C++26 `#embed` defaults are the fallback, not the only option</td>
</tr>
<tr>
<td>Documents</td>
<td>Quotation, invoice, receipt, delivery note, job card layouts and wording</td>
<td>Per-tenant template overrides layered over shared base templates</td>
</tr>
<tr>
<td>Commercial rules</td>
<td>Tax/PAN/VAT profile, numbering scheme, rounding policy, currency, due-day terms, discount ceilings</td>
<td>Tenant configuration records with schema validation and audit — never `if (tenant == …)` in code</td>
</tr>
<tr>
<td>Workflow</td>
<td>Enabled state axes, production operations, required approvals, proof steps</td>
<td>Per-tenant feature flags over a shared state machine; unknown flags fail closed</td>
</tr>
<tr>
<td>Vocabulary</td>
<td>Product/service names, units, Nepali/English UI strings</td>
<td>Tenant-scoped catalogue plus a translation overlay</td>
</tr>
<tr>
<td>Data</td>
<td>Everything</td>
<td>Existing RLS boundary; the asset index, LanceDB table and HF storage prefix are tenant-partitioned too</td>
</tr>
</table>
**Non-negotiables for the seam:** no tenant-specific branches in domain code; a tenant's full data must be exportable and re-importable on demand; every tenant is deletable; the branding package is validated and signed so a shop cannot inject arbitrary content into its own PDFs; and adding tenant #3 must cost configuration time, not engineering time.
**Deliberately deferred:** billing, metering, self-serve signup, per-tenant SLAs, marketplace extensions. Adoption is hand-held until at least two outside shops run happily.
---
## Technical baseline (what actually exists today)
A modular monolith with tenant-scoped in-memory and PostgreSQL adapters, migrating from C++23 to **C++26**.
**Verified foundation**
- C++23 source tree, independent CMake targets, warnings-as-errors, Debug/Release/sanitizer presets, layered CTest lanes, CI.
- Strong tenant/actor identifiers, exact minor-unit NPR money type, error/result contract, request context.
- PostgreSQL foundation + `002_identity_access` migrations: tenants, membership, audit, outbox, background-job ownership, RLS policies; migration checksums and a startup schema guard.
- OIDC RS256 verification against a bounded JWKS cache (issuer/audience/expiry/skew) mapped through an Oat++ request interceptor into a protected session endpoint.
- Parties/organizations/contacts, immutable quotation revisions, accepted-revision pinning, acceptance → order conversion with commercial snapshot.
- Append-only AR ledger: charges, payments, credits, reversals, allocations, immutable receipts, due-day aging, automatic FIFO credit application.
- Asset metadata catalog: immutable lineage, local file identity, typed relations, `IObjectStore` contract with SHA-256 verification and immutable keys.
- Podman Compose dev profiles and Quadlet production templates; vcpkg manifest with pinned registry.
**Explicitly not done**
- Production approval of the exact Oat++ dependency revision.
- Live Podman / PostgreSQL / MinIO execution (compile + automation evidence only).
- Qt Quick desktop, WSS sessions, object-store networking, search gateway, embedding worker.
- Any backup/restore rehearsal — this blocks real production data.
---
## C++26 migration
The whole tree moves to `-std=c++2c`. C++26 support in GCC and Clang is still **experimental**, so every use goes behind a feature-test macro with a C++23 fallback path, and CI pins exact compiler versions and builds both lanes until support stabilizes.
### Features to actually use, and where
<table header-row="true">
<tr>
<td>Feature</td>
<td>Where it earns its place</td>
</tr>
<tr>
<td>Contracts — `<contracts>`, P2900</td>
<td>Pre/postconditions on ledger invariants (an allocation never exceeds its charge; a revision is never mutated after acceptance). Checked in Debug and sanitizer builds, ignored in Release</td>
</tr>
<tr>
<td>`std::execution` senders/receivers — P2300</td>
<td>The bounded background executor: render jobs, hashing, embedding dispatch, outbox drain. Replaces ad-hoc thread pools with composable, cancellable pipelines</td>
</tr>
<tr>
<td>Pack indexing — P2662</td>
<td>Variadic DTO, property and SQL-binding helpers without recursive template chains</td>
</tr>
<tr>
<td>`#embed`</td>
<td>Compile SQL migrations, the OpenAPI schema, QML resources and default document templates straight into the binary — fewer loose files to lose on a shop PC</td>
</tr>
<tr>
<td>`std::inplace_vector`</td>
<td>Fixed-capacity, allocation-free buffers in the file agent's hot scan/hash loop</td>
</tr>
<tr>
<td>`std::simd`</td>
<td>Content hashing and pixel-difference work in the file agent</td>
</tr>
<tr>
<td>`std::hive`</td>
<td>Stable-reference registry for live WSS sessions (pointers survive insert/erase)</td>
</tr>
<tr>
<td>`= delete("reason")`</td>
<td>Money and identifier types reject wrong-unit or wrong-tenant construction with a readable compiler message</td>
</tr>
<tr>
<td>Checked integer arithmetic — `<stdckdint.h>`</td>
<td>Minor-unit NPR money overflow is a compile-visible concern, not a runtime surprise</td>
</tr>
<tr>
<td>Erroneous behaviour for uninitialized reads</td>
<td>Removes an entire class of UB from the ledger and asset code</td>
</tr>
<tr>
<td>`<debugging>`, `std::stacktrace`</td>
<td>Support-bundle diagnostics and breakpoint-if-debugging in the agent</td>
</tr>
<tr>
<td>Reflection — P2996</td>
<td>**Deliberately not depended on for v1.** Merged into GCC trunk and available in a Clang fork; would eliminate hand-written DTO/JSON/SQL mapping. Keep behind a flag and revisit when a release compiler ships it</td>
</tr>
</table>
### Migration rules
- One flag flip, then incremental adoption — do not rewrite working C++23 code just to use a new feature.
- Every C++26 use is guarded by its feature-test macro (`__cpp_pack_indexing`, `__cpp_contracts`, `__cpp_lib_inplace_vector`, …) with a tested fallback.
- Contracts must never be the only enforcement of a financial invariant — Release builds ignore them, so the repository layer still validates.
- CI keeps a C++23 lane green until the C++26 lane has run clean for a full release cycle.
---
## Windows desktop packaging and Qt licensing
Static linking is the right *technical* choice for a shop PC: one signed `.exe`, fast cold start, no DLL set to go missing, no Visual C++ redistributable surprise, and an installer that cannot be half-broken by a user copying the program folder. Nothing below argues against that goal. The problem is that with Qt, static linking is a **licensing decision before it is a build decision**.
### The constraint, stated precisely
Qt under the open-source licence is LGPLv3. The LGPL lets you keep your own source closed only while your application is "work that *uses* the library" — which dynamic linking clearly satisfies. The Qt Company's own guidance is blunt: *"In case of static linking of the library, the application itself may no longer be 'work that uses the library' and thus become subject to LGPL."* The FSF's position is that static linking is permissible **only if** you also ship your application in object form so a user can substitute their own Qt build and relink it.
So static + closed source + LGPL is not a combination that exists. Pick one of three real options:
<table header-row="true">
<tr>
<td>Option</td>
<td>What it costs</td>
<td>Verdict</td>
</tr>
<tr>
<td>**Dynamic link, LGPLv3**</td>
<td>Ship Qt DLLs via `windeployqt`, include Qt's source or a written offer for it, do not prevent relinking. Application source stays private. Free.</td>
<td>**Default for v1.** Costs an installer that copies more files — a solved problem.</td>
</tr>
<tr>
<td>**Static link, LGPLv3, relinkable**</td>
<td>Legal, but you must distribute your application's object files with every release so users can relink against their own Qt. That means publishing build artefacts and internal structure on an ongoing basis.</td>
<td>Technically compliant, operationally unpleasant. Not worth it.</td>
</tr>
<tr>
<td>**Static link, commercial Qt**</td>
<td>Qt for Small Business: eligibility is a registered business with combined revenue and funding at or below 1M EUR and no more than three developer seats. Application Development is roughly 546 EUR per developer per year.</td>
<td>**The correct answer before the product is sold or white-labeled.** A print shop qualifies comfortably.</td>
</tr>
</table>
### The educational licence cannot be used for this
This needs to be unambiguous, because it is the single largest legal exposure on this page. The Qt Educational EULA is a **personal, term-limited (12 month), learning-purpose** licence. It states that The Qt Company maintains separate versions of the software for commercial and for-profit purposes, and it forbids the licensee from transferring, publishing, sublicensing or otherwise making the licensed software available to third parties.
SquiFlow is software for a **family business that earns revenue**, and Phase 7 exists specifically to put it on **other companies' machines**. That is commercial distribution to third parties by any reading. "It started as an educational project" does not carry forward — the licence covers the use, not the origin story.
The consequence of getting this wrong is not a fine on day one. It is that the breach becomes discoverable at exactly the worst moment: when a second shop adopts the product, or when someone does diligence on the business. Retrofitting a licence after distribution is far harder than buying one before.
**Practical position:** learn and prototype on the educational licence if it is already in hand — that is what it is for. Ship the family shop's build **dynamically linked under LGPLv3**. Purchase Qt for Small Business **before the first external distribution**, and treat that purchase as a hard gate on Phase 7, at which point static linking becomes available for free.
### Other packaging consequences to remember
- **Module licences differ.** Qt Charts, Qt Virtual Keyboard and similar add-on modules are **GPLv3** under the open-source licence, not LGPL — using one would force the whole application open. Check every module against the licence table before adding it.
- **Static builds need explicit plugin imports.** Platform, image-format and style plugins are no longer discovered at runtime; they need `qt_import_plugins` / `Q_IMPORT_PLUGIN`, including the AVIF image plugin the search grid depends on.
- **Build the deployment path in CI from Phase 1E**, not at pilot time. Installer signing, update and migration rehearsal is already a Phase 4 gate.
---
## Modifying Oat++: patches, not a fork
The proposal is to clone Oat++, replace legacy internals with modern C++ under the hood, and keep the public API and macros byte-identical — tracking upstream with `git fetch upstream`. The **three rules attached to it are good engineering instincts**, and two of them survive. The mechanism does not.
### The three rules, assessed honestly
- **"Do not alter class memory footprints."** Sound instinct, **wrong justification.** Binary compatibility only matters when a pre-built library binary is swapped underneath an already-compiled application. SquiFlow does not do that: vcpkg builds Oat++ from source, in the same CMake build, with the same pinned compiler and the same `-std=c++2c` flag as everything else. **There is no ABI boundary to protect.** The rule is still worth keeping — but as a *merge-conflict* discipline, not an ABI one.
- **"Keep the interface identical."** Keep this, for the real reason: an unchanged interface means upstream's changes to *other* files still apply cleanly. The moment a signature changes, every future merge touches call sites across the tree.
- **"Use inline drop-in implementations."** **This is the genuinely valuable rule**, and it is valuable precisely because it minimizes the diff. A change confined inside one function body is a small, reviewable, re-appliable patch. A change that touches a class declaration is a permanent fork.
Notice what those three rules add up to: *make the change as small and as re-appliable as possible.* That is the definition of a patch. So use a patch.
### Why a maintained clone is the wrong container for it
Oat++ is Apache-2.0, so forking is entirely permitted — the obligation is only to preserve copyright and licence notices, and to state changes. The problem is not legal, it is operational.
- **A fork is a permanent tax.** Every upstream release becomes a merge you own, forever, including security fixes. The published industrial research on divergent forks is consistent: upstream-merge-induced conflicts are the dominant long-term cost, and they scale with how long the fork has diverged. This is a cost paid monthly for a benefit received once.
- **It makes an already-blocked decision worse.** "Oat++ dependency revision unapproved" is an open risk on this page and it blocks the entire P1 transport migration. Approving a pinned upstream revision is a decision someone can make. Approving *a fork of an unapproved revision, containing hand-written internal rewrites* is a much harder decision, and it lands on the critical path.
- **It fails the reproducibility rule already stated here.** A second repository whose state lives in someone's workspace is exactly the kind of environment this plan says must not exist. A patch file committed to our own tree is reproducible; a clone is a place where undocumented changes accumulate.
### The mechanism that gives the same result for a fraction of the cost
**vcpkg supports patches natively.** An overlay port passes a `PATCHES` list to `vcpkg_from_github`, applied to a pinned upstream `REF` at build time. That yields exactly what the fork was meant to yield, and several things it could not:
- Every modification is a **readable diff living in our repository**, reviewed like any other change.
- The upstream revision stays **pinned, unmodified and auditable** — so approving the dependency is once again a normal decision.
- Patches are **droppable**. When a fix lands upstream, delete the file. A fork has no equivalent of deleting a change.
- If a patch stops applying, **the build fails loudly** rather than silently drifting.
### What actually justifies a patch, and what does not
- **Justified: the tree moves to ****`-std=c++2c`****.** Oat++ is zero-dependency, deliberately conservative C++, and compiling it under an experimental C++26 mode is a real risk. Compilation breakage, a standard-library incompatibility or a sanitizer finding are legitimate reasons to patch, and this is the one to actually plan for.
- **Justified: a bug or a security fix** we need before upstream ships it — and it is sent upstream the same week.
- **Not justified: modernizing internals for their own sake.** Replacing legacy loops with modern language features inside a working, tested, zero-dependency framework delivers **nothing to the shop**: no feature, no measurable performance change on a workload bounded by network and PostgreSQL, and no reduction in risk. It consumes the scarcest resource on this project — the time of one student — on the one component that is currently blocking Phase 1. Meanwhile the failure mode is real: a subtle behavioural change inside a framework internal surfaces as an intermittent transport bug months later, and the fork is the first place nobody thinks to look.
**If the modernization is genuinely good, send it upstream.** A merged pull request costs one review cycle and then costs nothing forever; 8,600 stars' worth of other users test it for us. A private rewrite costs a merge every release and is tested by exactly one person.
**The position:** pin an upstream Oat++ revision, carry a small, documented, upstream-submitted patch set through a vcpkg overlay port, and treat any patch that outlives two upstream releases without being accepted as a signal to reconsider either the patch or the dependency.
---
## Deployment topology and the cost constraint
The hard constraint shapes the architecture more than any framework choice: **there is no static IP, no rented VPS, and no budget for always-on cloud compute.** The backend runs on hardware we already own.
### Topology
- **The server is our own machine**, sitting behind a residential/consumer connection with a dynamic address. Podman Quadlet units run Oat++, PostgreSQL, Caddy and the Python sidecar.
- **Tailscale is the network.** Every desktop client, every shop PC and every remote device joins the tailnet; the backend is reachable at a stable MagicDNS name regardless of what the ISP does to the public address. **No port forwarding, no dynamic-DNS hack, no exposed attack surface.** This is genuinely the right answer to "no static IP," not a workaround.
- **Free tier fits.** The Tailscale Personal plan is free indefinitely: up to 6 users, unlimited user devices, ACL groups and an initial allowance of tagged resources. A shop with a handful of staff and a server is comfortably inside it — but note that *tagged* resources (servers, subnet routers, service accounts) are the metered dimension, so tag deliberately.
- **ACLs are the authorization perimeter's outer ring**, not a replacement for it. Application auth (OIDC, RBAC, RLS) still assumes a hostile network. Tailscale removes exposure; it does not remove the need for authentication.
- **A subnet router** covers legacy devices that cannot run a Tailscale client (older printers, an existing NAS).
### When something must be public
Some things eventually need to reach a customer who will never install a VPN — a proof-approval link, a payment return URL, a webhook.
- **Tailscale Funnel** publishes one node's service to the internet over HTTPS with an automatically provisioned certificate, still with no static IP and no open inbound port. Good enough for webhooks and low-volume links.
- **Caveats:** Funnel gives one hostname per node, offers no WAF or rate limiting, and routes through Tailscale's relays. For anything customer-facing at volume, a **Cloudflare Tunnel** in front is the better choice — same no-static-IP property, plus caching, rate limiting and DDoS protection on a free plan.
- **Default posture:** internal by default over the tailnet; public exposure is an explicit, reviewed, per-endpoint decision.
### Getting the most out of free tiers
<table header-row="true">
<tr>
<td>Need</td>
<td>Free option</td>
<td>The catch to design around</td>
</tr>
<tr>
<td>Private networking</td>
<td>Tailscale Personal</td>
<td>6 users; tagged resources are the metered axis</td>
</tr>
<tr>
<td>Public endpoints</td>
<td>Tailscale Funnel, or Cloudflare Tunnel</td>
<td>Funnel: one hostname per node, no WAF</td>
</tr>
<tr>
<td>Cold blob tier</td>
<td>Hugging Face private dataset repo</td>
<td>Free private quota is limited; file-count limits; not a backup</td>
</tr>
<tr>
<td>Offsite backup</td>
<td>Cloudflare R2 free tier (10 GB, zero egress) or Backblaze B2</td>
<td>Small free quota — back up the database and metadata, not the design originals</td>
</tr>
<tr>
<td>Backup tooling</td>
<td>restic (encrypted, deduplicated, incremental) driven by rclone</td>
<td>Deduplication is what makes a 10 GB quota viable; watch API-call counts</td>
</tr>
<tr>
<td>GPU inference</td>
<td>Modal Starter: \$0/month plus monthly free credit, scale to zero</td>
<td>Billed per GPU-second, so batch tightly; cold starts; free credit is monthly, not cumulative</td>
</tr>
<tr>
<td>CI</td>
<td>Free CI minutes on the hosted runner</td>
<td>Cache aggressively (sccache, vcpkg binary cache) or minutes evaporate</td>
</tr>
</table>
**The rule:** every free tier is treated as a resource with a **hard ceiling and a defined degradation path**, never as "unlimited until it breaks." Each one gets a monitored usage metric, an alert threshold, and a documented answer to "what happens when it runs out."
### Backup, layered
Free quotas are too small for design originals, so backup is tiered rather than uniform:
1. **Tier 1 — irreplaceable, tiny:** PostgreSQL dump, tenant configuration, branding packages, asset *metadata*. Encrypted with restic, pushed nightly to an offsite object store. Fits a free tier easily. This is the tier that must be restore-rehearsed.
2. **Tier 2 — large, regenerable:** AVIF derivatives, LanceDB index, embeddings. Stored on Hugging Face, and explicitly **rebuildable from originals** — losing this tier costs GPU time, not information.
3. **Tier 3 — large, irreplaceable:** the design originals. These stay on shop hardware with a rotating local/offline copy (external drive or NAS). Cloud storage of originals is a paid decision to be taken deliberately, not smuggled in through a free tier.
This mapping is why the tiering matters: **the only thing that must survive on someone else's infrastructure is small.**
### Minimizing GPU time on principle
GPU minutes are the one genuinely metered, genuinely expensive resource, so they are rationed by design, not by discipline:
- Nothing reaches the GPU until the **free signals are exhausted** — filename, folder, layer names, text layers, EXIF/XMP and metadata FTS run locally and answer a real share of queries at zero cost.
- Embedding is **batched nightly**, content-hash idempotent, and never triggered per file save.
- **There is no reranker**, so there is exactly one model on the GPU and one cold start per run. Retrieval returns 50 candidates and shows 10 purely so the option stays open.
- **Scale to zero** with memory snapshotting; no warm pool outside shop hours.
- A **hard monthly GPU-minute ceiling** degrades the system to FTS-only rather than overspending.
- Reindexing is an **explicit, budgeted decision**, tied to a pinned model revision — not a side effect of a deploy.
- **The arithmetic that makes this free.** An L4 on Modal runs around \$0.80/hour, and Modal's Starter plan includes a monthly free credit on the order of \$30 — roughly **35–40 L4-hours per month**, or over an hour of GPU time every single night. A nightly batch embedding a normal day's new and changed files does not come close to that ceiling. **The steady-state running cost of visual search is realistically zero**, and the budget cap exists to catch the abnormal case: a bulk import, a full reindex, or a bug. This is also precisely why an H100 is the wrong choice — at roughly 5x the rate, the same free credit buys about 7 hours instead of 37.
### When there is no payment card
Being a student without a usable Visa card is a real constraint, not an excuse to bend anyone's terms. Two things follow, and the first matters more than the second.
**Almost nothing in Phase 5 needs a GPU at all.** Steps 1, 2, 3, 5, 7 and 8 — agent discovery, PSD rendering, AVIF derivatives, metadata extraction, the LanceDB table, the search API and the Qt grid — are local, free and card-free. They are already first in the delivery order, and metadata FTS alone resolves a real share of "where is that file." There are **months of work here before a payment method matters at all.** The card is not on the critical path; treating it as a blocker would stall the project over a step that comes late.
**And when the GPU tier does arrive, there are card-free routes:**
<table header-row="true">
<tr>
<td>Option</td>
<td>Card required</td>
<td>What it gives</td>
<td>Terms fit</td>
</tr>
<tr>
<td>**The shop's own CPU**</td>
<td>No</td>
<td>A small embedding model, slow per image but perfectly shaped for a nightly batch</td>
<td>Total control, no account, no quota</td>
</tr>
<tr>
<td>**Lightning AI free tier**</td>
<td>No — phone verification instead</td>
<td>**CPU only, in practice.** An earlier draft of this page claimed 80 free GPU hours *per month*, taken from Lightning's marketing copy. That is wrong. The free GPU credit is **one-time, not recurring**, and this account has already been degraded to CPU-only. The marketing page and the delivered account do not agree, and the delivered account is the fact</td>
<td>Fine as a free CPU box; **not a GPU source**</td>
</tr>
<tr>
<td>**Modal Starter**</td>
<td>Reportedly not required for the free credit — verify at signup</td>
<td>\$30/month credit, per-second billing, scale to zero</td>
<td>Built for programmatic job submission</td>
</tr>
<tr>
<td>**Modal for Academics**</td>
<td>Application-based</td>
<td>Substantial research credits for students and academics</td>
<td>Worth an application with a student email</td>
</tr>
</table>
**Correction, recorded deliberately.** This page previously argued that Lightning AI could be the GPU worker itself and therefore made Colab and Kaggle unnecessary. That argument was built on a vendor's marketing page and it does not survive contact with a real account: the GPU allowance is one-time, it has already been consumed, and what remains is a free CPU. **Provider free-tier claims are marketing until an account proves them.** Every free tier in the table above should be treated as unverified until someone has actually run a job on it.
### Interim GPU: the notebook platforms, with eyes open
With Lightning's GPU gone and no card, the remaining zero-cost GPU is a notebook platform. The decision recorded here is the one chosen: **Colab or Kaggle during development, Modal from the first paying customer.** The objections on this page are not withdrawn, so they are written down rather than argued again.
- **Development and evaluation use is unambiguously fine.** Building the quantized checkpoint, running the gold set, benchmarking Recall@10 against Recall@50, and proving the pipeline end to end are exactly what an interactive notebook is for. A student prototyping on Colab is the intended user. **Nothing in Phase 5 before the pilot needs more than this.**
- **Production use is the part that carries risk.** Kaggle's terms limit it to *personal, non-commercial* use, and Colab restricts headless automation. Once real customer designs are being indexed for a revenue-generating shop on a recurring schedule, that is production, and the risk is not a fine — it is the pipeline stopping without notice.
- **The mitigation is the account boundary.** If a notebook platform is used at all, it must not share a Google account with the Drive backup. Compute and disaster recovery behind one identity means one enforcement action takes out both. This is non-negotiable and costs nothing.
- **The exit is already funded by the trigger.** First paying customer → Modal. That is the right trigger, because the moment revenue exists is precisely the moment the terms problem becomes real and the money to solve it appears.
#### The blocker nobody expects: FP8 will not run on free notebook GPUs
This is a hardware fact, not a preference. **FP8 W8A8 requires compute capability 8.9 or higher — Ada Lovelace or Hopper.** What the free tiers actually hand out:
<table header-row="true">
<tr>
<td>Platform</td>
<td>GPU you get</td>
<td>Compute capability</td>
<td>FP8?</td>
</tr>
<tr>
<td>Colab free</td>
<td>T4, 16 GB</td>
<td>7.5 (Turing)</td>
<td>**No.** Weight-only W8A16 via FP8 Marlin at best — no activation speedup</td>
</tr>
<tr>
<td>Kaggle free</td>
<td>P100 16 GB, or T4 x2</td>
<td>6.0 / 7.5</td>
<td>**No.** P100 predates tensor cores entirely</td>
</tr>
<tr>
<td>Modal (later)</td>
<td>L4 / A10 / L40S, 24–48 GB</td>
<td>8.6 / 8.9</td>
<td>L4 and L40S yes; A10 weight-only</td>
</tr>
</table>
So **"Qwen3-VL-Embedding-8B in FP8 on Colab or Kaggle" is not a configuration that exists.** Three honest ways forward, all worth considering before committing:
1. **INT8 W8A8 on T4.** Turing has INT8 tensor cores. Weights land near 9 GB, leaving \~7 GB on a 16 GB card for the vision tower's activations and KV cache. Workable, but genuinely tight — a capped 1024 px render and a bounded context are load-bearing, not optional.
2. **Kaggle's T4 x2, tensor-parallel.** 32 GB total removes the memory anxiety at the cost of multi-GPU setup, and Kaggle's 30 h/week quota is more predictable than Colab's dynamic allocation.
3. **INT4 AWQ or GPTQ.** Roughly 5 GB of weights, comfortable on any 16 GB card, at a real accuracy cost that only the gold set can price.
**The rule this suggests: the quantization format is a function of the hardware you actually get, not a decision made in advance.** INT8 remains the portable default precisely because it runs on Turing through Hopper alike; FP8 becomes available only once the pipeline is on Modal, and even then buys nothing on a nightly batch with no latency target.
**The buffer does not need to be rented either.** The accumulation-threshold pattern is genuinely the right idea, and it is already in this plan. But the always-on shop server *is* the waiting room: it already has the pending queue, content-hash idempotency and bounded backpressure specified. Renting a third-party studio to hold files adds a hop, an account and a failure mode to the one component that must stay reliable for everything else to run.
#### Two corrections to the storage picture
- **LanceDB does not hold the raw images.** It holds vectors, metadata and IDs. AVIF derivatives live on local disk and in the Hugging Face dataset repo; originals never leave the shop. Putting image bytes in the table would inflate it by orders of magnitude and destroy the exact storage economics AVIF was chosen for.
- **The 15 GB Drive is a Tier 1 destination, not an image store.** Tier 1 is the database dump, tenant configuration, branding packages and asset *metadata* — small, encrypted, irreplaceable. Images are Tier 2 (regenerable) and Tier 3 (originals, kept locally). Sizing Drive against image volume optimizes the wrong tier and would fill it with the one category of data that can always be rebuilt.
### The free-compute mesh: what is adopted, and what is rejected
A multi-provider design was proposed — a Lightning AI CPU orchestrator triggering Google Colab GPU workers, writing to hosted LanceDB, backed up weekly to Google Drive, with randomized delays to avoid platform detection. Parts of it are genuinely good and are adopted. Parts of it are rejected, and the reasons matter more than the verdict.
**Adopted:**
- **Rebuildable-from-code environments.** If a workspace, container or remote repository is wiped, the system should reconstruct it from declarative definitions rather than from someone's memory. This is already the direction of travel (Podman Quadlet units, pinned vcpkg registry, content-hash idempotency) and it should be stated as a requirement: **no environment is allowed to be irreproducible.**
- **Weekly compressed snapshot to free cloud storage.** A 15 GB Google Drive is a perfectly reasonable *additional* Tier 1 destination via rclone. One correction: use **restic rather than ****`tar -czf`**. Encrypted, deduplicated and incremental beats a weekly full tarball on every axis, and customer design metadata must not sit unencrypted in a personal Drive account.
- **Lance as the format bet.** Open format, CDC support and columnar snapshots are exactly why the index can be treated as regenerable and portable.
- **Jitter on retries.** Legitimate and already in the plan — it spreads backpressure and prevents thundering-herd retries.
**Rejected — and not for style reasons:**
- **Google Colab as a production backend.** Colab is an interactive notebook service; its own FAQ states that it prioritizes users *actively programming in a notebook* and restricts actions associated with bypassing its anti-abuse policies. There is no supported job-submission API for headless production work. Building a revenue-generating shop's search pipeline on it means the pipeline can stop working because of a policy change, a usage limit or an account action — and the account at risk is likely the same Google account holding the Drive backup. That is a **correlated failure between compute and disaster recovery**, which is the one dependency a DR plan may never have.
- **Kaggle as compute.** Kaggle's Terms of Use limit the service to *"your own internal, personal, non-commercial use, and not on behalf of or for the benefit of any third party."* A family business — and later, other shops — is neither personal nor non-commercial. This one is not a grey area.
- **Detection evasion, entirely.** The proposal frames randomized jitter and CLI transport as making the automation "invisible to platform scraping detectors." That is not a resiliency control; it is circumventing enforcement of the terms above, and it is removed from the design rather than relocated. The useful heuristic: **if a workload has to be concealed from the provider hosting it, it is on the wrong provider.** Jitter stays for backpressure only.
- **Hosted LanceDB for live queries.** This contradicts a decision already made for good reasons. The index is small, embedded, on the same machine as the API, and already snapshotted offsite. A hosted store would add a network dependency, a vendor and a monthly bill in exchange for nothing that the local table does not already do.
- **A third-party CPU orchestrator.** The always-on home server exists and costs nothing to schedule on. Moving the scheduler to Lightning AI adds an external failure mode to the one component that must run reliably for everything else to happen.
**Where the "100% free" claim actually breaks.** It is not free — the cost is moved onto providers whose terms exclude this use, and it is repaid as the risk of losing a production system with no notice. The honest version is already on this page: **Modal's Starter credit covers the nightly batch outright**, Modal is explicitly built for programmatic job submission, and it scales to zero. That is the same zero rupees per month, inside the terms, with an invoice-able relationship if the shop ever outgrows it. **Free tiers used as designed are an asset; free tiers used against their terms are undisclosed debt.**
---
## Phase plan
### Phase 0 — Discovery and evidence ✅
**Objective:** validate real workflows, legal/accounting boundaries and pilot constraints before freezing the domain schema.
- 0A — observe 10–20 real jobs; collect redacted quotations, statements, files, delivery examples.
- 0B — approve glossary, role matrix, independent workflow axes, genuine pricing examples.
- 0C — validate PAN/VAT, numbering, payment, rounding and accounting boundaries with qualified advisers.
- 0D — measure users, devices, file sizes, growth, downtime tolerance, minimum backup capability.
**Acceptance:** owners can explain every selected state, calculation and exception; unresolved assumptions have named owners and release gates.
### Phase 1 — Foundation, brand, identity and tenant safety 🔄 *active*
**Objective:** establish the production runway and the first authoritative tenant-isolated vertical slice.
<table header-row="true">
<tr>
<td>Sub-phase</td>
<td>Scope</td>
<td>Status</td>
</tr>
<tr>
<td>1A</td>
<td>Official brand, source/public repo split, governance, CI, contracts, Podman foundation</td>
<td>✅ Complete (2026-07-12)</td>
</tr>
<tr>
<td>1B</td>
<td>Tenant enrollment, OIDC Auth Code + PKCE, sessions, request context, role/permission policy</td>
<td>🔄 Partial</td>
</tr>
<tr>
<td>1C</td>
<td>PostgreSQL repositories, non-owner RLS sandbox, audit/outbox, concurrency-safe numbering</td>
<td>🔄 Partial</td>
</tr>
<tr>
<td>1D</td>
<td>Oat++ controllers/interceptors/problem responses, bounded executor, Caddy edge</td>
<td>⬜ Decided, not implemented</td>
</tr>
<tr>
<td>1E</td>
<td>Qt Quick shell, session service, design system, typed HTTPS/WSS client</td>
<td>⬜ Not implemented</td>
</tr>
</table>
**Remaining 1B work**
1. OIDC discovery/JWKS HTTP refresh, bounded retry, cache-control lifetime, unknown-key rotation.
2. PostgreSQL tenant, membership, role and permission repositories over the existing RLS schema.
3. RFC 9457 problem responses plus protected-controller integration tests for valid bearer tokens.
4. Atomic authoritative mutation + audit + outbox insertion through the persistence adapter.
**Remaining 1C/1D work**
1. Adapt official Oat++ PostgreSQL component patterns behind SquiFlow persistence interfaces.
2. Authenticated WebSocket connection registry: tenant, actor, device, last sequence, bounded outbound queue, heartbeat, disconnect, graceful drain.
3. Keep OpenAPI in `squiflow-protocol` authoritative; Swagger stays development-only and is diffed against the published surface.
4. Caddy handles public TLS/HTTP negotiation and private upstream proxying; the server never receives container-engine credentials.
**Acceptance:** a disposable stack creates two tenants; authenticated clients reach only their own tenant; authoritative mutation + audit + outbox is atomic; HTTPS and WSS contracts pass; the desktop can enroll and show connection state.
### Phase 2 — Quote-to-order companion slice 🔄 *exit criterion not met*
**Objective:** create, revise, find, print and accept real quotations with minimal repeated entry.
<table header-row="true">
<tr>
<td>Deliverable</td>
<td>Status</td>
</tr>
<tr>
<td>Parties, organizations, contacts</td>
<td>✅ Implemented</td>
</tr>
<tr>
<td>Quotation revisions and exact acceptance</td>
<td>✅ Implemented</td>
</tr>
<tr>
<td>Acceptance → order conversion</td>
<td>✅ Implemented</td>
</tr>
<tr>
<td>Product templates and typed specifications</td>
<td>⬜ Not implemented</td>
</tr>
<tr>
<td>Deterministic pricing and rounding policy</td>
<td>⬜ Not implemented</td>
</tr>
<tr>
<td>PDF quotation rendering from real samples</td>
<td>⬜ Not implemented</td>
</tr>
<tr>
<td>Local FTS5 search and repeat-order cloning</td>
<td>⬜ Not implemented</td>
</tr>
<tr>
<td>Contextual tasks and user-confirmed suggestions</td>
<td>⬜ Not implemented</td>
</tr>
</table>
**Acceptance:** owners reproduce approved samples, find a prior customer/job, print a correct quotation, and convert one exact accepted revision to an order — without later template changes altering history.
### Phase 3 — Job-to-cash and file continuity 🔄 *active tranche*
**Objective:** operate design, production, fulfilment and receivables without contradictory status or file loss.
<table header-row="true">
<tr>
<td>Deliverable</td>
<td>Status</td>
<td>Remaining boundary</td>
</tr>
<tr>
<td>Direct order and independent state axes</td>
<td>🔄 Partial</td>
<td>Change orders, production operations, delivery evidence missing</td>
</tr>
<tr>
<td>Charges, payments, allocations, reversals</td>
<td>✅ AR core</td>
<td>In-memory and PostgreSQL</td>
</tr>
<tr>
<td>Receipts and aging</td>
<td>✅ Implemented</td>
<td>Due-day aging from unpaid charge allocations</td>
</tr>
<tr>
<td>Order payment integration</td>
<td>🔄 Partial</td>
<td>Cancellation accounting and one shared order+AR transaction boundary remain</td>
</tr>
<tr>
<td>Confidential cost/margin and discount approval</td>
<td>⬜ Not implemented</td>
<td>No confidential projection or approval workflow</td>
</tr>
<tr>
<td>Logical asset/version/file/relation model</td>
<td>✅ Implemented</td>
<td>Metadata-only catalog, immutable lineage</td>
</tr>
<tr>
<td>Hosted asset persistence</td>
<td>🟡 Compile-verified</td>
<td>`PostgresAssetCatalog`, RLS, audit/outbox build; no live run</td>
</tr>
<tr>
<td>Local Windows file agent</td>
<td>⬜ Not implemented</td>
<td>No NTFS File ID reader, USN checkpoints, scanner, hasher, thumbnailer</td>
</tr>
<tr>
<td>Local asset search and reconciliation</td>
<td>⬜ Not implemented</td>
<td>No FTS index or duplicate/missing reports</td>
</tr>
</table>
**Acceptance:** selected historical jobs replay end to end; related files are found without opening every source; balances and allocations match expected statements; unauthorized staff cannot infer restricted financial data.
### Phase 4 — Pilot hardening ⬜
**Objective:** run one controlled production pilot with parallel manual records and recoverable operations.
Staff-limited projections and permission tests · installer signing / update / migration rehearsal · import with preview · ClamAV ingest scanning · secrets management · observability and health dashboard · backup age and failure visibility · clean-host restore rehearsal · diagnostics and redacted support bundles · training and daily defect triage.
**Acceptance:** zero unexplained balance mismatch over two weeks of parallel operation, one verified clean restore, critical support paths documented, and residual single-server/downtime risk explicitly accepted.
### Phase 5 — Visual design-file search ⬜
> **The actual problem:** the design file exists. It is somewhere on the shop drive. Nobody remembers the filename, the folder, or the year. `final_v3_FINAL (2).psd` is not a search key. Filename and full-text search cannot find it, so someone opens folders for twenty minutes or just redesigns it.
**Objective:** find a past design by describing it, or by showing something that looks like it — with no reliance on filename or folder discipline.
<callout icon="🧱">
	**The build order for this phase lives on its own page:** [SquiFlow — Design-File Search: Execution Plan](https://app.notion.com/p/d01adc14460b4a1e9fac575b7ed8cbcf). This section states *what the pipeline is*; that page states *the order it gets built in*, milestone by milestone, with an acceptance test and a gate for each. Where this page leaves a decision open, the execution plan names the measurement that closes it rather than guessing.
</callout>
#### Pipeline
**1. Discover — C++26 Windows agent**
NTFS File ID plus USN journal checkpoints give a stable logical identity that survives rename, move within a volume, copy and restore. This is the same agent already scoped in Phase 3; visual search is a consumer of it, not a second crawler. Watched roots, extension filter (`.psd`, `.psb`, `.ai`, `.cdr`, `.tif`, `.pdf`, `.jpg`), incremental content hashing, and an explicit "volume unavailable" state rather than silent deletion.
**2. Render and normalize — Python sidecar**
- `psd-tools` opens PSD/PSB and composites a flattened preview (`psd-tools[composite]` for vector shapes, gradients and layer effects; falls back to the embedded cached preview when compositing is too slow).
- Extract the free text that already exists inside the file: **layer names, text-layer contents, fonts, colour mode, dimensions, spot colours, XMP/EXIF**. This is often the single highest-value signal — a layer literally named "Hotel Yak wedding card" beats any embedding.
- Produce **AVIF** derivatives at three sizes: 256 px grid thumbnail, 1024 px search render (what the embedding model sees), 2048 px proof view. AVIF is chosen for the storage math — typically several times smaller than PNG/JPEG at equal perceptual quality, which is what makes the whole archive fit.
- Non-PSD formats route through the same normalizer (Ghostscript/pdftoppm for PDF/AI, ImageMagick for TIFF).
**3. Store — Hugging Face as the cold blob tier**
Local storage is the binding constraint, so AVIF derivatives and LanceDB snapshots live in a **private Hugging Face dataset repository** (Xet-backed), with a local disk cache for hot thumbnails.
<callout icon="⚠️">
	**Hugging Face limits to design around.** A Space's git repo is capped around **1 GB** — so the Space holds app code only and is never the blob store; put blobs in a *dataset* repo. Free-tier **private** storage is \~100 GB (PRO adds \~1 TB); public quotas are larger but the family business's customer designs must stay private. Keep total files under \~100k and never more than 10k files per folder — use sharded subdirectories, or pack derivatives into Parquet/WebDataset shards. And HF is **not a backup**: an independent cold copy of originals is still required.
</callout>
Originals never leave the shop. Only derived AVIF renders and metadata are uploaded, which also keeps the privacy blast radius small.
**4. Embed — two tiers, because the GPU tier needs a payment method**
**Tier 1, available now: a small multilingual model on the shop's own CPU.** SigLIP 2 ships at ViT-B (86M), L (303M) and So400m (400M) parameters, was trained on 109-language data, is openly licensed, and runs acceptably on CPU through ONNX Runtime. For a nightly batch of a few dozen to a few hundred renders, CPU throughput is simply not the binding constraint. It will not match an 8B model — and it does not have to. The gold set decides whether the gap matters, and a working search that is merely good beats an excellent one that is blocked on a bank.
This makes the **embedder a swappable interface**, exactly like the ranking step. Two seams, one principle: the expensive component is always replaceable without a redesign.
<callout icon="⚖️">
	**Licence warning on the obvious alternative.** `jina-clip-v2` looks ideal on paper — 89 languages, Matryoshka dimensions, 512 px images — but it is released under **CC BY-NC 4.0, which forbids commercial use.** A revenue-generating print shop cannot use it without a commercial licence from Jina. This is the same category of mistake as the Qt educational licence, and model licences must be checked with the same discipline as library licences.
</callout>
**Tier 2, interim: Qwen3-VL-Embedding-8B on a free notebook GPU.** Colab or Kaggle with internet enabled, used for checkpoint building, gold-set evaluation and pre-pilot indexing. **Format is INT8, not FP8** — see the hardware table above; free notebook GPUs are Turing or Pascal and cannot do FP8 W8A8 at all. Treat this tier as *evaluation infrastructure that happens to also index files*, not as production.
#### Choosing the pre-built checkpoint
A correction first: **this model is not "the only one quantized."** Hugging Face lists roughly **21 quantizations** of `Qwen/Qwen3-VL-Embedding-8B`, including `gonuit/Qwen3-VL-Embedding-8B-AWQ-4bit`, several GGUF conversions and an MLX build. That matters, because the two candidates raised are not interchangeable and one of them cannot do the job at all.
<table header-row="true">
<tr>
<td>Checkpoint</td>
<td>What it actually is</td>
<td>Runs where</td>
<td>Verdict</td>
</tr>
<tr>
<td>`QuaduxIT/Qwen3-VL-Embedding-8B-W8A16`</td>
<td>Weight-only 8-bit, **activations stay FP16**. Built with `llm-compressor` for direct vLLM serving; relies on the **Marlin** kernel family, CUDA only</td>
<td>**T4 included.** Marlin weight-only runs on Turing, which is exactly the hardware Colab hands out</td>
<td>**The right interim choice**, and better than it looks — see below</td>
</tr>
<tr>
<td>`dam2452/Qwen3-VL-Embedding-8B-GGUF` (Q8_0)</td>
<td>llama.cpp CPU format, roughly 7.5 GB</td>
<td>Any CPU, including a Lightning CPU studio</td>
<td>**Rejected for v1** — the image path does not work. See the blocker below</td>
</tr>
<tr>
<td>Our own `llm-compressor` INT8 W8A8</td>
<td>Calibrated on real shop renders and real query phrasings</td>
<td>Ada or newer, so Modal only</td>
<td>The eventual answer, at the Modal tier</td>
</tr>
</table>
**W8A16 is a better fit here than W8A8, not a compromise.** Two reasons. First, it is the format that *runs* — W8A8 wants compute capability above 7.5 and Colab's T4 sits exactly at 7.5, whereas Marlin weight-only is supported on Turing. Second, leaving activations in FP16 usually costs **less accuracy** than quantizing them, and accuracy is the whole point of a retrieval model. What W8A16 gives up is throughput: memory drops to roughly 9 GB but there is no activation-side speedup. **On a nightly batch with no latency target, throughput is the one thing we can afford to lose.**
<callout icon="⛔">
	**Why the GGUF cannot be v1: it embeds text, not images.** llama.cpp's support for Qwen3-VL-Embedding *multimodal* embeddings is still an open draft pull request, and the community model cards say plainly that image embedding needs a patched `mtmd` multimodal build rather than a stock binary. A text-only embedder deletes the entire feature — "find the blue wedding card by describing it" is the requirement, and that is the image path. There is also a documented history of **GGUF embedding conversions being quietly broken by pooling-configuration errors**, with one reported benchmark showing a Qwen3 embedding GGUF at 18.7% against 53–60% for the same class of model served properly. A silently degraded embedder is the worst possible failure here, because search still returns results — just bad ones.
</callout>
**Two conditions on using any third-party checkpoint.** Its calibration data is unknown to us, so (a) it is **pinned by revision hash**, never by tag, and (b) it is **validated on the gold set before it is trusted**, exactly as the page already requires for our own quantized builds. A community quant is a convenience, not a warranty.
**Tier 3, from the first paying customer: the same model on Modal.** Revenue is the trigger, and it arrives at the same moment the terms problem stops being theoretical. Everything below describes this tier.
- `Qwen/Qwen3-VL-Embedding-8B`: multimodal (text, image, screenshot, video), 32K context, **30+ languages** including Nepali, and a Matryoshka embedding dimension configurable from 64 to 4096. It tops MMEB-v2 among open models.
- **Pick 1024 dimensions**, not 4096 — Matryoshka truncation keeps most of the quality at a quarter of the index size and query cost. Validate the drop on the gold set before committing.
- **8-bit quantization on a low-tier GPU is the target configuration.** At FP16 the 8.8B model needs roughly 19 GB before KV cache; at 8-bit the weights drop to about 9 GB, which fits a **24 GB card** with room for the vision encoder's activations. There is no reason to rent an H100 for a nightly batch job.
- **With INT8 chosen, the GPU is picked on price and VRAM alone.** The 8-bit column below is recorded only to show why the choice is safe:
<table header-row="true">
<tr>
<td>GPU</td>
<td>VRAM</td>
<td>Modal rate</td>
<td>8-bit story</td>
<td>Verdict</td>
</tr>
<tr>
<td>**L4** (Ada, sm_89)</td>
<td>24 GB</td>
<td>about \$0.80/hr</td>
<td>4th-gen tensor cores: **both FP8 W8A8 and INT8 W8A8** natively</td>
<td>**Default.** Cheapest 24 GB card; INT8 W8A8 runs natively</td>
</tr>
<tr>
<td>**A10** (Ampere, sm_86)</td>
<td>24 GB</td>
<td>about \$1.10/hr</td>
<td>INT8 W8A8 yes; FP8 only as **weight-only W8A16** via Marlin kernels</td>
<td>**Drop-in fallback.** INT8 behaves identically here — this portability is exactly why INT8 was chosen</td>
</tr>
<tr>
<td>**L40S** (Ada)</td>
<td>48 GB</td>
<td>about \$1.95/hr</td>
<td>FP8 with Transformer Engine, roughly 2x the FP8 throughput</td>
<td>Only if large batches or high-resolution renders exhaust 24 GB</td>
</tr>
<tr>
<td>**T4** (Turing)</td>
<td>16 GB</td>
<td>about \$0.59/hr</td>
<td>INT8 supported, but 16 GB is tight once the vision tower runs</td>
<td>Not worth the OOM risk</td>
</tr>
<tr>
<td>**H100 / B200**</td>
<td>80 GB+</td>
<td>\$3.95–\$6.25/hr</td>
<td>Fastest FP8, and INT8 is *unsupported* on Blackwell</td>
<td>**Rejected.** 5x the price for a job with no latency requirement</td>
</tr>
</table>
- **The format is decided: INT8 W8A8.** Not FP8, not a bake-off. INT8 runs on every NVIDIA GPU with compute capability above 7.5 — Turing, Ampere, Ada and Hopper alike — so the pipeline stops caring which card Modal happens to schedule. FP8 would lock the design to Ada or newer for no benefit on a nightly batch job that has no latency target.
- **Producing the checkpoint:** quantize once with `llm-compressor`, using roughly 512 calibration samples at 2048 sequence length, drawn from **real shop data** — actual design renders and actual query phrasings, not a generic text corpus. Calibration data that matches the workload is the single biggest lever on quantized accuracy. Version the resulting checkpoint and store it next to the model revision.
- **One caveat to record:** INT8 is *not* supported on Blackwell (compute capability 10.0 and above). That is irrelevant today and would only matter if some future decision moved to a B200 — at which point the format, not just the GPU, has to change.
- **Cap the input resolution.** Vision activation memory scales with pixels, and an uncapped 5000 px PSD render is what actually causes OOM on a 24 GB card — not the weights. The 1024 px AVIF search render is the contract with the model, and it is enforced in the sidecar.
- Bound `--max-model-len` and enable a **quantized KV cache**; the KV cache, not the weights, is the second-largest allocation and the usual cause of a failed start on a 24 GB card.
- Serve with vLLM on Modal, one GPU (`N_GPU = 1`), no tensor parallelism. One function, one model, one job.
- Embed **both** the AVIF render *and* a synthesized text sidecar (layer names + text-layer strings + customer + job number + date + folder tokens), then store both vectors. Multimodal recall and "I remember the customer name" recall are different failure modes.
**5. Index — LanceDB**
- Embedded, file-based, no server to babysit — correct for a one-shop deployment. Table lives on local disk; versioned snapshots sync to the HF dataset repo.
- **Hybrid retrieval:** vector similarity + Tantivy full-text over the extracted layer/metadata text + scalar filters (customer, date range, job number, file type, volume).
- **C++ access:** LanceDB officially ships Python, TypeScript and Rust SDKs. There are community **C FFI bindings (****`lancedb-c`****)** with a CMake build and a Doxygen'd header — use those from Oat++ if they hold up. **Fallback:** run LanceDB in the existing Python sidecar behind a local socket and keep the C++ side talking to a tiny internal contract. Decide this with a spike before committing; do not let an unmaintained binding block the feature.
**6. Rerank — deferred, deliberately**
<callout icon="⏸️">
	**No reranker in v1.** Retrieval is embedding-only: LanceDB hybrid search returns its top results and the UI shows them. The reranker is a **quality optimization on top of a working system**, and there is no way to know whether it is worth the second model, the second cold start and the extra latency until the gold set says the embedding-only ordering is the actual bottleneck.
</callout>
**What is built now, so adding it later is cheap:** retrieval already returns a **larger candidate set (K ≈ 50) than it displays (10)**, and the ranking step is a named, swappable interface with identity ordering as its default implementation. Dropping `Qwen3-VL-Reranker-2B` in later becomes a configuration change and a Modal function, not a redesign.
**When to revisit:** the gold set shows good Recall@50 but weak Recall@10 — that specific gap is the signature of a reranking problem. If Recall@50 itself is poor, the reranker cannot help and the fix is upstream in rendering, metadata or embedding.
**Planned milestone:** the reranker is scheduled for the **Modal tier, once a paying customer exists** — the same trigger that pays for the GPU. Two conditions, not one: the money must be there *and* the Recall@50-versus-Recall@10 gap must justify it. A second model on a free notebook GPU is the worst of both worlds — twice the cold start and twice the memory on the hardware least able to spare either.
**7. Serve — Oat++ gateway**
`/api/v1/assets/search` accepts a text query, an uploaded reference image, or an existing asset ID ("more like this"). Returns ranked results with AVIF thumbnail URLs, the resolved local path, the linked job/quotation, and a stale-index flag. Same auth, same RFC 9457 errors, same tenant scoping as every other route. Long-running reindex work goes through the PostgreSQL job queue, not the request thread.
**8. Present — Qt Quick desktop**
AVIF thumbnail grid with progressive load, drag-an-image-in to search, "find similar" on any result, filter chips for customer/date/type, **reveal in Explorer** and **open original**, and one-click attach of a result to a quotation or job. Shows honestly when the index is stale or a volume is offline.
#### How a search query is actually processed
This is the half of the system the pipeline diagram does not show, and it constrains the indexing design more than the indexing design constrains it.
#### First: "quantized vector" means two different things
These get confused constantly, and confusing them leads to wrong decisions:
<table header-row="true">
<tr>
<td></td>
<td>**Model quantization** (INT8, W8A16)</td>
<td>**Vector quantization** (PQ, SQ)</td>
</tr>
<tr>
<td>What is compressed</td>
<td>The model's *weights*, in GPU memory</td>
<td>The *stored vectors*, in the index on disk</td>
</tr>
<tr>
<td>Chosen because</td>
<td>The model must fit a 16 GB card</td>
<td>The index must stay fast and small</td>
</tr>
<tr>
<td>Affects</td>
<td>Embedding cost and a little accuracy</td>
<td>Search recall and query latency</td>
</tr>
<tr>
<td>Decided by</td>
<td>Which GPU we get</td>
<td>How many files the shop has</td>
</tr>
</table>
**They are independent.** An INT8 model does not emit int8 vectors — Qwen3-VL-Embedding outputs **float32 regardless of how its weights are stored**. So "we saved the quantized vector" is not quite what happens: what is saved is a normal float vector produced by a quantized model. Whether the *index* compresses those vectors is a separate decision made later, on different grounds.
#### The query path, step by step
1. **The query arrives** at `/api/v1/assets/search` — text ("blue wedding card, gold border, hotel"), an uploaded photo, or an existing asset ID for "more like this". Tenant and actor come from the session, exactly as on every other route.
2. **Full-text search runs first and returns immediately.** Tantivy over the layer names, text-layer contents, customer, job number and folder tokens already extracted in step 2 of the pipeline. **No model, no GPU, single-digit milliseconds.** This result is rendered right away.
3. **The query is embedded into the same vector space as the index.** This is the step with all the consequences — see below.
4. **Scalar filters are applied as a *prefilter*, not a postfilter.** Tenant, customer, date range, file type, volume. Prefiltering is mandatory here: a postfilter searches the whole space and then discards, which in a multi-tenant system is both slower and a correctness hazard. LanceDB's own docs note that post-filtering silently loses results and needs a higher refine factor to recover them.
5. **Vector similarity search** over the surviving rows, returning **K ≈ 50** candidates.
6. **The two ranked lists are fused.** LanceDB's hybrid search combines the vector and full-text results with **Reciprocal Rank Fusion** by default. RRF is well suited here because it merges *ranks* rather than scores, so a cosine distance and a BM25 score never have to be made commensurable.
7. **Top 10 are shown**, 50 are retained. That gap is the reranker's future home and the number the gold set watches.
#### The consequence nobody expects: the query needs the same model, live
**Vectors from two different models are not comparable.** Not "less accurate" — meaningless. A cosine distance between a SigLIP 2 vector and a Qwen3-VL vector is a number with no interpretation. Two things follow, and they are the most important operational facts on this page:
- **The embedding tier is a property of the entire index, not of a run.** Moving from Tier 1 to Tier 3 is not a config change; it is a **full reindex of every file**. Both indexes can coexist during a migration, but a mixed table is a broken table.
- **A batch-only GPU cannot serve search.** Indexing is nightly and can wait an hour. A search box must answer in about 200 ms. **You cannot start a Kaggle notebook to answer a keystroke.** So whatever model embeds the query must be *always available* — which the notebook tier structurally is not.
It still does not rescue the notebook tier — **nothing does**, because a Kaggle kernel is not addressable and takes minutes to start. But it does change *which local model* can sit on the query path, and that turns out to matter more.
#### The five-second budget, and what it actually buys
The latency tolerance is **five seconds, not 200 ms**. That is a large relaxation and it changes the answer, so the reasoning is recorded rather than the conclusion alone.
**The decisive measurement: a text query is not a generation workload.** Embedding a query is a *single prefill pass* over a few dozen tokens — there is no token-by-token decode, which is the slow, memory-bandwidth-bound part everyone's intuition is calibrated on. Published llama.cpp CPU benchmarks put prefill for an 8B model at 8-bit somewhere between **121 and 370 tokens/second** on ordinary desktop silicon. A 30-token query is therefore **well under one second**. The thing that looked impossible is not close to the limit.
<callout icon="✅">
	**Therefore: symmetric model, asymmetric compute.** Run **the same Qwen3-VL-Embedding checkpoint on the shop's own CPU for queries**, while indexing continues to run in batch on the notebook GPU. Indexing is thousands of images and belongs on a GPU; querying is one item and does not. Because both sides use identical weights, the vectors live in one space and the comparability problem disappears.
	**This directly overturns the previous conclusion on this page.** The Qwen3-VL index built on Colab or Kaggle is no longer evaluation-only — **it becomes the shipped product**, with no card, no cloud dependency and no quota on the query path. SigLIP 2 drops from "the product until revenue exists" to a **fallback tier** for machines that cannot hold the larger model. Search quality improves immediately, and Modal reverts to what it should always have been: a throughput upgrade for indexing, not a prerequisite for search.
</callout>
#### The budget is not spent on text — it is spent on images
The vision tower is where five seconds actually goes. A single image expands into on the order of a **thousand or more tokens** before the encoder runs, so a drag-in-a-photo query costs roughly **3–15 seconds** at the same prefill rates that make a text query trivial. Three consequences:
- **Cap the *query* image at 512 px**, separately from the 1024 px indexing render. Token count scales with pixels, so this is roughly a 4x cut. A query image only has to convey the gist; the indexed render is what carries the detail.
- **Text queries and image queries get different budgets in the UI.** Text returns effectively instantly; "find similar" is allowed a spinner and an honest progress state.
- **Image queries are the rarer path.** Optimize the common case first and let the rare one be slow.
#### Settled: one production profile, one documented fallback
The two sections that follow were written while the model size was genuinely undecided, and they reached the opposite conclusion from the one now in force. They are kept because the reasoning is still useful, but **this table is what the build follows.**
<table header-row="true">
<tr>
<td>Tier</td>
<td>Model</td>
<td>Format</td>
<td>Where it runs</td>
<td>Role</td>
<td>Ships in v1?</td>
</tr>
<tr>
<td>**Production semantic**</td>
<td>`Qwen3-VL-Embedding-8B`</td>
<td>float32 vectors at 4096 dim; W8A16 weights on notebook GPU, 8-bit locally</td>
<td>Notebook GPU for batch indexing; shop CPU for queries</td>
<td>The residual "no recoverable text" query</td>
<td>**Yes — committed.** Local CPU on the query side, free notebook GPU for batch indexing</td>
</tr>
<tr>
<td>**Low-memory fallback**</td>
<td>`Qwen3-VL-Embedding-2B`</td>
<td>2048 dim</td>
<td>Shop CPU</td>
<td>Machines that cannot hold the 8B resident</td>
<td>As a documented alternative profile, not the default</td>
</tr>
<tr>
<td>**Emergency fallback**</td>
<td>SigLIP 2</td>
<td>1152 dim, ONNX CPU</td>
<td>Shop CPU</td>
<td>Only where neither Qwen profile is viable</td>
<td>No</td>
</tr>
<tr>
<td>**Throughput upgrade**</td>
<td>Same 8B checkpoint</td>
<td>INT8 W8A8</td>
<td>Modal, from the first paying customer</td>
<td>Faster backfill and reindex</td>
<td>No</td>
</tr>
</table>
**Two rules that make the table safe.** A profile is a property of the whole index, so switching profiles means a **separate index and a gold-set comparison**, never a config flip — the 2B and 8B do not even emit the same vector length. And the semantic tier is the *last* thing built, so if the text path answers enough queries, none of this ships and nothing is lost.
#### The model-size decision this reopens
With a CPU on the query path, model size stops being an abstract quality question and becomes a **RAM question**. `Qwen3-VL-Embedding` ships in **2B and 8B**, and the smaller one collapses several open problems at once:
<table header-row="true">
<tr>
<td></td>
<td>**2B**</td>
<td>**8B**</td>
</tr>
<tr>
<td>Resident RAM on the shop server, 8-bit</td>
<td>\~2.5 GB</td>
<td>\~9 GB</td>
</tr>
<tr>
<td>On a free T4 for indexing</td>
<td>**Fits in FP16 outright** — no quantization at all</td>
<td>Needs W8A16 or INT8, and 16 GB is tight</td>
</tr>
<tr>
<td>Query latency on CPU</td>
<td>Comfortably inside budget, image queries included</td>
<td>Text fine; image queries at the edge</td>
</tr>
<tr>
<td>Retrieval quality</td>
<td>Lower — by how much, only the gold set knows</td>
<td>77.8 on MMEB-v2, first among open models</td>
</tr>
</table>
**Notice what 2B deletes:** the W8A16-versus-INT8 debate, the Marlin kernel dependency, the FP8-on-Turing blocker, the 16 GB OOM risk, the third-party checkpoint trust problem, and most of the RAM pressure on the shop PC. That entire chain of difficulty exists *because* the 8B model was assumed. **Every one of those problems is a consequence of a model-size choice that was never actually justified against measured retrieval quality.**
**So the gold set decides, and it must evaluate both.** The marginal cost is one extra indexing run over the same corpus, which is hours of free notebook GPU. If 2B is within a few points of 8B on real shop queries, take 2B and delete half this page's complexity. **Plan for 2B; upgrade to 8B only if the evidence demands it.**
#### Why an 8B index with a 2B query encoder cannot work
The instinct behind this is right and the mechanism is not. **The two models do not share a vector space.** `Qwen3-VL-Embedding-2B` outputs up to **2048** dimensions and `Qwen3-VL-Embedding-8B` outputs up to **4096** — so the vectors are not even the same *shape*, and the comparison fails as a type error before it fails as a quality problem. Matryoshka does not rescue this: MRL truncates dimensions **within one model**, it does not align two separately trained models. Truncating the 8B to 2048 would produce two same-length vectors pointing in unrelated directions, which is worse than an error because it returns plausible nonsense instead of crashing.
**The legitimate version of the same instinct** is to keep one model and vary *how much work you do per item*: a higher-resolution render at index time, multiple crops or a text sidecar per file, versus one cheap pass at query time. Asymmetry belongs in the effort, never in the weights.
#### But if the search is text-driven, the model question mostly dissolves
This is the more important half of the answer, and it reorders the phase. **Banners and certificates whose content is Nepali and English text are not a visual-similarity problem — they are a text-retrieval problem.** That changes which component is the product.
**Semantic embeddings are actively bad at what is being asked for.** A vector search for a specific name, date, hotel or programme title returns *files that look like certificates*, because that is what visual semantics encodes. Exact tokens — proper nouns, numbers, spellings — are precisely what an embedding blurs away and precisely what BM25 nails. **For "find the certificate that says श्री राम बहादुर", full-text search is not the fallback. It is the correct answer, and the embedding is the fallback.**
<callout icon="🎯">
	**The text already exists inside the PSD, and extracting it costs nothing.** `psd-tools` reads **text-layer contents directly** — no OCR, no model, no GPU, no quantization, no notebook. For a banner or certificate laid out in Photoshop, the Nepali and English strings are *editable text objects*, which means the highest-value search signal in the entire system is already sitting in step 2 of the pipeline as a plain string. This was always in the plan as a secondary signal. **It should be promoted to the primary one.**
</callout>
#### The catch that decides how much OCR is needed
**Print shops routinely convert text to outlines before output** — to avoid font-substitution accidents at the press. Once text is converted to paths, the string is gone and `psd-tools` cannot recover it. The same applies to flattened TIFFs, rasterized layers and scanned proofs. So the real question is not *whether* OCR is needed but *what fraction* of the archive needs it, and that is a **measurement to run in week one**: sample 200 real files and count how many still carry live text layers.
- **High live-text share** → ship text extraction plus FTS and the feature is largely done, on CPU, for free.
- **Low live-text share** → OCR becomes the core of Phase 5, and it should be budgeted as such.
#### Nepali OCR, honestly assessed
Devanagari OCR is harder than Latin, and **decorative banner typography is the worst case** — heavy display faces, outlines, gradients, text over imagery, curved baselines. Certificates are the easier case: cleaner fonts, higher contrast, predictable layout. Expect the two categories to behave very differently and measure them separately.
- **Surya** is the strongest open option for Indic scripts and is built for exactly this class of work; a published low-resource comparison reports it best-in-class on Sinhala. Roughly 2 seconds per page on a modest GPU — irrelevant for a nightly batch.
- **Tesseract** with `nep` traineddata is CPU-only, extremely fast and trivially parallel, but its accuracy on stylized Devanagari is the weakest of the realistic options.
- **Practical shape:** run OCR **once at index time** as part of the nightly batch, never at query time. Store the extracted text as a normal field. OCR is a batch concern, which means it can live on the notebook GPU without ever touching the search path.
#### Devanagari full-text search has a specific gotcha
LanceDB's full-text index is Tantivy, and **Tantivy ships stemming for 17 Latin languages with third-party tokenizers for Chinese, Japanese and Korean — there is no Indic stemmer.** Devanagari does use spaces, so default tokenization mostly finds word boundaries correctly, but inflected Nepali forms will not match each other. Three mitigations, all cheap:
- **Normalize to Unicode NFC on both index and query.** Devanagari has multiple valid encodings of the same visual string, and a mismatch here causes silent zero-result searches that look like missing data.
- **Add an n-gram indexed field** alongside the tokenized one. It costs index size and buys substring and inflection tolerance without a language-specific stemmer.
- **Index a romanized transliteration too.** Staff frequently type Nepali names in Latin script on a Latin keyboard; without this, "Ram Bahadur" never finds राम बहादुर.
#### What this means for the model decision
<callout icon="✅">
	**If retrieval is text-driven, the embedding tier stops being the critical path, and the 8B-versus-2B question drops from architectural to marginal.** The order of value becomes: **text-layer extraction → OCR for outlined files → FTS with Devanagari handling** — all CPU, all free, all offline — and only then a semantic vector index for the residual "I cannot remember any of the words, but it was blue with a gold border" query.
	So: **2B, if any.** It comfortably serves both indexing and querying, it removes every quantization and VRAM constraint on this page, and its lower retrieval ceiling matters far less once it is the secondary signal rather than the primary one. **Build the text path first and measure what it fails to find.** That residual set is the only honest justification for an 8B, and it may turn out to be small enough that no visual embedding ships in v1 at all.
</callout>
#### When the organization's name is a logo, not text
This is the real gap in a text-first design, and it deserves a different mechanism rather than a bigger model. **A logo is not an open-ended visual query — it is a small, closed set of recurring marks.** A print shop deals with perhaps fifty to five hundred organizations over its whole history, each with a mark that is reproduced *exactly* rather than approximately. That is an **entity resolution** problem, and it is far more tractable than either OCR or semantic search.
#### Answer the cheap questions first, because two of them are already built
- **The job record usually already knows.** The asset catalog in Phase 3 has typed relations from files to quotations, orders and parties. "Every file for Hotel Yak" is therefore a **join, not a search** — exact, instant, free, and already specified. Whenever a file was produced through the system, the customer identity is a *fact* rather than an inference. This should be the first thing the search box tries, and it removes the logo problem entirely for everything created after go-live.
- **The folder path is a signal.** Print shops organize by customer far more often than they admit. Folder tokens are already in the FTS index.
- **The logo's own filename is usually recoverable text.** Logos are typically *placed*, not drawn in situ, and `psd-tools` exposes placed art through a `SmartObject` API covering both embedded and externally linked files. A layer named `yak_logo` or a linked file called `HotelYak-logo-final.ai` is **plain text, extractable on CPU, with no OCR and no model.** This is the same free-signal principle as text layers and it should be harvested in the same pass.
#### For the rest: a logo gallery, matched once at index time
The residual case is a flattened or inherited file with no relation, no useful folder and no live text. The design for it:
1. **Keep a small gallery of reference marks** — one to five crops per organization, linked to the party record that already exists in the domain model.
2. **At index time, match each render against the gallery.** Batch work, on the nightly pass, never at query time.
3. **On a confident match, write the organization's name into the file's text field** — the same field the FTS index already searches.
<callout icon="🔑">
	**The trick is that this converts a visual problem into a text problem at index time.** Once the mark is resolved to a name, searching for "Hotel Yak" is an ordinary full-text query. **The query path stays pure FTS — fast, exact, CPU-only, offline, and identical for logo-derived and OCR-derived and layer-derived text.** No new code on the search path, no second latency budget, no model on the query side. The hard work happens once per file, at night, and everything downstream is unchanged.
</callout>
#### Keypoint matching, not embeddings, for this specific job
The intuitive move is to embed the logo and do a vector search. **That is the wrong tool here, and the failure is documented rather than theoretical.** CLIP-family image encoders produce one global embedding per image and are measurably **biased toward large objects** — a CVPR 2025 analysis found exactly this in the image encoder, and the object-centric retrieval literature exists because global embeddings miss small objects in cluttered scenes. A logo occupying 3% of a banner is precisely that failure case. Tiling the render into patches mitigates it, but multiplies embedding cost per file by the tile count.
**Classical instance matching is a better fit, and it is nearly free.** A logo inside a design is not a *similar* logo — it is **the identical artwork**, placed and scaled. That is instance retrieval, which is the classic strength of local-feature matching: ORB or SIFT keypoints, descriptor matching, then **RANSAC geometric verification** to confirm the matches form a consistent transform rather than coincidental texture agreement. Published work on hybrid ORB-plus-template-matching logo detection targets CPU-only deployment specifically.
- **Runs on CPU in OpenCV.** No model, no training data, no GPU, no quantization, no notebook, no licence question.
- **Scale and rotation tolerant**, which plain `matchTemplate` is not — template matching handles translation and scale but degrades badly under rotation and non-affine transforms.
- **Geometric verification is what makes it precise.** The RANSAC inlier count is a directly interpretable confidence score, which is what makes a threshold defensible.
- **Perceptual hashing does not work here.** pHash and its relatives are whole-image fingerprints, and published evaluations show they fail on borders, crops and mirroring. A logo is a small crop of a large image — the exact case pHash is worst at.
#### Do not build the gallery by hand — let the archive build it
Asking someone to crop five hundred logos is a project that never finishes. Invert it: **cluster recurring marks across the archive automatically, then ask a human to name each cluster once.** The system says *"this mark appears in 340 files — whose is it?"*, a staff member picks the existing party record, and 340 files are tagged retroactively in one action. A few hours of clicking covers a decade of files, the work is naturally ordered by value (the most frequent marks first), and it is done by the people who actually recognize the marks.
This is also the right shape for the product philosophy already on this page: the system **suggests**, a human **confirms**. It is level 2 behaviour, not level 4.
#### Precision matters more than recall here
A missed logo tag means a file is harder to find. **A wrong logo tag files one customer's design under another customer's name** — which is a confidentiality problem, not an inconvenience, and in a white-labeled multi-tenant system it is the worst failure the product can have. So:
- **Threshold high and accept misses.** An untagged file still reaches the other search paths.
- **Auto-tags are suggested until confirmed**, visibly distinct in the UI, and a single rejection removes the tag and feeds the threshold.
- **Never let an inferred tag cross a tenant boundary or drive an automatic attachment** to a quotation or job. Inference informs search; it never mutates a business record.
The matcher lives behind its own interface — an entity-tagging step in the sidecar, parallel to the renderer and the embedder — so it can be replaced by a learned detector later without touching the index, the API or the client.
#### The date half of the question is much easier
"Files from a particular day" is not a hard problem, because dates have **redundant, independent sources**: the job or quotation date in the domain record, the file's own timestamps, EXIF/XMP creation and modification dates, and any date written in a text layer. At least one is almost always present, and all of them are scalar fields rather than search signals — they become prefilters on the query, in exactly the position already specified in the query path. **Store all of them separately rather than collapsing them into one "date"**, because "when the job was sold" and "when the file was last touched" are different questions and staff will ask both.
#### Tracing one real query end to end
The test case, in the shop's own words: **"the .psd of International Women's Day, where Hands Nepal was a supporter."** Today that costs about **two hours** of opening folders. It is worth tracing precisely, because it corrects the previous section and it shows what the system actually has to do.
#### First, a correction: the relational path does not help here
The previous section leaned on "the job record already knows." **For this archive that is wrong.** The files are saved at random — no folder convention, no naming convention, no link to a quotation, most of them predating the system entirely. So for the existing archive: **filename is dead, folder is dead, and the relation does not exist.** The join only ever helps files created *through* SquiFlow after go-live. **Everything that makes the two-hour search go away has to come out of the file's own contents.** That is the honest starting position, and it raises the value of content indexing from "useful" to "the entire feature."
#### The query is not one question, it is three
The sentence decomposes into three independent facets, and **not one of them is sufficient alone** — which is exactly why the manual search takes two hours:
<table header-row="true">
<tr>
<td>Facet</td>
<td>What it is</td>
<td>Where the answer lives in the file</td>
</tr>
<tr>
<td>**Event** — International Women's Day</td>
<td>The subject of the design</td>
<td>The headline text, in Nepali (अन्तर्राष्ट्रिय महिला दिवस) or English — plus a **date window**, because it is 8 March every year</td>
</tr>
<tr>
<td>**Organization** — Hands Nepal</td>
<td>A party</td>
<td>A logo, possibly also a text layer, possibly a smart-object filename</td>
</tr>
<tr>
<td>**Role** — supporter</td>
<td>*Where* the organization appears</td>
<td>Position and grouping: the sponsor strip, not the masthead</td>
</tr>
</table>
#### The word "supporter" is the most informative part of the query
It is easy to read past, but it changes the mechanism. **A supporter is not the organizer.** Their mark sits in the small logo row along the bottom, one of five or ten, at maybe 2% of the canvas area. Three consequences follow directly:
- **Whole-image embedding is definitively out for this.** A vector summarizing an entire banner cannot encode a mark occupying 2% of it. This is no longer an argument — it is the specific failure this query would hit.
- **Logo matching must run on a full-resolution tile, not the 1024 px search render.** Downsampled to 1024 px, a bottom-strip logo is a few dozen pixels tall and has no keypoints left to match. **Detection resolution and embedding resolution are different settings** and must be configured separately.
- **"Supporter" is itself a searchable structural fact**, not just context — see below.
#### The layer tree answers the structural question for free
`psd-tools` exposes `descendants()` over the full nested layer tree, and every layer carries `name`, `kind` (`type`, `pixel`, `group`, `shape`, `smartobject`), `visible`, and a **`bbox`**** of (left, top, right, bottom)**. That is far more than a bag of strings — **it is a layout map, extracted on CPU in milliseconds, with no OCR and no model.**
What that yields for this query:
- **Group names are labels designers already wrote.** Real PSDs contain groups called `sponsors`, `supporters`, `logos`, `footer`, `सहयोगी`. When one exists, "Hands Nepal as supporter" is answerable from the layer tree alone — no vision at all.
- **Position is a proxy when names are absent.** A cluster of small elements in the bottom band of the canvas *is* a sponsor strip. That is a geometric rule over `bbox` values, and it costs nothing.
- **The role is stored as structure, not guessed at query time.** Index each recognized organization with **where** it appeared — masthead, body, or sponsor strip — so "as supporter" becomes a filter rather than a hope.
- **Text layers carry their own position too**, which is how the system distinguishes the headline ("International Women's Day") from fine print.
#### The retrieval is an intersection, not a ranking
This is the core point, and it is why the answer does not depend on any model being excellent:
<table header-row="true">
<tr>
<td>Filter applied</td>
<td>Signal</td>
<td>Archive remaining (illustrative; measured corpus is approximately 6,000 files)</td>
</tr>
<tr>
<td>File type is PSD/PSB</td>
<td>Metadata</td>
<td>At most 6,000; exact PSD/PSB count to be measured</td>
</tr>
<tr>
<td>Created or modified in a **Feb–Mar window, any year**</td>
<td>Timestamps, EXIF/XMP</td>
<td>Hundreds; derive from the actual timestamp distribution</td>
</tr>
<tr>
<td>Text contains the event, in **either script**</td>
<td>Text layers, OCR, FTS</td>
<td>\~40</td>
</tr>
<tr>
<td>Organization token or logo match: **Hands Nepal**</td>
<td>Text, smart-object name, logo gallery</td>
<td>\~6</td>
</tr>
<tr>
<td>That organization sits in the **sponsor strip**</td>
<td>Layer group name or bbox position</td>
<td>\~2</td>
</tr>
</table>
<callout icon="⏱️">
	**Each of those signals is individually weak and unreliable. Their conjunction is not.** The date window alone throws away 97% of the archive using nothing but a file timestamp, and it costs zero to compute. **The goal is not to rank the right file first — it is to reduce roughly six thousand files to a dozen thumbnails in one second, which a human recognizes instantly.** Two hours becomes ten seconds even if every individual component is mediocre. That is why this design does not need an excellent embedding model, and it is the strongest argument yet that the 8B is unnecessary.
</callout>
#### What the query box has to do with the sentence
The user types one natural sentence, so the API needs a small **facet parser** in front of retrieval — and it should be rule-based, not a language model:
1. **Match known organization names** against the existing party table, including aliases and romanized spellings. This list already exists in the domain model.
2. **Match role keywords** — supporter, sponsor, organizer, partner, सहयोगी, आयोजक — to a position filter.
3. **Match a small recurring-event lexicon** — Women's Day, Teej, Dashain, Constitution Day, school anniversaries — each mapping to Nepali and English surface forms **and to a date window**. A print shop's calendar repeats every year; this list is perhaps 50 entries, written once, and it is the single highest-leverage lookup table in the product.
4. **Everything unmatched stays free text** for FTS and, if needed, the vector path.
Crucially, **the parse is a suggestion the user can see and edit.** Show the facets as removable chips — `PSD` · `Feb–Mar` · `Women's Day` · `Hands Nepal` · `supporter`. When the result set is empty, the user drops a chip instead of retyping a sentence and guessing what went wrong. This is also the same "suggest, human confirms" pattern used everywhere else on this page.
#### Recurring events compound the value
International Women's Day happens annually, so the shop has probably made a dozen of these. Once **one** is found, the rest follow almost for free: same event tag, same sponsor set, "find similar." And the same is true of the organization — finding one Hands Nepal file with a confirmed logo match seeds the gallery entry that then tags every other Hands Nepal file in the archive retroactively. **The first successful search is the expensive one; it makes the next hundred cheap.**
#### The honest failure case, and what it still gives you
Suppose the file is fully flattened, the text was outlined, OCR misreads the decorative Devanagari headline, and Hands Nepal is not yet in the logo gallery. Then the facet chain collapses to *PSD, Feb–Mar, any year* — about 1,200 files. **That is still a thumbnail grid that can be scanned in a couple of minutes rather than a two-hour folder crawl**, and scanning it produces the gallery entry that prevents the next failure. **The system degrades to "much faster than today" rather than to "no answer,"** which is the property that matters for adoption.
#### One consequence for the phase plan
This archive needs a **one-time bulk backfill** — render, extract, OCR and index approximately 6,000 existing files — and that is still a much larger compute event than the nightly delta the schedule was designed around. It is also the job that must finish before the feature is worth anything at all, since the two-hour problem lives entirely in the legacy archive. **Budget the backfill explicitly**: it is resumable, content-hash idempotent, runs for days rather than hours, and its text-extraction and OCR stages run on the shop's own CPU for free.
#### The desktop search experience: Explorer shortcut to a scannable result grid
The product surface should be the mock-up described here, not a web search page. **Explorer remains where files live; SquiFlow becomes the fast way to locate them.**
```plain text
[Windows File Explorer]
        │ global shortcut / selected file
        ▼
[C++ Raycast-like overlay]
        │ text, pasted image, or "find similar"
        ▼
[Oat++ /api/v1/assets/search]
        │ tenant filters + FTS + exact vector scan
        ▼
[LanceDB matches]
        │ asset IDs + scores + facets + AVIF URLs
        ▼
[Qt Quick result grid]
        │ Open / Reveal / Filter / Attach
        ▼
[Windows File Explorer selects the original]
```
**The overlay is intentionally thin.** It captures the query, shows the parsed chips and the first few results, then launches the full Qt window only when the user wants the thumbnail grid or advanced filters. It does not load a model, read LanceDB, parse PSDs or perform OCR. Those stay behind the backend boundary.
**Windows integration uses established platform APIs rather than a custom Explorer replacement:**
- `RegisterHotKey` opens the overlay from Explorer or any application.
- `IShellWindows` + `IFolderView2` read the currently selected Explorer item for **Find similar**.
- `SHOpenFolderAndSelectItems` reveals and selects the winning PSD reliably; it is preferable to constructing an `explorer.exe /select` command line.
- A signed shell context-menu extension is optional later. The global shortcut ships first because it is simpler, safer and does not inject code into Explorer.
#### The 6,000-file scale changes the storage choice, not the accuracy target
The measured corpus is **approximately 6,000 design files**, not forty thousand. That is small enough to remove several optimization compromises:
- Keep **Qwen3-VL-Embedding-8B as the high-accuracy semantic model** unless the gold set disproves it. The 2B remains a degraded-hardware fallback, not the planned quality tier.
- Use the model's full **4096-dimensional float32 vector** initially. One vector for 6,000 files is about **94 MiB**; even separate image and synthesized-text vectors are about **188 MiB** before metadata. That is trivial on the shop machine and avoids spending recall to save space that is not scarce.
- Use **flat, exact cosine search**. Six thousand vectors do not justify IVF-PQ, HNSW tuning or vector quantization. Exact search gives the highest possible retrieval accuracy and remains comfortably fast.
- Keep the 1024 px indexing render and full-resolution logo tiles. Image resolution affects small-logo recall far more than shaving a few milliseconds from a 6,000-row scan.
- The backfill is now a bounded job measured in thousands, not tens of thousands. It still remains resumable and low-priority, but it can be completed and re-run during evaluation without designing a distributed pipeline.
<callout icon="🎯">
	**High accuracy does not mean model-only search.** The 8B score is one signal beside exact text, OCR, date, event, organization and sponsor-position facets. For the benchmark query — *"the PSD of International Women's Day where Hands Nepal was a supporter"* — filters should reduce the corpus first; the 8B model ranks the small residual set and handles vague visual queries. This is more accurate than asking the model to solve every part of the sentence alone.
</callout>
#### Search record: stable identity first, paths as projections
The proposed four fields are a sound start, but an absolute Windows path cannot be the primary identity: files are renamed, moved, restored and opened from different PCs. Store a stable asset record and treat the current path as a mutable projection.
<table header-row="true">
<tr>
<td>Field</td>
<td>Type</td>
<td>Purpose</td>
</tr>
<tr>
<td>`asset_id`</td>
<td>UUID</td>
<td>Stable SquiFlow identity returned by search and used by the UI</td>
</tr>
<tr>
<td>`tenant_id`</td>
<td>UUID</td>
<td>Mandatory partition and prefilter; never inferred from a path</td>
</tr>
<tr>
<td>`content_sha256`</td>
<td>32-byte hash</td>
<td>Idempotency, duplicate detection and reindex decisions</td>
</tr>
<tr>
<td>`volume_id`  • `ntfs_file_id`</td>
<td>String / 128-bit ID</td>
<td>Tracks the same local file across rename or move</td>
</tr>
<tr>
<td>`source_path`</td>
<td>String</td>
<td>Current absolute path, for example `C:\\Projects\\Design\\Banner.psd`; updated by the file agent</td>
</tr>
<tr>
<td>`preview_key`</td>
<td>String</td>
<td>Stable derivative key such as `tenant/ab/cd/<hash>-256.avif`, not a machine-specific cache path</td>
</tr>
<tr>
<td>`local_preview_path`</td>
<td>String, optional</td>
<td>Disposable local cache projection; may be deleted and rebuilt at any time</td>
</tr>
<tr>
<td>`image_vector`</td>
<td>`float32[4096]`</td>
<td>8B image embedding used for semantic retrieval</td>
</tr>
<tr>
<td>`text_vector`</td>
<td>`float32[4096]`, optional</td>
<td>Same 8B model over synthesized metadata text; never mixed with another model</td>
</tr>
<tr>
<td>`model_id`, `revision`, `profile`</td>
<td>Strings</td>
<td>Pins model, commit, quantization/runtime and output dimension so incompatible rows cannot mix</td>
</tr>
<tr>
<td>`search_text`</td>
<td>String</td>
<td>NFC-normalized layer text, OCR, filenames, aliases and transliteration for ICU/ngram FTS</td>
</tr>
<tr>
<td>`file_type`, `event_ids`, `organization_ids`, `roles`, `dates`</td>
<td>Scalar/list fields</td>
<td>Cheap prefilters and visible query chips</td>
</tr>
<tr>
<td>`preview_state`, `ocr_state`, `embedding_state`, `indexed_at`</td>
<td>Enums/timestamp</td>
<td>Lets the UI distinguish ready, pending, stale and failed records</td>
</tr>
</table>
**Do not store AVIF bytes in the vector row.** LanceDB stores the stable preview key and searchable metadata; the AVIF remains in the local derivative store/cache. The backend resolves that key to a short-lived thumbnail URL. The result DTO returns `asset_id`, display path, preview URL, matched facets, score breakdown and actions — not the raw 4096-float vector.
#### Search request and response boundary
The overlay and full Qt application use the same endpoint and DTO:
```json
POST /api/v1/assets/search
{
  "query": "women's day Hands Nepal supporter",
  "selected_asset_id": null,
  "filters": { "file_types": ["psd", "psb"] },
  "limit": 50
}
```
The backend returns the parsed, removable chips plus results. Each result includes the stable asset ID, AVIF thumbnail URL, current display path, file availability, exact-text/logo/facet evidence and semantic score. The UI should explain **why** an item matched rather than exposing one opaque combined number.
When the user chooses **Open** or **Reveal**, the backend first resolves the asset ID to the latest path. The C++ client then opens that local path or calls `SHOpenFolderAndSelectItems`. If the volume is offline or the file moved after indexing, the UI reports that state and asks the file agent to reconcile; it never launches a stale path blindly.
#### Building this mostly out of parts that already exist
The last several sections described a fairly ambitious feature. It is worth counting how much of it is actually new work, because the answer is: very little. Almost every stage in the pipeline is a solved problem with a mature, permissively licensed implementation. The novel part of SquiFlow is not the parsing, the rendering, the OCR, the matching, or the indexing — it is the *glue*, the *domain vocabulary*, and the *interface*. Those are the only places where writing code is justified.
A useful discipline: for each pipeline stage, name the project that already does it. If no name comes to mind, that is a signal to search harder before writing anything.
<table header-row="true">
<tr>
<td>Stage</td>
<td>Established project</td>
<td>Licence</td>
<td>Runs on</td>
<td>What we write</td>
</tr>
<tr>
<td>PSD parsing, layer tree, bboxes, text layers, smart-object names</td>
<td>`psd-tools`</td>
<td>MIT</td>
<td>shop CPU</td>
<td>a thin extractor that walks `descendants()`</td>
</tr>
<tr>
<td>Composite a layered file to a raster</td>
<td>ImageMagick (the most PSD-capable of the raster tools)</td>
<td>Apache-2.0-style</td>
<td>shop CPU</td>
<td>invocation, sandboxing, timeouts</td>
</tr>
<tr>
<td>Resize, colour convert, thumbnail</td>
<td>libvips</td>
<td>LGPL-2.1</td>
<td>shop CPU</td>
<td>nothing</td>
</tr>
<tr>
<td>AVIF encode</td>
<td>libavif / `avifenc`</td>
<td>BSD-2</td>
<td>shop CPU</td>
<td>one quality and speed profile</td>
</tr>
<tr>
<td>OCR</td>
<td>PaddleOCR (PP-OCRv5) primary; Tesseract `nep` fallback</td>
<td>Apache-2.0</td>
<td>shop CPU, notebook GPU when available</td>
<td>language routing and confidence thresholds</td>
</tr>
<tr>
<td>Logo and instance matching</td>
<td>OpenCV feature2d, MAGSAC++</td>
<td>Apache-2.0</td>
<td>shop CPU</td>
<td>the gallery and the accept threshold</td>
</tr>
<tr>
<td>Full-text index</td>
<td>LanceDB FTS, built on Tantivy</td>
<td>Apache-2.0</td>
<td>shop CPU</td>
<td>schema, normalizer, tokenizer choice</td>
</tr>
<tr>
<td>Vector store and hybrid fusion</td>
<td>LanceDB</td>
<td>Apache-2.0</td>
<td>shop CPU</td>
<td>nothing</td>
</tr>
<tr>
<td>Embeddings, if any ship</td>
<td>Qwen3-VL-Embedding, or SigLIP 2 on CPU</td>
<td>Apache-2.0</td>
<td>notebook GPU or shop CPU</td>
<td>nothing</td>
</tr>
<tr>
<td>Filesystem watching</td>
<td>Qt `QFileSystemWatcher`, or `efsw` for recursive watches</td>
<td>LGPL / MIT</td>
<td>client</td>
<td>debounce and stability detection</td>
</tr>
<tr>
<td>Job queue and retries</td>
<td>PostgreSQL `SELECT ... FOR UPDATE SKIP LOCKED`</td>
<td>—</td>
<td>shop</td>
<td>roughly two hundred lines</td>
</tr>
<tr>
<td>Backup</td>
<td>restic + rclone</td>
<td>BSD-2 / MIT</td>
<td>shop</td>
<td>a cron unit</td>
</tr>
</table>
Note what is *absent* from that table: no message broker, no Airflow, no Celery, no Kubernetes, no separate search server. A print shop with tens of thousands of files does not need any of them, and each one added would be a service to run, monitor, upgrade, and explain to whoever inherits this.
#### The single trick that makes rendering cheap
Compositing a layered PSD is genuinely expensive — it means decoding and blending every layer. Doing that approximately 6,000 times is still the difference between a backfill that finishes overnight and one that occupies the machine for several days.
Most production PSDs do not require it. Photoshop's *Maximize Compatibility* setting, which is on by default and which most shops never turn off, embeds a pre-flattened composite of the whole document inside the file. Reading it is a single image decode with no blending at all. The rule follows directly: **try the embedded composite first, fall back to full compositing only when it is missing.** Record which path was taken, because the fallback rate on the real archive determines the backfill's actual duration and is currently unknown.
The second cost is the AVIF encode itself. `avifenc` exposes a speed dial from 0 to 10 with a default of 6, and at that default a large canvas takes seconds, not milliseconds. For a backfill of approximately 6,000 files, encoding may still be the dominant CPU cost — larger than OCR and potentially larger than embedding — so it must be measured rather than assumed cheap. The backfill should use a faster speed setting than the archival path, and the two should be measured against each other before the number is fixed.
#### Not overloading the machine it runs on
This runs on a desktop in a working print shop. The design constraint is not throughput; it is that **nobody should ever notice it running.** Several small mechanisms together achieve that, and none of them is clever.
- **One worker, not a pool.** A single indexing worker at low CPU and I/O priority, with a hard memory ceiling set in the container unit. Concurrency here buys nothing — the queue drains overnight either way — and costs responsiveness on the machine a designer is actively using.
- **Separately queued stages.** Parse, composite, encode, OCR, match, and index are distinct queue entries, not one long function. A file that fails at OCR does not re-composite. A crash resumes at the stage boundary. Each stage gets its own retry count and its own timeout.
- **Content hash as the primary key.** Every stage is keyed by the hash of the source bytes. Re-running is a no-op, duplicate files collapse to one unit of work, and the backfill can be stopped and restarted at any moment without bookkeeping.
- **Bounded queues with real backpressure.** When the queue is deep, the client is told so and stops pushing. Silent unbounded growth is how a background indexer turns into a disk-full incident.
- **A working-hours throttle.** Pause or heavily rate-limit between the shop's opening and closing hours; run freely at night. This is a one-line policy that eliminates most of the perceived-slowness risk.
- **Never on the request path.** Search reads the index. It never triggers work. A cold or partial index returns fewer results, never a slow response.
#### Accuracy comes from stacking agreeing signals, not from a better model
The instinct when accuracy is unsatisfying is to reach for a larger model. For this corpus that instinct is wrong, and the reason is the same one that produced the intersection funnel above: several independently weak signals, required to agree, beat one strong signal.
Concretely, the same organization may be evidenced by a live text layer, a smart-object filename, an OCR result, and a logo match. Those are four independent extractors with uncorrelated failure modes. Where two or more agree, confidence is high enough to index silently. Where exactly one fires, index it as a suggestion. Where they contradict, surface it for a human — that contradiction is a better use of a person's attention than reviewing a random sample would be.
This also makes the accuracy problem *measurable* rather than a matter of taste. Each extractor can be scored independently against the gold set, and the weakest one identified and replaced without touching the rest. That is only possible because each one sits behind its own interface.
A few specific accuracy choices worth pinning down:
- **MAGSAC++ rather than plain RANSAC** for the logo homography. OpenCV ships several robust estimators now, and the newer ones are measurably better at the same iteration budget. The inlier count remains the score, and the accept threshold is set by grid search on the gold set rather than guessed.
- **The ICU base tokenizer for the full-text index.** LanceDB's FTS exposes `simple`, `whitespace`, `raw`, `ngram`, `icu`, and language-specific segmenters. ICU applies Unicode word-boundary rules, which is a substantially better fit for Devanagari than whitespace splitting; the n-gram tokenizer remains the fallback for substring and partial-name matching. This should be A/B tested on the gold set — it is a configuration flag, so testing it costs almost nothing.
- **Two OCR engines, chosen by measurement, not by reputation.** Published 2025–2026 benchmarks disagree sharply: one ranks PaddleOCR most robust as documents get harder while placing Surya last on every set; Surya's own published figures claim 83.3% on olmOCR-bench and 87.2% across 91 languages at 650M parameters. Neither evaluation includes Nepali banner typography, so neither is evidence about this corpus. Run both over the same two hundred sampled files and let the numbers decide.
#### The backfill is where all of this is actually tested
The nightly delta is a handful of files. The one-time backfill is approximately 6,000 files, and it is the event that will expose every weakness above: the composite fallback rate, the AVIF encode cost, the OCR throughput, the memory ceiling, the resume logic. It should therefore be built *first*, run against a few hundred files, and instrumented before it is pointed at the archive.
It should also report honestly. A progress figure, a per-stage counter, a failure list, and an estimate of remaining time. A multi-day job with no visible progress will be killed by whoever is watching it, and then restarted from an unclear position.
#### The Colab CLI plus a static tunnel: what works, and what it actually costs
The proposal is to drive Colab from its CLI, start a GPU runtime with internet, serve the embedding model, and expose it through a static ngrok domain so the backend can call it like any inference provider. **Mechanically this works.** A GPU-backed vLLM or Transformers server behind a stable HTTPS hostname is a clean interface, and a fixed domain removes the usual "the tunnel URL changed again" problem. The honest assessment is that the *interface* is right and the *host* is not.
- **A stable hostname does not create a stable service.** Colab runtimes are interactive, time-limited and reclaimable. The tunnel makes the address permanent; it does not make the process permanent. A stable address that intermittently returns connection errors is worse than an obviously absent one, because callers stop treating failures as expected.
- **This is precisely the pattern Colab's terms restrict.** A CLI-triggered runtime serving a long-lived endpoint for another system is headless, non-interactive use — and unlike a notebook someone is watching, it is trivially recognizable as such. The concern is not a penalty; it is losing the runtime mid-backfill with no appeal and no notice.
- **A tunnel is inbound exposure.** The whole deployment topology on this page is "private by default over Tailscale, public only per reviewed endpoint." A permanent public hostname pointing at an unauthenticated inference server is the opposite of that. If it is used at all: bearer-token auth on every route, a request-size cap, a rate limit, and an allowlist. Never an open `/embed`.
- **Reversing the direction removes most of this.** The page already specifies a pull-based worker: the notebook wakes, claims a batch from the backend's narrow ingest endpoint, embeds, posts vectors back, exits. That needs **no inbound tunnel at all**, survives the runtime dying (the batch simply stays unclaimed), and is the same contract Modal will implement later. **The tunnel only becomes necessary if the notebook is on the query path — and it must never be.**
<callout icon="🔌">
	**Position: keep the inference-provider interface, reject the notebook as its host.** Define one `IEmbeddingProvider` HTTP contract — `POST /embed` with images or text, returning vectors plus the model and revision that produced them. Implement it for local CPU (always available), for the pull-based notebook worker (batch only), and later for Modal. A Colab-plus-ngrok deployment is acceptable as a **short-lived development fixture** for testing that contract. It may not be the production indexing path, and it may never serve queries.
</callout>
#### Input sanitization: a required stage, not defensive programming
Everything proposed here is correct and should be a named, testable pipeline stage rather than scattered try/except blocks. **Nothing reaches an embedding model that has not passed it**, and the same code runs for indexing and for queries so that both sides are normalized identically.
**Visual sanitization**
- **Decode defensively and quarantine failures.** Truncated downloads, zero-byte files and partial writes from a designer saving over the network are all normal in a shop, not exotic. A file that fails to decode is marked `render_failed` with its reason and skipped — it never aborts a batch. Set Pillow's decompression-bomb guard explicitly rather than relying on the default, and give every decode a wall-clock timeout.
- **Clamp total pixels, not just the long edge.** Qwen3-VL uses dynamic resolution, so token count tracks pixel area: a wide banner and a tall poster with the same long edge can differ several-fold in cost. The rule is a **total-pixel ceiling** enforced before the model ever sees the image. This page already caps the indexing render at 1024 px and query images at 512 px, which sits far below any megapixel bound — the ceiling is the backstop for anything that bypasses the normal render path, and it is what prevents a single blueprint scan from OOM-ing a 16 GB card.
- **Normalize colour and alpha at the same point.** CMYK is routine in print work, and a CMYK or 16-bit-per-channel buffer handed to an RGB-expecting pipeline fails inconsistently. Convert to 8-bit sRGB, flatten alpha onto a known background, strip embedded ICC surprises — once, in the sanitizer.
- **Reject the degenerate cases explicitly**: zero-dimension images, extreme aspect ratios, single-colour renders. A blank composite usually means the render step failed silently, and embedding it pollutes the index with a vector that matches everything mediocre.
**Textual sanitization**
- **The instruction prefix is a correctness requirement, not a tuning knob.** Qwen3-VL-Embedding is instruction-aware, so queries and documents must be prefixed distinctly and *identically across index and query time*. Getting this wrong does not raise an error — it silently degrades every search. **The prefix strings belong in the pinned embedding profile alongside the model revision and dimension**, and changing one invalidates the index exactly like changing the model.
- **Normalize before truncating, and truncate on token count.** Order matters: NFC-normalize Devanagari, collapse repeated whitespace, strip control and non-printable characters, drop zero-width joiners that layout tools scatter through text — then measure length with the real tokenizer and cut at a clean boundary. Character-count truncation is a guess, and mid-token clipping is how a query quietly stops matching.
- **Cap length far below the context window.** The synthesized text sidecar is layer names, extracted strings, filenames and folder tokens; a pathological PSD with thousands of layers can produce enormous input. A tight application-side cap (a few thousand tokens, not 32k) keeps cost bounded and loses nothing — the useful signal is at the top.
- **Deduplicate before embedding.** Repeated layer names and boilerplate footers appear hundreds of times per file and skew a pooled vector toward the boilerplate. Collapse duplicates in the sidecar builder.
**Structural and security sanitization**
- **The indexer must never fetch a URL supplied by input.** Images come from the local file agent and the derivative store by content hash — never from a link inside a document or a request. This removes server-side request forgery as a category rather than filtering for it.
- **If a remote reference is ever accepted**, it goes through an explicit fetcher with an HTTPS-only scheme allowlist, DNS resolution checked against private and link-local ranges, no redirect following to a new host, a hard byte cap, and a short timeout. Validate after resolution, not on the string.
- **Base64 payloads are size-capped before decoding**, decoded to a bounded buffer, and identified by magic bytes rather than by a declared content type or filename extension.
- **Every uploaded query image is treated as hostile input**: size limit, decode timeout, pixel ceiling, re-encode to a clean intermediate before it reaches the model. Re-encoding discards embedded metadata and malformed structure for free.
**Where this runs — and it is not Lightning AI.** The suggestion is to put preprocessing on a free Lightning CPU. **This page has already rejected a third-party CPU orchestrator twice, and the reasoning has not changed:** the shop server is always on, already holds the originals, already runs the render and extraction stages, and already owns the pending queue. Sanitization is the cheap part of the pipeline and it sits directly beside work that must happen locally anyway. Moving it off-machine adds an account, a network hop, a data-egress boundary for customer designs, and a second place to debug — in exchange for CPU cycles that are not scarce. **Sanitization runs in the sidecar, on the shop server, as the stage immediately before dispatch.**
One consequence worth stating: **the remote worker re-validates anyway.** It re-checks dimensions, byte counts and payload shape on arrival, because a worker that trusts its input is one malformed manifest away from crashing mid-batch. Sanitize at the source, validate at the boundary.
#### The relational store: PostgreSQL, and why the question is closed
PostgreSQL is not a provisional choice here — the schema, the migrations and the security model are already built on features MariaDB does not have equivalents for. **Row-level security is the tenant isolation boundary.** In MariaDB, that becomes view plumbing and application discipline, which is exactly the class of mistake that leaks one shop's data into another shop's search results. Also already in use: `JSONB` with GIN indexes for tenant configuration and specifications, `SELECT ... FOR UPDATE SKIP LOCKED` for the job queue, `LISTEN`/`NOTIFY` for change fan-out, transactional DDL so a failed migration rolls back cleanly, and exact `NUMERIC` for the money type. Migrating would mean rebuilding the isolation model to gain nothing.
**"More efficient" here means tuning what exists, not swapping engines.** At print-shop scale the database is nowhere near a bottleneck, so the effort belongs in correctness and recoverability:
- Right-size `shared_buffers`, `work_mem`, `effective_cache_size` and `random_page_cost` for an SSD desktop rather than shipping defaults.
- Enable `pg_stat_statements` from day one and index against measured slow queries, not guessed ones. Every tenant-scoped table leads with `tenant_id` in its composite index.
- Use connection pooling with a small pool. A desktop serving a handful of staff wants ten connections, not a hundred.
- Partition only the asset and audit tables, and only when row counts justify it. Six thousand assets do not.
- Keep the vector index in LanceDB. **Do not move embeddings into ****`pgvector`** — that would merge two failure domains and put a regenerable Tier 2 artefact inside the one database that must stay small and restorable.
**Resilience is measured by restore, not by configuration.** The controls that matter: WAL archiving with point-in-time recovery so the loss window is minutes rather than a day; nightly `pg_dump` into the encrypted restic Tier 1 backup; **`pg_verifybackup`**** and an actual restore to a clean host on a schedule**, since an unrehearsed backup is a hypothesis; `data_checksums` enabled at initdb time to catch silent corruption early; and a health view surfacing backup age, replication or archiving lag, and last successful restore test. Beyond that, the desktop client's offline SQLite store and durable outbox already carry the shop through a backend outage — that, not database clustering, is the real availability story for a single-server deployment.
#### The embedding service is stateless and does one thing
The refinement is right and it is worth stating as a hard boundary: **the notebook is an inference service, not a pipeline stage.** It holds no data, parses nothing, decides nothing, and remembers nothing between spawns. Everything else — PSD parsing, layer-tree extraction, OCR, AVIF rendering, sanitization, tagging, ranking, storage — happens on the shop server, which is always on and already owns the originals.
This gives the notebook exactly one job: *bytes in, vectors out.* That is the smallest possible surface, and small surfaces are what make disposable compute safe. A runtime that dies mid-batch loses nothing, because there was nothing on it to lose.
**The service contract, in full:**
<table header-row="true">
<tr>
<td>Concern</td>
<td>Where it lives</td>
<td>Why</td>
</tr>
<tr>
<td>PSD parse, layer tree, text layers</td>
<td>Shop server</td>
<td>Needs the original file, which never leaves the premises</td>
</tr>
<tr>
<td>Render to AVIF, resolution clamp</td>
<td>Shop server</td>
<td>Cheap, local, and shrinks what crosses the wire</td>
</tr>
<tr>
<td>Sanitization (visual, textual, structural)</td>
<td>Shop server</td>
<td>Must run before dispatch; re-validated on arrival</td>
</tr>
<tr>
<td>OCR, entity tagging</td>
<td>Shop server (own models)</td>
<td>Different model, different lifecycle, no reason to couple</td>
</tr>
<tr>
<td>Embedding inference</td>
<td>Notebook / Modal / local CPU</td>
<td>The only step that genuinely wants a GPU</td>
</tr>
<tr>
<td>Vector storage, indexing, ranking</td>
<td>Shop server (LanceDB)</td>
<td>Persistence must outlive any runtime</td>
</tr>
</table>
#### What the spawned notebook actually runs
A single startup script with no notebook-specific logic in it, so the same image runs on Colab, Kaggle, or Modal unchanged:
1. Install pinned dependencies from a lockfile, not a loose `pip install` list.
2. Pull the pinned model checkpoint by **revision hash**, not by tag.
3. Report readiness, including the model ID, revision, quantization profile, embedding dimension, and instruction-prefix pair. If any of these disagree with what the shop server expects, the worker refuses the batch rather than producing vectors that silently do not compare.
4. Loop: claim a batch, embed, post vectors back, acknowledge, repeat.
5. Exit on an empty queue, a wall-clock cap, or a shutdown signal.
Nothing is written to the runtime disk except the model weights and the batch currently in flight. There is no local database, no cache that matters, no state that a restart would need to recover.
#### Why this is still a pull loop, not an HTTP endpoint
The instinct to "spawn an inference service and call it" is the familiar shape, but it inverts the availability requirement. An endpoint must be up *when the caller decides to call*. A batch worker only has to be up *sometime*. Since a free notebook runtime cannot promise the first and easily satisfies the second, the pull loop turns platform unreliability from an outage into a delay.
It also keeps the interface identical across every provider. `IEmbeddingProvider` exposes one method — embed this batch, return these vectors — and the local CPU implementation, the notebook implementation, and the Modal implementation are interchangeable behind it. Swapping providers becomes a configuration change, not a rewrite, which is what makes the eventual move to paid compute cheap.
**One exception worth allowing:** if a request-shaped service is genuinely wanted later, put it behind the same interface as a third implementation and let the shop server decide per batch. The contract does not change; only the transport does.
#### The consequences of statelessness that need designing for
- **The model download is paid on every spawn.** Roughly 9 GiB at 8-bit, several minutes of wall clock. This is the strongest argument for large batches and few spawns, and it is why a per-image trigger would be pathological.
- **Batch size is bounded by the runtime's lifetime, not by memory.** Size batches so a full one completes well inside the session cap, with margin for a slow start.
- **Every batch needs an owner and a lease.** A claimed batch that is not acknowledged within its lease returns to the queue automatically. Without this, a reclaimed runtime silently strands work.
- **Vectors are idempotent by content hash.** A batch delivered twice overwrites rather than duplicates, so retrying is always safe.
- **Nothing sensitive crosses the boundary.** Only derived AVIF previews and sanitized text sidecars are dispatched — never original PSDs, never customer records.
#### Not hosting PostgreSQL: TiDB Cloud assessed honestly
The appeal is real — no database to operate, no backups to run, no tuning, free forever at this size. **TiDB Cloud Starter gives 5 GiB of row storage, 5 GiB of columnar storage and 50 million Request Units per month per instance, up to five instances per organization**, and it scales to zero when idle. For a shop's metadata that quota is genuinely generous; the relational data here is small, since the large objects are design files and vectors that were never going in the database anyway.
So the storage math is not the problem. Three other things are, and the first two are decisive.
**1. It is MySQL-compatible, and this design is built on PostgreSQL-only mechanisms.**
<table header-row="true">
<tr>
<td>What the plan depends on</td>
<td>Status on TiDB</td>
<td>Consequence</td>
</tr>
<tr>
<td>Row-level security as the tenant boundary</td>
<td>Not available — a MySQL-family feature set</td>
<td>Tenant isolation moves into application code. This is the single mechanism the white-label promise rests on</td>
</tr>
<tr>
<td>`FOR UPDATE SKIP LOCKED` job queue</td>
<td>**Not supported.** Long-standing open feature requests, accepted but unimplemented</td>
<td>The queue design on this page does not run. Workers contend on the same rows or need a different claiming scheme entirely</td>
</tr>
<tr>
<td>`LISTEN` / `NOTIFY`</td>
<td>Not available</td>
<td>Event fan-out becomes polling</td>
</tr>
<tr>
<td>`JSONB` with GIN indexing</td>
<td>MySQL-style `JSON`, different indexing story</td>
<td>Tenant configuration and branding manifests need rework</td>
</tr>
<tr>
<td>Transactional DDL</td>
<td>Different semantics</td>
<td>The migration guard's failure model changes</td>
</tr>
</table>
The `SKIP LOCKED` gap alone is disqualifying, because it is not a syntax difference — it is the mechanism that lets multiple workers claim jobs without blocking each other, and the entire indexing pipeline is queue-driven.
**2. It moves the database off-premises, which inverts the topology.**
This matters more than the feature list. Today PostgreSQL runs in a Podman container **on the same machine as Oat++**, reachable over a loopback socket. A managed cloud database means every query crosses the public internet from a shop on a consumer connection with no static IP. That produces:
- **A hard internet dependency for the whole product.** The connection drops and quoting, invoicing and search all stop. Right now an internet outage costs remote access; with a cloud database it costs the business day.
- **Latency on every statement**, where there was none. A page rendering thirty small queries pays thirty round trips to another continent.
- **Customer data leaving the premises**, which contradicts the privacy boundary already written here — only derived AVIF previews and metadata are allowed off-site, and the relational database is the one place actual customer records live.
- **A free-tier ceiling on the critical path.** Request Units are a metered resource; exhausting them degrades the core system, not an optional feature.
**3. It removes work that is not currently painful.** The operational burden being avoided is: one Quadlet unit, one nightly `pg_dump` into the restic backup that already exists, and occasional tuning. That is real work, but it is small, local and already designed. The trade is a rewrite of the tenant boundary and the job queue in exchange for it.
**Where a managed database would make sense:** if the architecture ever moves to a rented always-on server and the no-static-IP constraint disappears. At that point the question is worth reopening — with a managed *PostgreSQL* (Neon, Supabase, or similar), not a MySQL-family engine, so none of the above has to be rewritten.
#### The better answer is the one already half-built: tune the Podman deployment
The instinct behind the question is right — there is efficiency to be had. It is just not in changing engines. PostgreSQL already runs under Podman Quadlet here, and that is where the tuning belongs.
**Container-level**
- **Pin the database to a persistent named volume on the fastest local disk**, never an overlay filesystem. Container storage drivers add write amplification exactly where it hurts most.
- **Set explicit CPU and memory limits on the unit**, so the indexing worker and the embedding process can never starve the database. This is the same load-governance principle already applied to the pipeline.
- **Use host networking or a Unix socket between Oat++ and PostgreSQL.** Loopback through a container network stack is pure overhead for two processes on one machine.
- **Quadlet dependency ordering** so the API waits for a healthy database rather than crash-looping into it, and a real health check rather than a port probe.
- **Rootless Podman is correct and stays**, but note that rootless overlay and shared memory need attention — give PostgreSQL an adequate `/dev/shm`, which is a common and confusing source of parallel-query failures in containers.
**Database-level, sized for one desktop rather than a cloud default**
- `shared_buffers` at roughly a quarter of the memory allotted to the container; `effective_cache_size` reflecting what the host actually caches.
- `work_mem` set per-workload rather than globally high — it is allocated per sort, per connection, and is the usual cause of a surprise memory spike.
- `random_page_cost` lowered for SSD, which the default assumes you do not have.
- **A small bounded connection pool**, sized to cores rather than to optimism. A desktop with a hundred idle connections is slower than the same desktop with ten.
- `pg_stat_statements` **enabled from day one.** Without it, "make it more efficient" is guesswork; with it, the slow queries name themselves.
- **`tenant_id`**** leading every composite index**, and partial indexes on the job queue's hot predicate.
- **Autovacuum tuned more aggressively on the job table**, which is high-churn by design, and left alone elsewhere.
**Resilience, which is where the real risk is**
- `data_checksums` enabled at initdb — it cannot be turned on conveniently later, and it is how silent disk corruption becomes visible.
- **WAL archiving for point-in-time recovery**, alongside the nightly logical dump. The dump protects against yesterday; WAL protects against the last ten minutes.
- **A rehearsed restore onto a clean host.** This is already the top open risk on this page and it is unchanged by any of the above. An unrehearsed backup is a hypothesis, and no engine choice substitutes for testing it.
#### What five seconds does *not* excuse
- **A ceiling is not a target.** FTS still returns first and renders immediately; the semantic result streams in behind it. Nobody should watch an empty pane for five seconds because the budget permitted it.
- **CPU inference does not multiplex.** A GPU batches concurrent queries almost for free; a CPU serializes them. Three staff searching at once turns a 2-second query into a 6-second one. Mitigation: a **single-flight queue with a hard concurrency cap**, plus a visible queue position rather than a silent stall. This is the most likely way the budget gets blown in real use, and it has nothing to do with the model.
- **Cache query embeddings** keyed by normalized query text. A print shop asks similar questions repeatedly, and a cache hit costs nothing.
- **Keep the CPU encoder even after Modal.** Modal's GPU memory snapshotting restores a \~9 GiB vLLM checkpoint in **roughly 2–5 seconds** against \~50 seconds for a full cold start — which fits a five-second budget with **no headroom at all**. Scale-to-zero query serving is therefore viable but fragile, and the local encoder stays as the fallback that works when the internet does not. Offline-first is a stated principle; the search box should not be the one feature that violates it.
#### On the index itself: probably do not build one yet
The standard answer is an **IVF-PQ** index: partition the space into cells, compress each vector with product quantization, search only the nearest `nprobes` cells (default 20), then use `refine_factor` to rescore the survivors against the full uncompressed vectors. It is the right structure at scale, and it is where *vector* quantization enters.
**At a print shop's scale it is unnecessary complexity.** Six thousand files at the full 4096 dimensions in float32 are roughly **94 MiB per vector field** — a brute-force scan is comfortably fast on a normal desktop, and it is **exact**. IVF-PQ trades recall for speed you do not need, adds `num_partitions` and `num_sub_vectors` to tune, and introduces a failure mode where search quietly returns slightly wrong neighbours.
**The rule: start with flat/exact search, and add IVF-PQ only when measured query latency actually becomes a complaint.** That threshold is likely in the millions of files, which is to say never for one shop. If it is ever crossed, the escape path is ordered: first drop the Matryoshka dimension from 1024 to 512, then scalar quantization (typically 99%+ recall retained), and only then PQ.
**One operational detail that matters for a nightly batch:** rows written after an index build sit *outside* that index. LanceDB still finds them, via a slower fallback scan, until an optimize pass folds them in. So last night's files are searchable immediately — just on the slow path. Schedule the optimize with the batch, and never call `fast_search()`, which skips the fallback and would make new files invisible.
#### The dispatch path, corrected
The proposed flow — backend signals a module, the module runs Lightning AI, Lightning formats and sends to a notebook, the notebook infers and writes back — has three problems. The shape is right; two of the hops are not.
**1. Colab cannot be triggered at all. Kaggle can.** There is no Colab API for programmatic execution; it is an interactive notebook service and has never offered one. Kaggle, by contrast, has a **public API that pushes and runs a kernel**, plus built-in scheduling (daily, weekly, monthly, or on dataset update). **This settles the Colab-versus-Kaggle question by removing it** — there is no rule-based selection to make between two platforms when only one can be automated. Colab stays a manual tool for interactive experiments; **Kaggle is the interim batch worker.**
**2. The signal flows the other way: the notebook pulls.** Even with the Kaggle API, a home desktop behind a dynamic address cannot reliably push work into a notebook on a schedule the notebook controls. Invert it. The scheduled kernel wakes up, calls home, asks *"what is pending?"*, receives a batch manifest, downloads the AVIF renders, embeds, posts vectors back, and exits. The backend's job is not to *launch* the worker — it is to **maintain a pending queue that a worker can drain whenever one appears.** That design is strictly better anyway: it survives the worker being late, throttled or quota-exhausted, and it is the same interface Modal will use later.
**3. Lightning AI in the middle is pure overhead.** Backend → Lightning → notebook → LanceDB has one more hop, one more account, one more thing to be degraded than backend → notebook → backend. The formatting step Lightning was going to do — batching, manifest building, AVIF packing — is a few hundred lines that already belong in the sidecar, on a machine that is always on and that we control. **This page already rejected a third-party CPU orchestrator once**, for exactly this reason; Lightning's CPU tier being the only thing left on that account is not a reason to find work for it.
**How the notebook reaches home.** It is not on the tailnet, and it should not be: putting an ephemeral third-party VM on the private network to write to LanceDB is a large hole for a small convenience. Use the **cloudflared tunnel that is already in the design** to publish one narrow, token-authenticated ingest endpoint — `claim batch`, `submit vectors`, nothing else. The backend, not the notebook, writes to LanceDB. That keeps the database reachable only from the desktop, makes every write auditable, and means a leaked notebook token costs a rotation rather than an intrusion.
**On jitter, once more.** Spreading retries and staggering batch starts is sound backpressure engineering and stays. Randomizing behaviour so automation resembles a human is not, and it is not what the rule-based dispatcher is for. **The dispatcher's rule is "run when quota and batch size allow," not "run at an unpredictable time so nobody notices."** With Kaggle's 30 h/week quota published and its scheduler built in, using the documented mechanism is both simpler and honest.
#### The module boundaries that keep this from becoming unmaintainable
The request is a modular design that does not inflate complexity. The way to get that is to notice that **every genuinely complicated part of the above is temporary**, and to put each one behind an interface it can be deleted from:
<table header-row="true">
<tr>
<td>Interface</td>
<td>Implementations</td>
<td>Why it is a seam</td>
</tr>
<tr>
<td>`IFileObserver`</td>
<td>Windows NTFS/USN agent; later macOS or a plain polling scanner</td>
<td>The agent is platform-specific; nothing above it should know that</td>
</tr>
<tr>
<td>`IRenderer`</td>
<td>psd-tools compositor; Ghostscript for PDF/AI; ImageMagick for TIFF</td>
<td>New file formats are a recurring request, and each one is one class</td>
</tr>
<tr>
<td>`IEmbedder`</td>
<td>**Local CPU (SigLIP 2)** — the default that must always work; remote batch; Modal</td>
<td>The single most likely thing to change, and the only one with a licence and cost dimension</td>
</tr>
<tr>
<td>`IIndexingDispatcher`</td>
<td>Inline local; **pull-based remote worker (Kaggle)**; Modal scheduled function</td>
<td>**This is where all the temporary ugliness lives.** Quota rules, batch manifests, token auth and retry all sit inside one class that gets deleted at the Modal milestone</td>
</tr>
<tr>
<td>`IVectorStore`</td>
<td>LanceDB via C FFI; LanceDB via the Python sidecar</td>
<td>The binding decision is still an open spike; the rest of the system must not care how it resolves</td>
</tr>
<tr>
<td>`IRanker`</td>
<td>Identity (v1); RRF hybrid; cross-encoder reranker</td>
<td>Already specified; the reranker becomes a config change rather than a redesign</td>
</tr>
</table>
**The test for whether this stayed simple:** deleting the notebook tier at the Modal milestone should touch **one implementation class and one configuration value**. If it would touch the search API, the Qt client, the sidecar or the schema, the abstraction leaked and the complexity became permanent. That is the specific outcome to design against.
#### Scheduled runs and load thresholds
- **Nightly cron on Modal** embeds the day's new and changed files as one batch. No per-file GPU wakeups — that is where serverless GPU budgets die.
- **Threshold trigger:** run early if the pending queue exceeds *N* items or the oldest pending item is older than *T*. Both configurable, both visible in the health view.
- **Size *T* in days, not hours.** A two-hour trigger means up to twelve runs a day, and on a serverless GPU the **cold start — image pull, model load, warm-up — routinely costs more than the inference itself.** Frequency is the expense, not batch size. Nightly, plus a size threshold that fires early only on genuinely large days, is the shape that minimizes cost. A design shop's files are not urgent enough to justify a two-hour freshness target.
- **Scale to zero** between runs (`min_containers = 0`), with a hard `max_containers` cap. Use GPU memory snapshotting to cut cold-start latency for interactive queries.
- **Warm window:** optionally keep one container warm during shop hours only; cold outside. Query-side embedding is a single short input and is cheap either way.
- **Budget guard:** a hard monthly GPU-minute ceiling. On breach, degrade gracefully to full-text-only search and mark the semantic index stale in the UI — never fail the search box.
- **Idempotency:** work is keyed by content hash, so rename, move and restore cost zero GPU time. Only genuine content changes re-embed.
- **Backpressure:** bounded queue, retry with jitter, dead-letter after *n* attempts, and a visible "files awaiting indexing" count.
- **Cheap signals first:** a query only reaches the GPU after metadata FTS has run. Many searches resolve locally at zero cost, and the FTS result is shown immediately while the semantic result streams in behind it.
- **Multi-tenant batching:** once a second shop exists, one nightly run embeds all tenants' pending work together. GPU cold-start cost is amortized across tenants; results stay strictly partitioned by tenant prefix.
#### Evaluation before it becomes the default
Build a gold set of \~100 real "find this file" queries from shop history, in **both Nepali and English**, each with a known-correct file. Measure Recall@10, **Recall@50** and MRR across: filename baseline → metadata FTS → INT8 embedding. The semantic path only becomes the default search mode when it measurably beats metadata FTS, and the FTS path stays available forever as the fallback.
Track Recall@50 from day one even though nothing consumes it yet — the gap between Recall@50 and Recall@10 is the number that decides whether the reranker is ever worth adding.
#### Delivery order within Phase 5
1. Agent discovery + PSD render + AVIF derivatives + metadata extraction, **local only, no GPU**. Ship metadata FTS first — it alone solves a real share of the problem.
2. HF dataset storage tier with sharding, local cache and restore test.
3. LanceDB table, hybrid FTS + scalar filters, C++ binding spike.
4. **Embedder, three tiers in order.** Tier 1: SigLIP 2 on the shop server's CPU — no card, no account, no quota, and always kept working. Tier 2: Qwen3-VL-Embedding-8B in **INT8** on a free notebook GPU for checkpoint building and gold-set evaluation. Tier 3: the same model on Modal with scheduled batch and budget guard, from the first paying customer.
5. Gold-set evaluation and Qt search UI. **The reranker is not in scope for v1** — revisit only if the Recall@50 versus Recall@10 gap justifies it.
**Acceptance:** from an archive of approximately 6,000 designs, a staff member types *"the PSD of International Women's Day where Hands Nepal was a supporter"* — in Nepali, romanized Nepali or English — and receives a scannable grid containing the correct file within five seconds. A vague visual description or dragged reference image is the secondary acceptance case. No filename or folder knowledge is required, and the whole system remains inside its fixed resource budget.
### Phase 6 — Offline synchronization and multi-device ⬜
**Objective:** tolerate connectivity loss without silent financial or workflow conflict.
Versioned SQLite migrations · cache and durable outbox · base revisions · server sequence · tombstones · conflict UI · attachment resume · WSS reconnect · two-client fault injection · device enrollment.
**Acceptance:** operations apply at most once; reconnect catches up from a durable sequence; financial and approval conflicts reject or require human resolution; no SQLite-file replication exists.
### Phase 7 — Adoption by a second business ⬜
**Objective:** onboard a peer shop with configuration and a branding package only — proving the white-label seam holds.
Automated tenant provisioning · signed branding packages · per-tenant document templates and commercial rules · build-once release promotion · per-tenant data export and deletion · tenant-partitioned asset index and storage prefix · formal support expectations · extensions only after signing, sandboxing, revocation and conformance · HA/RPO/RTO only when measured impact justifies it.
**Deliberately still out of scope:** billing, metering, self-serve signup, marketplace.
**Acceptance:** a second shop is provisioned, branded, installed, upgraded, exported and restored from documented automation, **with zero commits to domain code**; both tenants coexist on one server without either seeing the other's data, files or search results.
---
<callout icon="🧾">
	**Quoting and quote-to-order is planned to full depth on its own page:** <mention-page url="https://app.notion.com/p/870d03e520494412aa17fa9ed6b73515">SquiFlow — Quoting & Quote-to-Order: Full Plan</mention-page>. It also records the sequencing for job-to-cash, customer proofing, and white-label tenant onboarding.
</callout>
## Prioritized backlog
<table header-row="true">
<tr>
<td>Priority</td>
<td>Task</td>
<td>Status</td>
<td>Next deliverable</td>
<td>Exit evidence</td>
</tr>
<tr>
<td>P0</td>
<td>Complete financial truth</td>
<td>Partial</td>
<td>Cancellation-specific accounting + shared application transaction boundary for order and AR mutations</td>
<td>Cancellation/refund/change-order scenarios reconcile immutable entries; one command commits or rolls back both sides together</td>
</tr>
<tr>
<td>P0</td>
<td>Job-to-cash exit rehearsal</td>
<td>Not started</td>
<td>Replay representative historical jobs with expected balances, allocations, asset references</td>
<td>No unexplained balance mismatch; source files resolve or are explicitly reported unavailable</td>
</tr>
<tr>
<td>P1</td>
<td>Caddy + Oat++ HTTPS/WSS transport</td>
<td>Decided</td>
<td>Approve patched Oat++ lock, controllers/interceptors, bounded workers, migrate `/api/v1`, RFC 9457, OIDC/PKCE, W3C Trace Context, `/api/v1/realtime`</td>
<td>Route/error parity, HTTP/2 + 1.1 fallback, trusted forwarding, TLS/WSS handshake, reconnect, slow-consumer, edge reload, graceful drain, load tests</td>
</tr>
<tr>
<td>P1</td>
<td>Object-store boundary</td>
<td>Partial</td>
<td>Networked MinIO/S3 adapter, durable object metadata, generated-document persistence, provider compatibility tests</td>
<td>Upload/download/delete, tenant isolation, checksum mismatch, retry, provider replacement, restore tests pass</td>
</tr>
<tr>
<td>P1</td>
<td>Windows asset agent</td>
<td>Not implemented</td>
<td>NTFS File ID + USN checkpoint readers, incremental hashing, metadata extraction, thumbnails, open-file resolution</td>
<td>Rename/move/replace/save/restart and journal-invalidation fixtures produce correct logical-file observations</td>
</tr>
<tr>
<td>P1</td>
<td>Asset search and reconciliation</td>
<td>Not implemented</td>
<td>Local FTS projection plus missing/unlinked/duplicate/stale/unavailable-volume findings</td>
<td>Deterministic reconciliation report distinguishes rename, copy, replacement, restore, missing device</td>
</tr>
<tr>
<td>P1</td>
<td>Auth, RBAC, tenant enrollment</td>
<td>Partial</td>
<td>Persist tenant membership/roles, authenticate API requests, role-limited projections for confidential fields</td>
<td>Unauthorized tenant/role/pricing/mutation tests fail closed through the HTTP boundary</td>
</tr>
<tr>
<td>P1</td>
<td>Production and fulfilment operations</td>
<td>Partial</td>
<td>Change orders, reprints, material/production operations, delivery evidence, pickup, cancellation effects</td>
<td>State transitions, evidence and AR consequences are replayable and auditable</td>
</tr>
<tr>
<td>P2</td>
<td>Quotations and repeat orders</td>
<td>Not implemented</td>
<td>Product templates, typed specifications, PDF rendering from real samples, search, duplication</td>
<td>A representative quotation is created, found, rendered, accepted, converted and duplicated with no manual DB edits</td>
</tr>
<tr>
<td>P2</td>
<td>Backup and restore controls</td>
<td>Not implemented</td>
<td>Backup age/failure visibility, export scope, restore procedure, rehearsal automation</td>
<td>A fresh environment is restored from an actual backup and reconciled before pilot approval</td>
</tr>
<tr>
<td>P2</td>
<td>Notifications and payment evidence</td>
<td>Not implemented</td>
<td>Proof-approval notifications, payment-gateway reference capture without making the gateway the ledger authority</td>
<td>Retries idempotent, provider references auditable, notification failure does not corrupt business state</td>
</tr>
<tr>
<td>P2</td>
<td>Offline sync and live fan-out</td>
<td>Partial</td>
<td>Generalize command schemas beyond draft orders; define attachment resume semantics</td>
<td>Reconnect catch-up, conflict resolution, subscription authorization, interrupted transfer tests pass</td>
</tr>
<tr>
<td>P3</td>
<td>Harden production operations</td>
<td>Partial</td>
<td>Installer/update rehearsal, support-bundle redaction, observability, pool/failover policy, parallel-operation checklist</td>
<td>Pilot gate evidence complete with zero unexplained data, balance or file-index discrepancies</td>
</tr>
</table>
### Working order
1. Close the P0 financial and exit-rehearsal gaps.
2. Build hosted storage, Windows asset and authorization foundations required for real shop data.
3. Complete production/fulfilment evidence and quotation usability.
4. Add backup, notifications, offline fan-out and production-pilot controls.
5. Only then start Phase 5 search and companion intelligence.
---
## Definition of done
<table header-row="true">
<tr>
<td>Status</td>
<td>Meaning</td>
</tr>
<tr>
<td>**Implemented**</td>
<td>Source, contract and deterministic tests exist</td>
</tr>
<tr>
<td>**Compile-verified**</td>
<td>Dependency-gated source builds, but no live external service was exercised</td>
</tr>
<tr>
<td>**Partial**</td>
<td>A useful boundary exists, but the roadmap outcome or production invariant is incomplete</td>
</tr>
<tr>
<td>**Not implemented**</td>
<td>No repository capability satisfies the documented outcome</td>
</tr>
</table>
A phase is **not** complete merely because interfaces or folders exist. Each exit requires code, tests, operations guidance and synchronized user-facing documentation.
Every task should update the affected ADR, architecture/reference page, phase matrix, changelog and executable verification **in the same change set**. Completing a cross-cutting sub-phase does not advance the business roadmap automatically — the repository does not enter the production-pilot tranche until job-to-cash exit criteria are met or explicitly re-baselined through an ADR and roadmap update.
---
## Tool and dependency policy
- CMake + Ninja are the primary build tools; vcpkg manages C/C++ dependencies with a pinned registry.
- Catch2 for framework-neutral tests; Qt Test for Qt/QML behaviour in the workstation repository.
- CLI11 and spdlog become server dependencies when diagnostics and structured logging begin.
- Clang-Format, Clang-Tidy, Cppcheck and sccache are the quality/CI tools.
- PostgreSQL jobs + outbox remain the first durable background-work mechanism; **no message broker is added without measured need**.
- Repository validation rejects former identity tokens (JobWright, octo-umbrella) and brand drift.
- `-std=c++2c` across the tree, with pinned GCC/Clang versions and a maintained C++23 fallback lane.
- A single Python sidecar owns PSD rendering, AVIF encoding and (if the C bindings disappoint) LanceDB — it is a deliberate, bounded exception to the C++ rule, not an invitation to grow a second backend.
- GPU inference is rented, never hosted: Modal serverless, scale to zero, hard budget cap.
- **8-bit quantization on a 24 GB low-tier GPU (L4 default, A10 fallback) is the pinned serving configuration.** Moving to a larger or newer GPU class requires a measured justification, not a convenience argument — the whole cost model depends on staying here.
- The quantized checkpoint is **built once, versioned, and stored alongside the model revision**; inference never quantizes on the fly at container start.
- One model is pinned and recorded alongside the index version: **`Qwen/Qwen3-VL-Embedding-8B`****, INT8 W8A8**. A model, revision or quantization change forces an explicit reindex decision. `Qwen/Qwen3-VL-Reranker-2B` is a known future option, not a current dependency.
- **No tenant-specific code paths.** Branding, documents, commercial rules, vocabulary and enabled workflow steps are data. A pull request that adds a shop name to domain logic is rejected on sight.
- **Tailscale is the transport perimeter**; the application still authenticates and authorizes every request as if the network were hostile.
- **Public exposure is opt-in per endpoint**, via Funnel or Cloudflare Tunnel, and recorded in an ADR — never enabled ad hoc.
- **Every third-party free tier gets a usage metric, an alert threshold and a written degradation path** before it is depended on.
- **restic + rclone** are the backup tools; backups are encrypted client-side, so the storage provider is untrusted by construction. Plain `tar -czf` to cloud storage is not an accepted backup format.
- **Qt ships dynamically linked under LGPLv3 for v1.** Static linking is unlocked only by a purchased commercial licence, and the educational licence is valid for learning and prototyping only — never for a shipped or distributed build.
- **A commercial Qt licence is a hard gate on Phase 7.** No external distribution to a second business happens before it is in place.
- **Every Qt module is licence-checked before adoption** — GPL-only modules (Qt Charts, Qt Virtual Keyboard and similar) are rejected unless the application is deliberately opened.
- **Compute providers are staged by what the work actually is.** Development, checkpoint building and evaluation may run on free notebook platforms; **recurring production indexing for a paying shop moves to Modal**, and the first paying customer is the hard trigger. Orchestration and buffering stay on our own server throughout. No design may depend on concealing its behaviour from a provider, and no notebook platform may share an account with the backup destination.
- **The quantization format follows the hardware, and INT8 is the portable default.** FP8 requires compute capability 8.9+, which no free notebook GPU has. Any change of format forces a gold-set re-run, because format is an accuracy decision.
- **Third-party quantized checkpoints are pinned by revision hash and gold-set validated before use.** Their calibration data is unknown; a tag is not a version. `QuaduxIT/Qwen3-VL-Embedding-8B-W8A16` is the approved interim checkpoint. GGUF builds are rejected for this feature until llama.cpp's multimodal embedding path is released and independently verified — a text-only embedder does not implement visual search.
- **The whole index shares one embedding model.** Vectors from different models are not comparable, so an embedder change is a full reindex, never a config flip. The model, revision, quantization format and Matryoshka dimension are recorded alongside the index version, and a mixed-model table is treated as corrupt.
- **Query-time embedding must run on always-available hardware.** A batch-only GPU may build the index but may never be on the search path. The query encoder therefore runs locally on CPU, using **the same checkpoint that built the index** — asymmetric compute, symmetric model. Hosted GPU inference may accelerate this path but may never be the only implementation of it.
- **Query latency budget is five seconds, and it is a ceiling rather than a target.** Full-text results render immediately and semantic results stream in behind them. Query images are capped at 512 px, query embeddings are cached, and concurrent queries are serialized through a single-flight queue with a visible position.
- **Model size is justified by gold-set evidence, not by leaderboard rank.** The 2B and 8B variants are both evaluated on real shop queries before one is pinned, because nearly every quantization and hardware constraint on this page is downstream of that single choice.
- **One model embeds both sides of the search, always.** Indexing with one model size and querying with another is invalid, not merely degraded — the 2B emits up to 2048 dimensions and the 8B up to 4096, and Matryoshka truncation aligns dimensions within a model rather than across models. Where asymmetry is wanted, vary the work done per item, never the weights.
- **Text extraction outranks embedding as a retrieval signal.** Live PSD text layers are extracted first, OCR fills the gap for outlined or flattened files, and full-text search is the primary path. The semantic index serves queries that carry no recoverable text, and no release is blocked on it.
- **OCR runs at index time only.** It is a batch concern and may live on remote compute; it may never appear on the query path.
- **Recorded relationships beat inference, always.** Where a file is linked to a job, quotation or party, that relation answers "whose file is this" exactly and is queried before any search runs. Visual or textual inference exists only for files that predate the system or arrived outside it.
- **Visual entity recognition writes text at index time; it never runs at query time.** Logo matching resolves a mark to an organization name during the nightly batch and stores that name as an ordinary indexed text field, so the query path remains full-text only.
- **Inferred entity tags are suggestions until a human confirms them**, are visually distinct from recorded facts, may never cross a tenant boundary, and may never mutate a business record. Precision is thresholded above recall, because a wrong customer attribution is a confidentiality failure rather than a search miss.
- **The full PSD layer tree is indexed, not just its text.** Layer and group names, layer kind, visibility and **bounding boxes** are all extracted and stored, because position and grouping answer structural questions ("as a supporter", "in the footer") that no amount of text or semantic matching can.
- **Logo detection runs at full render resolution, not on the downsampled search render.** Detection resolution and embedding resolution are independent settings; a sponsor-strip mark disappears entirely at 1024 px.
- **Search is faceted intersection first, ranking second.** Queries are decomposed into file type, date window, event, organization and role, and each facet is a cheap filter. The system's target is reducing the candidate set to a scannable grid, not producing a perfect top-1.
- **The query parser is rule-based and visible.** Organizations resolve against the existing party table, roles and recurring events against small maintained lexicons, and the parsed facets are shown as editable chips so a failed search can be widened by hand rather than retyped.
- **A recurring-event lexicon with date windows is maintained as product data**, in Nepali and English, because a print shop's calendar repeats annually and a date window is the cheapest large filter available.
- **No component is written where an established, permissively licensed project already does the job.** Parsing, compositing, resizing, encoding, OCR, feature matching, indexing, and backup are all dependencies. The code written here is glue, domain vocabulary, and interface.
- **The embedded PSD composite is used whenever the file carries one**, with full layer compositing as the fallback only. The fallback rate is recorded per file, because it drives the backfill's duration.
- **AVIF encoding uses a faster speed setting for backfill than for the archival copy**, and both settings are chosen by measurement rather than left at the default.
- **PaddleOCR is the default OCR engine and Tesseract ****`nep`**** the CPU fallback**, but the choice is provisional until both are scored on a sample of the real archive.
- **Instance matching uses OpenCV's MAGSAC++ rather than plain RANSAC**, with the accept threshold fitted on the gold set instead of guessed.
- **The full-text index uses the ICU base tokenizer**, with the n-gram tokenizer retained for substring and partial-name matching.
- **The job queue is PostgreSQL with ****`FOR UPDATE SKIP LOCKED`****.** No broker, no workflow engine, no scheduler service.
- **Every pipeline stage is separately queued, separately retryable, and keyed by the content hash of the source file**, so that any run is idempotent and any interruption is resumable.
- **A single low-priority worker with a hard memory ceiling, throttled during shop hours.** Indexing must be unnoticeable on the machine it runs on.
- **An extracted fact confirmed by two independent extractors is indexed silently; one is indexed as a suggestion; a contradiction is escalated to a human.**
- **The production quality profile uses Qwen3-VL-Embedding-8B with 4096-dimensional float32 vectors and exact search for the approximately 6,000-file corpus.** The 2B profile is a documented low-memory fallback, not the default; changing profiles requires a separate index and gold-set comparison.
- **Search results expose stable asset IDs, evidence and resolved preview URLs — never raw vectors and never cache paths as identity.** Absolute source paths are mutable projections maintained by the Windows file agent.
- **Explorer integration uses supported Windows Shell APIs.** A global shortcut ships before any shell extension; reveal/open actions resolve the current path by asset ID and use `SHOpenFolderAndSelectItems`.
- **No input reaches an embedding model without passing the sanitizer.** One stage, shared by indexing and querying, enforcing: defensive decode with quarantine, a total-pixel ceiling, colour and alpha normalization, NFC normalization, whitespace and control-character cleansing, and tokenizer-measured truncation at a clean boundary.
- **Task instruction prefixes are part of the pinned embedding profile.** Query and document prefixes are recorded alongside the model, revision, quantization and dimension; changing one invalidates the index exactly as a model change does.
- **The indexer never fetches a URL supplied by input.** Images are resolved from the local file agent and the derivative store by content hash. Any future remote fetch requires an HTTPS-only allowlist, private-range DNS rejection, no cross-host redirects, a byte cap and a timeout. Base64 payloads are size-capped, bounded on decode, and identified by magic bytes.
- **Preprocessing runs on the shop server, never on a third-party CPU.** Remote workers re-validate everything they receive; trust is never inherited across the boundary.
- **PostgreSQL is the relational store and the decision is closed.** Row-level security is the tenant isolation boundary, and `JSONB`, `SKIP LOCKED`, `LISTEN`/`NOTIFY`, transactional DDL and exact `NUMERIC` are all load-bearing. Efficiency work means measured tuning and indexing, not an engine swap. Embeddings stay in LanceDB; `pgvector` is explicitly not adopted.
- **Remote inference is consumed through one provider contract, and the notebook may only implement its batch half.** Any tunnel-exposed endpoint is authenticated, rate-limited, size-capped and treated as a development fixture; the query path never depends on it.
- **Devanagari text is normalized to Unicode NFC at both index and query time**, with an n-gram field and a romanized transliteration indexed alongside the tokenized field, because Tantivy provides no Indic stemmer.
- **Start with exact vector search; add an ANN index only when measured latency demands it.** IVF-PQ is a scale optimization with a recall cost, and a single shop's archive does not reach the scale that justifies it.
- **Remote workers pull work; the backend never pushes into them.** The backend maintains a pending queue and a narrow, token-authenticated ingest endpoint. Remote compute never holds a database credential, never joins the tailnet, and never writes to LanceDB directly.
- **Upstream dependencies are patched, never forked.** Modifications ship as reviewable patch files applied by a vcpkg overlay port to a pinned upstream revision, and every patch is submitted upstream. A patch that survives two upstream releases unaccepted triggers a review of the patch or the dependency. **Modernizing a working dependency's internals is not an accepted reason to patch it** — only compilation breakage under `-std=c++2c`, bugs, or security fixes are.
- **The embedding model is a swappable interface with a CPU default.** No release may be blocked on access to a hosted GPU; the local CPU tier must always remain a working configuration.
- **Model licences are checked exactly like library licences.** Non-commercial weights (CC BY-NC and similar, including `jina-clip-v2`) are rejected outright — the shop earns revenue.
---
## Open risks
- **No restore rehearsal yet** — real production data is blocked until a backup is actually restored to a clean host and reconciled.
- **No live database/object-store run** — current PostgreSQL and MinIO evidence is compile plus CI automation, not a local live-integration pass.
- **Oat++ dependency revision unapproved** — the entire hosted transport migration sits behind this lock.
- **Name fragmentation** — three historical names across repos and docs; the phase matrix and task backlog in the active repo are the authoritative sources when documents disagree.
- **C++26 is experimental in shipping compilers** — contracts and reflection in particular. The feature-macro guard plus the C++23 fallback lane is the mitigation; a toolchain regression must never be able to block a shop release.
- **LanceDB has no first-party C++ SDK** — the C FFI bindings are community-maintained. Spike early; the Python sidecar fallback must stay viable.
- **Hugging Face is a storage tier, not durability** — quotas are enforced, private free-tier space is \~100 GB, and terms can change. Originals stay on shop hardware with an independent cold backup.
- **Customer designs are sensitive** — only derived AVIF renders and metadata leave the premises, to a *private* repo. Any change to that boundary needs an explicit decision, not a config tweak.
- **Serverless GPU cost is unbounded by default** — the scheduled-batch plus budget-cap design is the control; per-file real-time embedding is explicitly rejected.
- **Semantic search can quietly regress** — without the gold set and periodic re-evaluation, a model or dimension change degrades recall invisibly.
- **Physical and infrastructure risk is accepted and out of scope.** Power, hardware failure, single-server topology and HA are not tracked here. The offline-first desktop (Phase 6) and the tested restore (Tier 1 backup) already cover the cases that would otherwise cost data; everything beyond that is a deliberate non-goal.
- **8-bit quantization is an accuracy decision, not just a memory trick** — retrieval quality can degrade in ways a smoke test will not reveal. The gold set must be run against the quantized checkpoint specifically, never against the FP16 model, and re-run whenever the quantization format or model revision changes.
- **A 24 GB card leaves little headroom** — an uncapped render resolution, an unbounded context length or an oversized batch causes OOM, not slowness. Resolution caps, `--max-model-len` and batch size are load-bearing configuration, and the L40S is the documented escape hatch.
- **INT8 accuracy depends on calibration quality** — a checkpoint calibrated on generic text can quietly under-perform on design renders. Calibrate on real shop data and validate the quantized checkpoint on the gold set, never the FP16 model.
- **Free tiers change their terms** — Tailscale user counts, Hugging Face quotas and object-store free allowances have all moved before. Nothing architectural may depend on a specific free limit; each has a named paid successor and a documented migration.
- **Tailscale Funnel has no WAF or rate limiting** — any customer-facing endpoint published this way is a soft target. Prefer Cloudflare Tunnel for public traffic, and keep authentication mandatory even on "just a proof link."
- **White labeling can rot into forking** — the first time someone patches domain code for a specific shop, the multi-business promise is dead. The single-build, config-only rule needs an automated check, not just good intentions.
- **A second tenant multiplies storage and GPU demand** — free-tier headroom that comfortably fits one shop may not fit two. Per-tenant quotas and usage visibility must exist *before* onboarding, not after the bill arrives.
- **Qt licensing is the largest legal exposure on this page** — static linking under LGPLv3 without a relinking mechanism, or shipping anything built under the educational licence, is a breach that surfaces precisely when the product succeeds. The mitigation is a dynamic-linked v1 and a purchased Small Business licence gating external distribution.
- **Provider terms are a design constraint, not paperwork** — a pipeline that violates them can be terminated without notice and without appeal, and account-level enforcement can take out compute and backup together if they share an identity. Compute, storage and DR must not sit behind the same third-party account.
- **Model weight licences are a live risk** — several of the strongest multimodal embedding models are released non-commercially. Every candidate model's licence is verified before it enters an evaluation, not after it wins one.
- **No payment method yet** — this constrains the hosted GPU tier only. The mitigation is structural rather than financial: a CPU embedding tier that always works, a notebook GPU for evaluation, Modal from the first paying customer, and a delivery order that puts every card-free component first.
- **Free-tier claims are unreliable until an account proves them** — the Lightning AI "80 GPU hours per month" figure came from a vendor page and turned out to be a one-time credit on an account since degraded to CPU-only. No plan may depend on an advertised allowance that has not been observed in a real account, and every free tier on this page inherits that caveat.
- **The interim notebook GPU is a known, time-boxed risk, not an endorsed design** — Kaggle's non-commercial terms and Colab's restrictions on headless automation mean this tier can stop working without appeal. It is acceptable while the work is development and evaluation; it becomes unacceptable the moment a paying shop depends on it, which is exactly why the Modal trigger is written into the delivery order.
- **FP8 on free hardware is a plan that cannot execute** — free notebook GPUs are Turing or Pascal, below the compute capability FP8 W8A8 requires. Any plan specifying FP8 must also specify Ada-or-newer hardware, or it is specifying nothing.
- **A silently degraded embedder is worse than a broken one** — a mis-converted checkpoint still returns ranked results, just poor ones, and no exception is ever thrown. Published reports of GGUF embedding conversions scoring a third of the original model's retrieval quality are the cautionary case. This is precisely why the gold set gates every checkpoint change, including a change between two quantizations of the same model.
- **A misattributed logo is a confidentiality incident, not a bad search result** — it places one customer's design under another customer's name, and in a multi-tenant white-labeled product that is the most damaging failure available. High thresholds, human confirmation and a hard rule against inference crossing tenant boundaries are the controls; none of them may be relaxed for recall.
- **Logo matching degrades on the cases most likely to matter** — heavily stylized marks, low-contrast placements, logos overprinted on imagery, and marks that were redrawn rather than placed. The clustering-and-naming workflow partially compensates by surfacing frequency, but there is no guarantee the long tail is ever covered, and the fallback must remain an ordinary browse-by-customer path.
- **The legacy archive has no filenames, no folder convention and no job links to lean on** — files are saved at random, so every retrieval signal must be derived from file contents. The relational join designed into Phase 3 helps only files created through the system after go-live, and it must not be counted as a mitigation for the archive that actually causes the two-hour searches.
- **The one-time bulk backfill is a much larger compute event than the nightly batch and was not budgeted** — rendering, extracting and OCR-ing tens of thousands of existing files runs for days, and the feature is worthless until it completes. It must be resumable, idempotent and interruptible, and its progress must be visible or it will be assumed to have failed.
- **Model weight licences are not the same as code licences, and two candidates are already suspect.** Surya's code is Apache-2.0 but its weights are governed by separate commercial terms; DINOv2's weights are CC BY-NC and DINOv3 ships under its own Meta licence. For a for-profit, white-labelled product none of these can be assumed usable. Every model must have its weight licence verified in writing before it enters the build, exactly as with Qt.
- **Published OCR benchmarks disagree sharply with each other and none of them evaluates Nepali banner typography.** One 2026 comparison ranks Surya last on every test set; Surya's own published figures put it at the top of its size class. Neither is evidence about this corpus, and treating either as evidence would be a mistake.
- **AVIF encoding is likely the dominant cost of the backfill and has not been measured.** Encode times of seconds per large canvas are plausible at default settings, which over tens of thousands of files is the difference between one night and one week. This is the single most important number to measure before committing to a backfill schedule.
- **Two raster paths exist and may diverge.** ImageMagick handles layered PSDs; libvips is faster for resizing and encoding. Running both means two sets of colour-management behaviour, and a colour shift between the search thumbnail and the archival render would be a visible defect in a print shop of all places.
- **The actual archive is approximately 6,000 files, but its composition is still unknown.** PSD/PSB share, duplicate-content share, average canvas size and embedded-composite rate must be measured before estimating the backfill duration.
- **The high-accuracy 8B profile is only operational if the same embedding space is available for query encoding.** GPU and CPU runtimes or quantizations must pass a cross-runtime parity test on the gold set; otherwise FTS remains immediate and semantic search is marked unavailable rather than mixing incompatible vectors.
- **Absolute paths and local AVIF cache paths are machine-specific and mutable.** Treating either as identity would break on rename, restore or a second workstation; stable asset, volume/file identity and derivative keys are required.
- **A static tunnel hostname makes an unstable service look stable.** A Colab runtime behind a fixed domain is still interactive, time-limited and reclaimable; callers that see a permanent address will stop treating outages as expected. It is also inbound public exposure, contradicting the private-by-default topology, and it is the clearest possible signature of the headless use Colab's terms restrict.
- **A missing or mismatched instruction prefix degrades retrieval silently.** No error is raised, results still rank, and the failure is invisible without the gold set. This is the most likely way the search quietly gets worse after a refactor.
- **Moving the relational database off-premises would make the whole product depend on a consumer internet connection.** Today an outage costs remote access; with a hosted database it costs quoting, invoicing and search. Any managed-database proposal must be evaluated against that failure mode first, and against the PostgreSQL-only mechanisms (RLS, `SKIP LOCKED`, `LISTEN`/`NOTIFY`) that the tenant boundary and job queue are built on.
- **The share of files with recoverable text layers is unknown and decides the shape of Phase 5** — print shops convert text to outlines before output, which destroys the string. If most of the archive is outlined or flattened, OCR moves from a gap-filler to the core of the feature and its cost was not budgeted. Sample 200 real files and count before committing to a design.
- **Nepali OCR on decorative banner typography is the weakest link in a text-first design** — display faces, outlines, gradients and text over imagery are the hard case for every engine, and certificates will measure far better than banners. Evaluate the two categories separately or the average will hide the failure.
- **Devanagari search can fail silently** — an NFC/NFD normalization mismatch between index and query returns zero results while looking exactly like missing data, and no Indic stemmer exists in Tantivy. Normalization, an n-gram field and a transliteration field are correctness requirements, not enhancements.
- **The query encoder competes with the rest of the stack for RAM** — roughly 9 GB resident for the 8B model at 8-bit, alongside PostgreSQL, Oat++, the sidecar and possibly Qt on the same desktop. A 16 GB machine is genuinely tight; 32 GB is the comfortable configuration, and the 2B variant is the mitigation if the hardware cannot be changed.
- **CPU query serving serializes under concurrent use** — unlike a GPU, it cannot batch across users, so latency degrades linearly with simultaneous searches. The concurrency cap and queue are load-bearing, not polish, and this is the most likely cause of a missed latency budget in real shop use.
- **CPU inference speed is a claim that must be measured, not assumed** — the prefill figures behind the query-path design come from published benchmarks on other people's hardware. A spike on the actual shop machine, with the actual model and an actual query image, must confirm them before the design depends on them. This page has already been wrong once by trusting a number it did not verify.
- **An embedder change is the most expensive change in the system** — it invalidates every stored vector at once. The cost is bounded only by having planned the reindex, kept originals and derivatives, and treated the index as regenerable Tier 2 data. A model swap made casually would be unrecoverable.
- **The interim architecture is deliberately temporary and must stay deletable** — the pull-based remote worker, its quota rules and its ingest endpoint exist only until Modal. If that code spreads beyond its interface, the Modal migration stops being a deletion and becomes a rewrite, and the interim risk becomes permanent.
- **Adding a self-hosted PaaS layer would duplicate the runtime.** Dokploy is Docker Swarm and Traefik based, while this plan already specifies Podman Quadlet and Caddy. Running both means two container runtimes and two reverse proxies on one desktop, which is precisely the maintainability cost this design is trying to avoid. One or the other, chosen deliberately.
- **Forking a framework converts a one-time gain into a permanent maintenance liability**\} — and does so on Oat++, the dependency already blocking the P1 transport migration. The patch-set-over-overlay-port mechanism is the mitigation; the discipline it requires is that internal modernization is never itself a reason to diverge from upstream.
<page url="https://app.notion.com/p/d01adc14460b4a1e9fac575b7ed8cbcf">SquiFlow — Design-File Search: Execution Plan</page>
<page url="https://app.notion.com/p/870d03e520494412aa17fa9ed6b73515">SquiFlow — Quoting & Quote-to-Order: Full Plan</page>
<page url="https://app.notion.com/p/f955d6b8687b499ba779a042688757e6">SquiFlow — Feature Set & Usage Specification</page>
<page url="https://app.notion.com/p/c4f8998f6911462c819be5946ed68894">SquiFlow — Runtime Architecture, Sync & Delivery</page>
<page url="https://app.notion.com/p/2ac5a2eb71ad47f58ef2320c93f1a3c0">SquiFlow — Codebase Structure, Modules, Updates & Build Pipeline</page>
<page url="https://app.notion.com/p/b5ea1a1c336447ef8c26dadef029eb7f">SquiFlow — Workstation & Server File Structure</page>
<page url="https://app.notion.com/p/0d412d670a614d599acdf1610ff23a36">SquiFlow Workstation — Codebase Production</page>
