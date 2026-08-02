# SquiFlow — Design-File Search: Execution Plan

Source page id: d01adc14460b4a1e9fac575b7ed8cbcf

---

<callout icon="🧭">
	**This page is the build order, not the design.** The parent plan argues *what* the system should be and *why*. This page turns the settled parts into an ordered sequence of milestones that each end in something the shop can actually use. Where the parent page leaves a decision open, this page states the **measurement that closes it** rather than guessing.
</callout>
## How this plan is sequenced
Five rules determine the order, and they explain every apparently strange decision below.
1. **Measure before building.** Three numbers decide the shape of this feature: the share of files with live text layers, the share with an embedded composite, and AVIF encode time per file. All three are cheap to measure and expensive to assume. M0 exists solely to get them.
2. **Every milestone ends in a usable capability.** No milestone is "build the infrastructure for X." The first milestone that ships puts a working search box in front of staff, even though it only searches layer names.
3. **Card-free work comes first — but card-free-first is not embedding-last.** Everything in this plan runs with no account, no quota and no payment method, **and that includes the embedding tier**: its local CPU query path needs nothing, and its batch indexing path runs on the free notebook GPU. Only the *Modal throughput upgrade* is blocked on money, and only that is deferred. An earlier version of this page confused those two things and pushed the whole embedding tier to the end; that was wrong and is corrected below.
4. **Interfaces before implementations.** Each stage is defined as a named contract with a trivial default implementation first, so the expensive version can be swapped in without touching callers. This is what makes the CPU→GPU and FTS→semantic transitions configuration changes rather than rewrites.
5. **The backfill is the real test, and it comes early.** The two-hour search problem lives entirely in the legacy archive. A pipeline that works on ten files proves nothing; the first honest signal is running it over the whole corpus.
---
## M0 — Measure the archive
**Size:** small. **Blocks:** almost every estimate on the parent page. **Needs:** a Python script and read access to the drive.
This is a throwaway script, not a component. It walks the watched roots, opens a random sample of **200 files**, and writes one CSV row per file. Nothing is stored, indexed or rendered permanently.
<table header-row="true">
<tr>
<td>Number to produce</td>
<td>How</td>
<td>What it decides</td>
</tr>
<tr>
<td>PSD/PSB share of the \~6,000 files</td>
<td>Extension count over the full walk, not the sample</td>
<td>Whether non-PSD formats are in scope for v1 at all</td>
</tr>
<tr>
<td>Share with **live text layers**</td>
<td>`psd-tools`: count layers with `kind == 'type'` and non-empty text</td>
<td>Whether OCR is a gap-filler or the core of the feature</td>
</tr>
<tr>
<td>Share with an **embedded composite**</td>
<td>Presence of the merged-image block ("Maximize Compatibility")</td>
<td>The single largest lever on backfill duration</td>
</tr>
<tr>
<td>**AVIF encode seconds per file**</td>
<td>Time the render+encode on 50 real files at two speed settings</td>
<td>Whether the backfill is one night or one week</td>
</tr>
<tr>
<td>Share with a **recognizable organization logo**</td>
<td>Human eyeball on the sample's thumbnails</td>
<td>Whether M7 is worth building</td>
</tr>
<tr>
<td>Share where layer **groups carry structural names** (`sponsors`, `footer`, `सहयोगी`)</td>
<td>Group-name frequency table across the sample</td>
<td>Whether "as a supporter" is answerable from the layer tree for free</td>
</tr>
<tr>
<td>Average canvas size, and the p95</td>
<td>`psd.width × psd.height`</td>
<td>Memory ceiling for the worker; the pixel clamp</td>
</tr>
<tr>
<td>Duplicate-content share</td>
<td>Content hash collisions over the full walk</td>
<td>How much of the backfill is redundant work</td>
</tr>
<tr>
<td>Timestamp distribution by month</td>
<td>Filesystem plus EXIF/XMP dates</td>
<td>Whether the date-window filter really removes 97%</td>
</tr>
</table>
**Acceptance:** a one-page table of these numbers, committed to the repository, with the sample method recorded. **Every estimate downstream cites this table instead of a guess.**
<callout icon="⚠️">
	**Do not skip this and do not fold it into M2.** Two of the parent page's largest open risks — "the share of files with recoverable text layers is unknown" and "AVIF encoding is likely the dominant cost of the backfill and has not been measured" — are closed by a script that takes a day. Building the pipeline first means discovering the answer after committing to a design.
</callout>
---
## M1 — The skeleton: contracts, queue, and an empty pipeline
**Size:** medium. **Depends on:** nothing. **Runs on:** the shop desktop only.
The goal is a pipeline that processes files end to end and produces *nothing useful* — so that every later milestone is filling in one stage rather than wiring plumbing.
### Interfaces to define, with trivial defaults
These extend the module boundaries already named on the parent page.
<table header-row="true">
<tr>
<td>Interface</td>
<td>Responsibility</td>
<td>Default implementation in M1</td>
</tr>
<tr>
<td>`IFileObserver`</td>
<td>Discover and identify files; emit change events</td>
<td>One-shot directory walk, no USN journal yet</td>
</tr>
<tr>
<td>`IJobQueue`</td>
<td>Enqueue, claim, lease, ack, retry, dead-letter</td>
<td>Real — PostgreSQL with `FOR UPDATE SKIP LOCKED`</td>
</tr>
<tr>
<td>`IRenderer`</td>
<td>Source file → normalized raster at a requested size</td>
<td>Throws `NotImplemented`</td>
</tr>
<tr>
<td>`ITextExtractor`</td>
<td>Source file → layer tree, text layers, smart-object names, metadata</td>
<td>Returns the filename only</td>
</tr>
<tr>
<td>`IOcrEngine`</td>
<td>Raster → recognized text with confidence</td>
<td>Returns empty</td>
</tr>
<tr>
<td>`IInputSanitizer`</td>
<td>The single validation stage for both index and query input</td>
<td>Real — it is cheap and everything depends on it</td>
</tr>
<tr>
<td>`IEntityTagger`</td>
<td>Raster → matched organizations with position and confidence</td>
<td>Returns empty</td>
</tr>
<tr>
<td>`IEmbeddingProvider`</td>
<td>Sanitized text or image → vector, plus the pinned profile it used</td>
<td>Returns empty; refuses rather than emitting a fake vector</td>
</tr>
<tr>
<td>`IVectorStore`</td>
<td>Upsert by content hash, filtered search, index stats</td>
<td>Real — LanceDB, exact search, no ANN</td>
</tr>
<tr>
<td>`IQueryParser`</td>
<td>Sentence → facets plus residual free text</td>
<td>Everything is residual free text</td>
</tr>
<tr>
<td>`IRanker`</td>
<td>Candidate list → ordered list</td>
<td>Identity ordering</td>
</tr>
</table>
**The rule that makes this worth doing:** a stage that is not implemented **refuses loudly and records why**. It never returns a plausible empty result that looks like "nothing matched." The parent page's worst failure mode is a silently degraded pipeline, and this is the first defence against it.
### The queue, concretely
One table per concern is wrong here; one table with a stage column is right, because the retry, lease and priority logic is identical for every stage.
- **Row identity is the content hash of the source file plus the stage name.** Re-enqueueing the same work is a no-op, which is what makes the whole pipeline resumable and idempotent.
- Columns that earn their place: `tenant_id`, `content_hash`, `stage`, `state`, `attempts`, `lease_expires_at`, `priority`, `last_error`, `created_at`, `updated_at`.
- **A partial index on the claim predicate** (`state = 'pending'` ordered by priority) — this is the only hot query and it should never scan.
- **`tenant_id`**** leads every composite index**, and RLS applies here exactly as everywhere else.
- Claiming is `SELECT … FOR UPDATE SKIP LOCKED LIMIT n`, then a state update in the same transaction. A crashed worker's rows return through **lease expiry**, not through a cleanup script.
**Acceptance:** the worker picks up 6,000 files, moves each through every stage, records "not implemented" for the stages that are stubs, and finishes. Kill the worker mid-run; restart it; it resumes without duplicating work and without losing any file.
### Resource governance, built in from the start
This is not a later optimization — retrofitting it means retrofitting every stage.
- **One worker process, low priority**, with a hard memory ceiling enforced by the container unit rather than by hope.
- **Throttled during shop hours** on a schedule, full speed outside them. The desktop is someone's working machine.
- **A per-stage concurrency cap**, because rendering is memory-bound and OCR is CPU-bound and they should not compete.
- **Progress is visible** — files done, files remaining, current stage, estimated completion. A long backfill with no progress indicator is indistinguishable from a hung one.
---
## M2 — Render and derive
**Size:** medium. **Depends on:** M0 (for the composite share and encode timings), M1.
### Tasks
1. **Implement ****`IRenderer`**** over the embedded composite path first.** If the file carries a merged preview, use it — this is the single trick that makes the backfill affordable. Record the fallback rate as a metric from day one.
2. **Full layer compositing as the fallback only**, with a per-file timeout. A pathological file must fail its job, not stall the queue.
3. **Three AVIF derivatives**: 256 px grid thumbnail, 1024 px search render, 2048 px proof view. Encoder speed setting differs between the backfill and the archival copy, and both settings come from M0's measurements.
4. **Enforce the sanitizer's pixel ceiling before the encoder**, not after. Clamp on **total pixels**, not the long edge — an extreme aspect ratio is how a memory ceiling gets breached.
5. **Derivative keys are content-hash-derived**, never path-derived. A moved or renamed file must not orphan its own renders.
6. **Colour management is pinned and identical across both raster paths.** A visible colour shift between the thumbnail and the proof view would be a defect a print shop notices immediately.
**Acceptance:** every file in the corpus has three derivatives or a recorded, categorized failure. The failure categories are enumerated, not lumped into "error." Re-running the stage changes nothing.
---
## M3 — Text extraction and full-text search
**This is the milestone that makes the feature real.** Everything before it is plumbing; everything after it is improvement.
**Size:** large. **Depends on:** M1, M2 (for OCR input). **Runs on:** CPU, free, offline.
### Tasks
1. **Implement ****`ITextExtractor`**** over the full layer tree** via `descendants()`. Capture per layer: `name`, `kind`, `visible`, and the `bbox` as (left, top, right, bottom). **The layer tree is a layout map, not a bag of strings** — the bboxes are what later answer "as a supporter."
2. **Text-layer contents**, extracted as-is, both scripts.
3. **Smart-object and linked-file names** — a placed logo usually names itself, and this costs nothing.
4. **File and document metadata**: dimensions, colour mode, spot colours, fonts, XMP/EXIF dates, filesystem timestamps. **Store each date source separately**; "when the job was sold" and "when the file was last touched" are different questions and staff ask both.
5. **Folder path tokens**, because shops organize by customer more often than they admit.
6. **OCR, scoped by M0's answer.** If the live-text share is high, OCR is a gap-filler on the flattened minority. If it is low, OCR is the core of this milestone and needs its own bake-off — PaddleOCR against Tesseract `nep` on a sample of \~50 real Nepali banners, scored separately for banners and certificates because they will behave very differently.
7. **Full-text index with the Devanagari handling that is a correctness requirement, not a nicety**: NFC normalization at both index and query time, an n-gram field alongside the tokenized one, and a **romanized transliteration field** so that typing "Ram Bahadur" finds राम बहादुर.
8. **A ****`stage`**** marker on every extracted fact recording which extractor produced it** — layer text, OCR, smart-object name, folder token. Provenance is what later lets two agreeing extractors be trusted and one be treated as a suggestion.
**Acceptance:** a command-line query against the index finds a file by a phrase that appears in one of its text layers, in Nepali and in English, with no model involved. NFC and transliteration cases both pass. **This is the first point at which the two-hour problem measurably shrinks.**
---
## M4 — Search API and the desktop overlay
**Size:** large. **Depends on:** M3.
### Backend
1. **`/api/v1/assets/search`** — text query, uploaded image, or asset ID for "more like this." Same auth, same tenant scoping, same RFC 9457 problem responses as every other route.
2. **Filters are prefilters, applied inside the store**, never postfilters. In a multi-tenant system postfiltering is a correctness hazard as well as a performance one.
3. **Response returns stable asset IDs, evidence, facets and derivative URLs.** Never raw vectors, and **never a cache path as identity** — absolute paths are mutable projections maintained by the file agent, resolved on demand at open time.
4. **Evidence is part of the contract, not a debugging aid.** Each result says *why* it matched — which text field, which extractor, which facet. Staff trust a result they can see the reason for, and a wrong result becomes diagnosable instead of mysterious.
5. **FTS results return immediately** and are rendered before anything slower runs. The five-second budget is a ceiling for the slow path, never a target for the fast one.
6. **Reindex and backfill run through the job queue**, never on a request thread.
### Desktop
1. **A global shortcut opens a Raycast-style overlay.** Ship the shortcut before any shell extension — shell extensions are a support burden and are not needed to prove the idea.
2. **Thumbnail grid with progressive AVIF loading.** The target is a scannable grid, not a perfect top-1.
3. **Reveal in Explorer** and **open original**, resolving the current path by asset ID via `SHOpenFolderAndSelectItems`.
4. **Honest states**: index stale, volume offline, semantic tier unavailable. Never a silent empty result.
**Acceptance:** from Explorer, a staff member presses the shortcut, types a phrase, and opens the right file — without knowing its name or folder. Measured against the M0 sample, not a demo file.
---
## M4b — The embedding tier (committed, built in parallel with M3–M4)
**Size:** medium. **Depends on:** M1 (contracts, queue, sanitizer) and M2 (renders). **Not** dependent on M6 or M7. **Runs on:** shop CPU for queries, free notebook GPU for batch indexing.
<callout icon="📌">
	**Correction to an earlier version of this page.** This tier was previously written as "M8," placed last, described as something that "may not ship in v1 at all," and gated on a measured residual. That was this page substituting its own preference for a decision that was already made. **Qwen3-VL-Embedding-8B inference is a committed component of v1, not a contingency.** Measurement informs *tuning* — batch size, dimension, thresholds, how much weight the semantic leg carries in the fusion — never *whether the tier exists*.
</callout>
**Why it sits here rather than at the end.** The full-text path ships first because it is fast to build, not because it outranks this. This tier is numbered M4b rather than M5 because it is **parallel work, not downstream work**: the sanitizer, the job queue, the pull loop and the ingest boundary are all M1-era pieces, and none of them wait on facets, logo tagging or the backfill. Two people can build M3–M4 and M4b at the same time; one person alternates between them.
1. **Implement ****`IEmbeddingProvider`**** with the local CPU path first**, using the pinned checkpoint. Same weights on both sides of the search, always: **asymmetric compute, symmetric model**. A 2B query encoder against an 8B index is mechanically impossible, not merely worse.
2. **Measure prefill on the actual shop machine** before depending on the published benchmark numbers. This page has already been wrong once by trusting a number it did not verify. The measurement sets the query-path budget; it does not decide whether the tier ships.
3. **The spawned notebook is a pure, stateless embedding service** and implements only the batch half of the contract — claim work, embed, post vectors, ack, exit. It never holds a database credential, never joins the tailnet, never writes to the store directly, and never parses, renders or extracts anything. Everything else already happened upstream.
4. **It is a pull loop, not an HTTP endpoint.** A tunnelled inbound hostname is a development fixture only; it never becomes the production transport.
5. **The readiness handshake refuses on profile mismatch.** Model ID, revision, quantization, dimension and the instruction prefix pair (`"Query: "` / `"Document: "`) are all advertised and all checked. A mismatched prefix degrades retrieval silently and is the most likely way search quietly gets worse after a refactor.
6. **The notebook may serve the query side too, as an accelerator with a mandatory fallback.** Weights are pre-mounted so a warm session answers a query encode in well under the five-second budget, and encoding both sides on the same runtime is the strongest possible parity guarantee. But the session is not durable, so **`IEmbeddingProvider`**** probes it and falls back to local CPU** rather than failing. See the subsection below.
7. **Exact search, no ANN index.** At \~6,000 files an IVF-PQ index is a scale optimization with a recall cost and no benefit.
8. **Cross-runtime parity test on the gold set** before the semantic tier is enabled. If GPU-indexed and CPU-queried vectors do not agree, semantic search is marked **unavailable** rather than mixing incompatible vectors.
### Query-side embedding on the notebook: what it buys and what it costs
Pre-mounting the weights is the right move and it removes the largest cold-start cost — a \~9 GiB re-download on every spawn. It does not remove the rest of the cold start, and it does not make the session durable. Both matter because this is now on the **interactive** path, where the parent page's five-second ceiling applies.
<table header-row="true">
<tr>
<td>What is true</td>
<td>Consequence for the design</td>
</tr>
<tr>
<td>**A warm, attached session encodes a short query in well under a second.** The model is already resident in GPU memory.</td>
<td>While a session is live, this is the **fastest and most accurate** query path available. Use it.</td>
</tr>
<tr>
<td>**Idle timeout is \~90 minutes; absolute session lifetime is 12 hours on the free tier.** Neither is negotiable and neither is a bug.</td>
<td>The session **will** be gone during a normal working day. Search must not degrade when it is.</td>
</tr>
<tr>
<td>**Colab has no execution API.** A session is started by a human opening a notebook.</td>
<td>Nothing in the backend can *cause* the query encoder to exist. It can only detect that it does.</td>
</tr>
<tr>
<td>**Mounted weights are read over Drive, which is slow for large files.** Drive is fine as a cache that avoids re-download; it is not fast local disk.</td>
<td>Copy the weights from the mount to the local runtime disk once at startup, then load from there. Measure this once — it is startup cost, paid per session.</td>
</tr>
<tr>
<td>**A tunnel hostname stays stable while the process behind it does not.**</td>
<td>Reachability is not readiness. The provider checks the **profile handshake**, not whether the port answers.</td>
</tr>
</table>
**So the rule is: opportunistic remote, guaranteed local.**
- `IEmbeddingProvider` gains a **remote query implementation** alongside the local CPU one. It is selected per request, not per deployment, by a cheap liveness-and-profile probe with a hard timeout of a few hundred milliseconds.
- **The probe result is cached with a short TTL** so a dead session is not re-probed on every keystroke, and the fallback does not itself cost the budget.
- **Profile mismatch means fall back, not proceed.** If the notebook advertises a different model ID, revision, quantization, dimension or instruction prefix pair than the index was built with, it is treated as unavailable. A vector from the wrong profile is worse than no vector.
- **The local CPU path is never optional and never removed.** It is what makes search work at 9 a.m. before anyone has opened a notebook, and it is what the acceptance test below is run against.
- **Query text is sanitized locally before it leaves the machine**, by the same `IInputSanitizer` stage as indexing, with the `"Query: "` prefix applied on our side rather than in the notebook. The notebook stays a pure encoder: bytes in, vector out, nothing stored, nothing parsed, nothing logged.
- **Everything after the vector stays ours.** Filtering, fusion, ranking and result presentation run in our modules against LanceDB on the shop machine. The notebook never sees the corpus, the tenant boundary, or a database credential.
- **Warmth is surfaced, not hidden.** The overlay shows which tier answered. A search that is quietly worse because a session expired is the failure mode this whole subsection exists to prevent.
**Acceptance:** the notebook batch path embeds the corpus, the local CPU path embeds a live query, the parity test passes, and the fused result set (FTS + vector, RRF) is at least as good as FTS alone on every gold-set category. **Additionally**, a query with no recoverable text — "blue wedding card, gold border" — finds a file that the text path provably could not.
**Deferred from this milestone, and only these:** the reranker, and the Modal throughput upgrade. Both wait on the first paying customer, as agreed.
---
## M5 — The backfill
**Size:** large in elapsed time, small in code. **Depends on:** M2, M3.
This is a scheduled operational event, not a feature, and it is the first honest test of everything above.
- **Resumable, idempotent, interruptible** — all three already guaranteed by M1's content-hash job identity.
- **Ordered by value, not by directory.** Newest first, then the date windows around the recurring events in the lexicon. The archive becomes useful long before the run finishes.
- **Runs for days rather than hours**, throttled during shop hours, with visible progress and a per-stage failure report.
- **A dry run over 500 files first**, to convert M0's encode-time estimate into an actual completion date before committing to the schedule.
**Acceptance:** the full corpus is indexed; the failure report is categorized and small; and the benchmark query of record — *"the .psd of International Women's Day where Hands Nepal was a supporter"* — is attempted and its outcome recorded honestly, including if it fails.
---
## M6 — Facets, structure and dates
**Size:** medium. **Depends on:** M3, M4.
The query is not one question but three: **event**, **organization**, **role**. This milestone makes each a cheap filter.
1. **A rule-based facet parser** — not a language model. Organizations resolve against the existing party table including aliases and romanized spellings; roles against a keyword list (supporter, sponsor, organizer, partner, सहयोगी, आयोजक); events against a maintained lexicon.
2. **The recurring-event lexicon is product data**, roughly 50 entries, each mapping Nepali and English surface forms **to a date window**. A print shop's calendar repeats annually, which makes this the highest-leverage lookup table in the product — and the date window alone discards the large majority of the archive for free.
3. **Role becomes structure, stored at index time.** Each recognized organization is indexed with *where* it appeared — masthead, body, or sponsor strip — derived from layer group names when they exist and from bbox position in the bottom band when they do not.
4. **Parsed facets render as editable, removable chips.** When a search returns nothing, the user drops a chip instead of retyping a sentence and guessing what went wrong. Suggest, human confirms — the same pattern as everywhere else in the product.
**Acceptance:** the benchmark query parses into `PSD` · `Feb–Mar` · `Women's Day` · `Hands Nepal` · `supporter`, each chip is independently removable, and removing the role chip visibly widens the result set.
---
## M7 — Logo entity tagging
**Size:** medium. **Depends on:** M2, M3, M6. **Gate:** M0's recognizable-logo share must justify it.
1. **Cluster recurring marks across the archive automatically**, then ask a human to name each cluster once. Never ask anyone to crop 500 logos by hand — invert it: *"this mark appears in 340 files — whose is it?"* and 340 files are tagged in one action. Ordered by frequency, so the most valuable marks come first.
2. **Keypoint matching, not embeddings.** A logo inside a design is the *identical* artwork, placed and scaled — that is instance retrieval, and it is ORB/SIFT plus **MAGSAC++ geometric verification**, on CPU, with an interpretable inlier count as the confidence score. The accept threshold is fitted on the gold set, not guessed.
3. **Match at full render resolution**, never on the 1024 px search render. A sponsor-strip mark is a few dozen pixels tall after downsampling and has no keypoints left.
4. **A confident match writes the organization's name into the ordinary indexed text field.** This is the whole trick: a visual problem is converted into a text problem at index time, and the query path stays pure full-text — fast, exact, offline, identical for logo-derived and OCR-derived and layer-derived text.
5. **Precision over recall, without exception.** A missed tag makes a file harder to find; a wrong tag files one customer's design under another's, which in a white-labeled multi-tenant product is a confidentiality incident. Tags are suggestions until confirmed, visually distinct, never crossing a tenant boundary, and never mutating a business record.
**Acceptance:** one confirmed Hands Nepal cluster retroactively tags every Hands Nepal file in the archive, and the benchmark query resolves the organization facet from the logo alone on a file with no matching text.
---
## Cross-cutting: the gold set
This is built during M3 and maintained forever after. It is not a milestone because it is never finished.
- **50–100 real queries in the shop's own words**, collected from staff rather than invented. Nepali, romanized Nepali, and English.
- **Each with its known-correct file or files**, established by hand once.
- **Categories kept separate** so averages cannot hide failures: proper-noun exact match, organization-by-logo, date range, compound faceted queries modelled on the benchmark query, and text-free visual queries.
- **Recall@10 and Recall@50 tracked separately.** Good Recall@50 with weak Recall@10 is the specific signature of a reranking problem; poor Recall@50 means the fix is upstream in rendering, extraction or embedding, and no reranker can help.
- **It gates every change** to a model, revision, quantization, dimension, instruction prefix, OCR engine, tokenizer or matching threshold.
---
## Ordering summary
<table header-row="true">
<tr>
<td>Milestone</td>
<td>Ends with</td>
<td>Needs money?</td>
<td>Needs internet?</td>
</tr>
<tr>
<td>M0 Measure</td>
<td>Nine numbers that close four open risks</td>
<td>No</td>
<td>No</td>
</tr>
<tr>
<td>M1 Skeleton</td>
<td>A resumable pipeline that does nothing yet</td>
<td>No</td>
<td>No</td>
</tr>
<tr>
<td>M2 Render</td>
<td>Every file has AVIF derivatives</td>
<td>No</td>
<td>No</td>
</tr>
<tr>
<td>M3 Text + FTS</td>
<td>**Search works.** Layer text, OCR, Devanagari handling</td>
<td>No</td>
<td>No</td>
</tr>
<tr>
<td>M4 API + overlay</td>
<td>Staff find files from Explorer</td>
<td>No</td>
<td>No</td>
</tr>
<tr>
<td>M4b Embedding tier</td>
<td>**Committed.** Semantic recall fused with text search</td>
<td>No — CPU queries, free notebook GPU batches</td>
<td>Only for batch indexing</td>
</tr>
<tr>
<td>M5 Backfill</td>
<td>The legacy archive is searchable</td>
<td>No</td>
<td>No</td>
</tr>
<tr>
<td>M6 Facets</td>
<td>The benchmark query parses and filters</td>
<td>No</td>
<td>No</td>
</tr>
<tr>
<td>M7 Logo tagging</td>
<td>Organizations findable without text</td>
<td>No</td>
<td>No</td>
</tr>
</table>
---
## What is explicitly not in this plan
- **Any reranker.** Retrieval returns 50 and shows 10 purely so the option stays open. Deferred to the first paying customer, alongside Modal — not cancelled.
- **Not on this list: the embedding tier.** It is M4b and it is committed. Nothing in this section defers it.
- **An ANN index.** Not at this corpus size.
- **Hosted vector storage or a hosted relational database.** Both contradict the local-first, no-static-IP topology.
- **A shell extension.** The global shortcut ships first and may be sufficient.
- **Modernizing Oat++ internals.** Patched only for compilation breakage, bugs or security fixes.
- **Static Qt linking.** Blocked on a purchased commercial licence, which is itself a hard gate on external distribution.
- **Any dependency on concealing workloads from the provider hosting them.** Permanently excluded.
