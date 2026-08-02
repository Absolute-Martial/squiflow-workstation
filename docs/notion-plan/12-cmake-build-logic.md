# cmake/ — build logic

Source page id: 738a5beb9ee842a9a4b73e83876f4420

---

<callout icon="⚙️">
	**Purpose.** Every build decision lives here so that no individual `CMakeLists.txt` contains a flag. A rule added here applies to every target automatically — which is the only way a rule stays applied.
</callout>
## Files
<table header-row="true">
<tr>
<td>File</td>
<td>Contract</td>
</tr>
<tr>
<td>`SquiflowModule.cmake`</td>
<td>Declares a module: its sources, its **allowed dependencies**, its tier (core or extra), its required modules, and its test target. A module cannot grant itself a dependency anywhere else</td>
</tr>
<tr>
<td>`Warnings.cmake`</td>
<td>The warning set, warnings as errors in CI. One list, applied everywhere</td>
</tr>
<tr>
<td>`Hardening.cmake`</td>
<td>Control-flow guard, buffer security checks, standard-library assertions, runtime checks in debug builds</td>
</tr>
<tr>
<td>`Sanitizers.cmake`</td>
<td>Address and undefined-behaviour builds, used by a CI lane and by anyone chasing a bug</td>
</tr>
<tr>
<td>`Packaging.cmake`</td>
<td>Staging the version folder, running the Qt deployment tool, pruning, hashing, writing the manifest</td>
</tr>
<tr>
<td>`Version.cmake.in`</td>
<td>Turns the git tag into a generated header, the installer version and the manifest version. One source of truth</td>
</tr>
<tr>
<td>`ModuleGraph.cmake`</td>
<td>The checks that make the architecture real: **no core module may require an extra**, the requires graph is **acyclic**, and every registered module exists on disk</td>
</tr>
</table>
## Rules
- **No compile flag appears anywhere except in this directory.** A flag written into a target's own build file is a bug, because the next target will not have it.
- **The module graph checks fail the build, never warn.** A warning about a dependency cycle is a cycle that ships.
- Debug information is always produced and never shipped.
## Done when
A new module can be declared in three lines, the graph checks reject a deliberately introduced cycle, and a deliberately mis-tiered dependency fails the build.
