# src/shell/ — the frame around everything

Source page id: fd370b2447dd40b080f70542a96474a5

---

<callout icon="🧭">
	**Purpose.** The parts of the interface that belong to no module: the window, navigation, search, the connection indicator, the attention list. It knows the registry, never a module.
</callout>
## Files
<table header-row="true">
<tr>
<td>File</td>
<td>Contract</td>
</tr>
<tr>
<td>`shell_controller.*`</td>
<td>Which screen is showing, navigation history, deep links from search results and attention items</td>
</tr>
<tr>
<td>`navigation_model.*`</td>
<td>Builds the sidebar from **registered, active, permitted** screens. A module the person cannot use does not appear at all</td>
</tr>
<tr>
<td>`command_palette.*`</td>
<td>Keyboard-first action search across registered operations. On a counter machine this is faster than any menu</td>
</tr>
<tr>
<td>`search_controller.*`</td>
<td>One search box, results grouped by module, each module contributing its own provider</td>
</tr>
<tr>
<td>`attention_model.*`</td>
<td>The single list of things needing a person: sync conflicts, failed services, overdue accounts, expiring agreements</td>
</tr>
<tr>
<td>`connection_indicator.*`</td>
<td>Online, metered, weak, offline — always visible, because a person acting offline must know it</td>
</tr>
<tr>
<td>`notification_center.*`</td>
<td>Transient messages, with a rule: **never a modal for anything the person did not initiate**</td>
</tr>
<tr>
<td>`session_bar.*`</td>
<td>Who is signed in, on which device, and sign-out</td>
</tr>
</table>
## Rules
- **The shell may not include a module header.** Everything reaches it through the registry.
- Screens are lazily instantiated — a module that is never opened costs nothing beyond its registration record.
- Any screen must be reachable by keyboard alone.
## Done when
Adding a module makes it appear in navigation, search and the palette with no edit to any file in this directory.
