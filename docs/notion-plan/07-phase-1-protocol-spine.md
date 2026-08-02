# Phase 1 — Setup and the protocol spine (compiled, tests pass)

Source page id: 0a9b76c109e14056a9ea8ff8fe279500

---

<callout icon="✅">
	**43 files. Compiled with warnings as errors and executed. All checks pass.** This is the layer every other phase depends on, so it was built first and made self-checking.
</callout>
## Verified output
```plain text
g++ -std=c++20 -O1 -g -Wall -Wextra -Wpedantic -Wshadow -Wconversion
    -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Werror ...

module graph
activation closure
operation table

summary
  wire version   0.1
  modules        12
  rights         43
  operations     67
  usable offline 44 of 67
  core modules   6, extra 6

all checks passed
```
**44 of 67 operations work with no connection.** That number is not a target anyone set — it falls out of the rules, and it is now a fact the test suite will defend.
## What was built
<table header-row="true">
<tr>
<td>Area</td>
<td>Files</td>
<td>What it does</td>
</tr>
<tr>
<td>Build logic</td>
<td>`cmake/` × 7</td>
<td>Module declaration, warnings, hardening, sanitizers, version from the git tag, staging, and the graph checks that fail the build</td>
</tr>
<tr>
<td>Project setup</td>
<td>Root `CMakeLists.txt`, presets, dependency manifest, ignore files, README</td>
<td>Release, debug, sanitizer and **headless** configurations. The headless one builds everything except the interface — which is what makes most of this testable on any machine</td>
</tr>
<tr>
<td>Module graph</td>
<td>`modules.def`, `module_requires.def`, `module_graph.hpp/cpp`</td>
<td>Twelve modules with tiers, one dependency per line, cycle detection, core-closure check, and **activation closure**</td>
</tr>
<tr>
<td>Rights</td>
<td>`rights.def`, `right_id.hpp`</td>
<td>43 rights, each owned by a module. Granted per person; no roles</td>
</tr>
<tr>
<td>Operations</td>
<td>`operations.def`  • 13 per-module files, `operation_table.hpp/cpp`</td>
<td>67 operations, each with its right, its sync class and its offline rule</td>
</tr>
<tr>
<td>Tests</td>
<td>`protocol_test.cpp`</td>
<td>Dependency-free, so it runs anywhere</td>
</tr>
</table>
## The decisions this phase actually locks in
### Cross-references are enumerations, so mistakes are compile errors
An operation naming a right that does not exist **does not compile**. Neither does one naming a module that does not exist. There is no string lookup anywhere except when decoding a payload from the network, where an unknown name is rejected rather than guessed at.
### The rules check themselves
<table header-row="true">
<tr>
<td>Rule</td>
<td>How it fails</td>
</tr>
<tr>
<td>Core is closed under dependency</td>
<td>Configure-time error naming both modules, **and** a runtime test</td>
</tr>
<tr>
<td>The dependency graph is acyclic</td>
<td>Configure-time error printing the path; the test names the modules in the cycle</td>
</tr>
<tr>
<td>A module on disk is not registered</td>
<td>Configure-time error</td>
</tr>
<tr>
<td>Two operations share a name</td>
<td>Test failure printing the duplicate</td>
</tr>
<tr>
<td>An operation requiring the server is marked usable offline</td>
<td>Test failure naming the operation</td>
</tr>
<tr>
<td>The table drifts out of order with the enum</td>
<td>Test failure — otherwise every lookup would silently return the wrong operation</td>
</tr>
</table>
### Activation is computed, never chosen
Switching an extra off switches off everything that requires it, and returns the list of what else went dark so the person can be told before confirming. **Switching off a core module is refused, and the refusal names the module.** Both behaviours are tested.
### `jobs` deliberately requires nothing
A job may exist with no order at all, so `jobs` has **no** dependency on `orders`. The order-to-jobs link is a workflow, exactly as decided.
---
## Judgement calls made while writing this — tell me if any are wrong
<table header-row="true">
<tr>
<td>Call</td>
<td>Reasoning</td>
</tr>
<tr>
<td>**Issuing an invoice is a workflow, not a receivables operation**</td>
<td>It crosses into orders and pricing to snapshot lines and rates. Leaving it inside receivables would have been the first crack in the one-way dependency rule. Same for cancel-and-reissue</td>
</tr>
<tr>
<td>**Everything in administration is online-only**</td>
<td>A rights change made on a disconnected machine could contradict one made elsewhere, and the losing side is a person quietly keeping access they should not have</td>
</tr>
<tr>
<td>**Issuing a quotation is allowed offline**</td>
<td>It needs a number, and numbers come from this device's reserved block. Nothing about it needs the server to be reachable</td>
</tr>
<tr>
<td>**Closing an agreement is online-only**</td>
<td>It changes what every later invoice may charge. Two devices disagreeing about whether an agreement is closed is a pricing dispute with a customer</td>
</tr>
<tr>
<td>**Archiving anything is online-only**</td>
<td>Archiving offline while the other device is still using the record is a conflict with no good resolution</td>
</tr>
</table>
<callout icon="🔍">
	**Currently no extra module depends on another extra.** The machinery for it is built and tested, but the real dependency graph has not needed one yet. If a genuine case appears — job tickets needing design files, say — it is one line in `module_requires.def`.
</callout>
