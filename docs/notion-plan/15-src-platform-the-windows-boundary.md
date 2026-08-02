# src/platform/ — the Windows boundary

Source page id: 8e3c108749604bbfbafcf20673539cf5

---

<callout icon="🪟">
	**Purpose.** Confine every Windows-specific call to a handful of files. Not for portability — for compile speed, for testability, and to keep platform limits documented where they are worked around.
</callout>
## Files
<table header-row="true">
<tr>
<td>Interface</td>
<td>Implementation</td>
<td>Contract</td>
</tr>
<tr>
<td>`paths.hpp`</td>
<td>`paths_win.cpp`</td>
<td>Data, cache, logs, secrets. **Data goes to the machine-wide program-data location, not the per-account one** — a different Windows account must not make the shop's records appear to vanish</td>
</tr>
<tr>
<td>`secrets.hpp`</td>
<td>`secrets_dpapi.cpp`</td>
<td>Store and retrieve credentials through the Windows data-protection interface. Nothing in plaintext, ever</td>
</tr>
<tr>
<td>`network_state.hpp`</td>
<td>`network_state_win.cpp`</td>
<td>Online, metered, offline — **from the operating system's notification, never by pinging the server**</td>
</tr>
<tr>
<td>`power.hpp`</td>
<td>`power_win.cpp`</td>
<td>Sleep and wake events. Wake triggers a staggered restart of services, not a stampede</td>
</tr>
<tr>
<td>`updater.hpp`</td>
<td>`updater_win.cpp`</td>
<td>Launch the updater and exit. Documents the reason it exists: **a running executable cannot replace itself**</td>
</tr>
<tr>
<td>`printing.hpp`</td>
<td>`printing_win.cpp`</td>
<td>Enumerate printers and send a rendered document. Deliberately not Qt's print module, so the widgets library is never linked — **to be proven by a spike first**</td>
</tr>
<tr>
<td>`idle.hpp`</td>
<td>`idle_win.cpp`</td>
<td>Time since last user input, for the maintenance gate</td>
</tr>
<tr>
<td>—</td>
<td>`testing/`</td>
<td>A fake for each interface. Without these, none of the sync or service behaviour is testable</td>
</tr>
</table>
## Rules
- **No Windows header is included outside this directory.** Its macros collide with ordinary names and slow every compile that sees them.
- Every interface has a fake before it has a second caller.
- Where an interface exists because Windows forces it, the reason is a comment in the implementation — that is the only place a future reader will look.
## Done when
Every interface has a working implementation and a fake, and the rest of the codebase compiles with no knowledge that Windows exists.
