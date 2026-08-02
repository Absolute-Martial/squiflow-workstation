# external/ — submodules

Source page id: fd6a62ab8b54480b818333006e6fc1c8

---

<callout icon="🔗">
	**Purpose.** Code we depend on that is ours but lives elsewhere. Submodules only — never copied source, because copied source has no version and therefore never gets updated.
</callout>
## Contents
<table header-row="true">
<tr>
<td>Entry</td>
<td>What it is</td>
</tr>
<tr>
<td>`protocol/`</td>
<td>Submodule pointing at `squiflow-protocol`. Contains the message definitions, the generated C++ types, **and the ****`operations.def`**** files for every module** — moved here so the workstation and the server cannot disagree about what a right is</td>
</tr>
</table>
Everything else — Qt, SQLite, the MessagePack codec — arrives through the dependency manager, not through this directory.
## Rules
- **Pinned to a commit, always.** A submodule tracking a branch is a build that changes without anyone changing it.
- **The server pins the same commit.** A protocol change is one commit, adopted by both sides deliberately.
- The build fails if the submodule is missing or dirty, rather than silently building against stale generated types.
- Private repositories mean CI needs an installation token to check this out. The default workflow token cannot do it — this is a known step, not a surprise.
## Done when
A clean clone with submodules builds, and a protocol change made in one commit visibly breaks the workstation build until it is adopted.
