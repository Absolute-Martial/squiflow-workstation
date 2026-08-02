# docs/ — decisions that must survive

Source page id: 3b709c1bfc1c4f198ef44525d6f1bc0d

---

<callout icon="📚">
	**Purpose.** Not a manual. The small set of documents whose absence would cost real time later — mostly records of *why*, which no amount of reading the code recovers.
</callout>
## Files
<table header-row="true">
<tr>
<td>File</td>
<td>Why it exists</td>
</tr>
<tr>
<td>`architecture.md`</td>
<td>One page: the layers, the direction dependencies run, and the three rules that must never bend</td>
</tr>
<tr>
<td>`decisions/`</td>
<td>One short record per decision, including the rejected alternative and the reason. **The rejected option is the valuable half** — without it the decision gets relitigated every year</td>
</tr>
<tr>
<td>`adding-a-module.md`</td>
<td>The generator command, the declaration, the one line in the composition root, the tier choice</td>
</tr>
<tr>
<td>`building.md`</td>
<td>From clean clone to running executable, including the private-submodule token step that will otherwise waste an hour</td>
</tr>
<tr>
<td>`releasing.md`</td>
<td>Tag, build, sign, publish, verify. Including how to revert a bad release</td>
</tr>
<tr>
<td>`operations.md`</td>
<td>What to do when the store is corrupt, sync is stuck, or the certificate must be rotated</td>
</tr>
<tr>
<td>`budgets.md`</td>
<td>The recorded numbers for memory, startup, footprint and traces, **measured on the real machine**, with the date measured</td>
</tr>
</table>
## Decisions already made that belong here as records
Static modules with Qt dynamic and why; runtime plugins rejected; QML compiled in; explicit registration rather than self-registration; single writer; server-assigned sync sequence; owner wins conflicts; **PostgreSQL as the system of record with the local store as a load-bearing cache**; approvals demoted from module to mechanism; pricing kept separate from catalog; orders and jobs kept separate; core and extra tiers; **no refunds — cancel and reissue only**.
## Rules
- **A decision record is written when the decision is made**, not at the end. Written later, it records what was built rather than why.
- Every measured number carries the date and the machine it came from.
- **Two open items must stay visible here until settled**: whether the stated Windows 7 minimum can hold against the Qt version in use, and whether printing can avoid the widgets library.
## Done when
Someone who has never seen the project can build, run and release it from this directory alone.
