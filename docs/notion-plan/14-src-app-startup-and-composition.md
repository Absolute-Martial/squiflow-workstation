# src/app/ — startup and composition

Source page id: 9fdc6674009546508cb0d173a86bf9ce

---

<callout icon="🚀">
	**Purpose.** The only place allowed to know that every other part exists. Everything else depends downward.
</callout>
## Files
<table header-row="true">
<tr>
<td>File</td>
<td>Contract</td>
</tr>
<tr>
<td>`main.cpp`</td>
<td>Construct, run, return. If it grows past about twenty lines something has been put in the wrong place</td>
</tr>
<tr>
<td>`application.*`</td>
<td>Owns startup order and shutdown order, both written explicitly rather than implied by construction order</td>
</tr>
<tr>
<td>`composition_root.cpp`</td>
<td>Names every module and calls its register function. **The only file in the codebase that includes every module.** Deleting a module must be one line here plus one directory</td>
</tr>
<tr>
<td>`module_registry.*`</td>
<td>What modules register into: screens, palette entries, rights, attention rules, sync handlers. Knows no module by name</td>
</tr>
<tr>
<td>`activation.*`</td>
<td>Reads the core/extra activation settings, computes the transitive closure, and hides inactive modules. Refuses an activation state that is internally inconsistent</td>
</tr>
<tr>
<td>`single_instance.*`</td>
<td>A named lock. A second launch raises the existing window and exits — two processes on one database is the failure this prevents</td>
</tr>
<tr>
<td>`crash_handler.*`</td>
<td>Minidump to the data folder, breadcrumb trail, offer to restart. Installed early, because nothing before it is protected</td>
</tr>
<tr>
<td>`logging.*`</td>
<td>Levels, rotation, and the **hard total size cap**. Owns the trace budget</td>
</tr>
<tr>
<td>`generated/version.hpp`</td>
<td>Written by the build from the git tag. Never edited by hand</td>
</tr>
</table>
## Startup order
1. Paths → 2. Logging → 3. Crash handler → 4. Single-instance lock → 5. Database open → 6. Migrations → 7. Integrity check → 8. Identity and session → 9. Activation → 10. Module registration → 11. Shell → 12. Window.
Nothing that touches the database may be constructed before step 6. Nothing that can crash usefully should run before step 3.
## The trap
Self-registering modules inside static libraries are **silently dropped by the linker**. That is why registration is an explicit list here rather than a clever global constructor. A completeness test compares the list against the module directories.
## Done when
The application starts to an empty window, writes a capped log, survives a second launch attempt, and produces a readable dump from a deliberately triggered crash.
