# SquiFlow Workstation — Codebase Production

Source page id: 0d412d670a614d599acdf1610ff23a36

---

<callout icon="🧱">
	**This is the working root for building the workstation.** One child page per directory, mirroring the tree exactly. This page answers the question that comes before any of them: **how the executable is actually produced, and what is linked how.**
</callout>
## What is linked which way, and why
The decision is not one decision. Four kinds of code end up in the deliverable and each is treated differently.
<table header-row="true">
<tr>
<td>What</td>
<td>How it is linked</td>
<td>Why it must be that way</td>
</tr>
<tr>
<td>**Our own modules** — engine, workflows, the twelve modules, shell</td>
<td>**Static libraries**, all folded into one executable</td>
<td>Whole-program optimisation, dead code eliminated, no load-time symbol resolution, nothing loose on disk. This is step 1 of the structure plan</td>
</tr>
<tr>
<td>**Qt**</td>
<td>**Dynamic — DLLs shipped beside the executable**</td>
<td>**This is a licensing requirement, not a preference.** Dynamic linking is what allows a closed-source application to use Qt under LGPLv3 without publishing object files or a relink kit. The DLLs must remain replaceable by the user, unmodified, with the licence texts included</td>
</tr>
<tr>
<td>**Third-party C++ libraries** — SQLite, the MessagePack codec, compression, and similar</td>
<td>**Static**</td>
<td>Small, permissively licensed, and no reason to be separate files. SQLite in particular is compiled directly into the binary with the compile-time options we choose</td>
</tr>
<tr>
<td>**The Microsoft C++ runtime**</td>
<td>**Dynamic**, with the runtime DLLs placed **beside the executable**</td>
<td>Forced: Qt's official binaries are built against the dynamic runtime, and mixing a static runtime with them is not permitted. Placing the runtime files app-local rather than installing a redistributable keeps the install elevation-free, which the update flow depends on. *Verify the exact redistributable terms for app-local deployment before release*</td>
</tr>
</table>
### Why Qt is not linked statically, stated bluntly
A static Qt build would produce one file and a smaller download. It is rejected for three reasons, in descending order of seriousness:
1. **LGPLv3 compliance becomes an obligation instead of a non-issue.** Static linking requires giving recipients the means to relink the application against a modified Qt — object files or an equivalent. Dynamic linking discharges that obligation simply by shipping replaceable DLLs.
2. **A static Qt has to be built, and building Qt is a multi-hour job** that would sit inside the release pipeline forever.
3. **It would push the project towards a commercial licence** — the exposure the plan already deliberately avoids for version one.
---
## Producing the executable, step by step
This is what the release workflow does, in order. Nothing in it happens on a developer machine as a prerequisite.
1. **Configure** with the named CMake preset and the vcpkg toolchain. Third-party dependencies use the **static-library, dynamic-runtime** triplet, so they are folded in while still agreeing with Qt about the runtime.
2. **Qt comes from a pinned official binary install**, not built from source. Version pinned exactly; the same version in CI and on any developer machine.
3. **Compile Release** with optimisation, link-time optimisation, identical-code folding, and the hardening flags from `cmake/Hardening.cmake`. Debug information is generated but **shipped separately, never to the shop**.
4. **QML is compiled** into the binary by the Qt build integration, so no `.qml` file exists in the output at all.
5. **Collect the Qt runtime** with Qt's deployment tool, which copies the required DLLs and plugins into the staging folder.
6. **Prune what the deployment tool over-collects** — translations we do not use, plugins for features we do not have. **Pruning is verified by launching the pruned build in CI**, because an over-eager prune fails at run time on the shop machine and nowhere else.
7. **Add the runtime DLLs** app-local.
8. **Sign** our own executables — the application and the updater — with the automated certificate.
9. **Write the manifest**: version, file list, a hash for every file, and the size of the folder. **Sign the manifest.** This is what makes an update verifiable even though the Qt DLLs themselves carry no signature of ours.
10. **Package** the version folder and publish it to the private release.
### The size budget is measured, not estimated
CI records the byte size of the staged folder on every build and **fails if it crosses a recorded ceiling**. No figure is written here in advance, because a made-up number is worse than no number — the first real build sets the baseline and every later build is compared against it.
---
## Two things to decide before the first build
<table header-row="true">
<tr>
<td>Question</td>
<td>The choice, and what hangs on it</td>
</tr>
<tr>
<td>**How documents get printed**</td>
<td>Qt's print-support module pulls in the whole widgets library, which the interface otherwise never uses — a permanent cost in binary size and memory for a feature used a few times a day. The alternative is to render the invoice or receipt to PDF using the PDF writer in Qt's GUI module, then hand it to the Windows printing interface directly, and never link widgets at all. **The second is preferred and must be proven in a spike before the interface is written**, because retrofitting it later touches every printed document</td>
</tr>
<tr>
<td>**Whether AVIF can be displayed at all**</td>
<td>Qt ships no AVIF support the way it ships PNG and JPEG. A plugin plus its library must be added and verified for the pinned Qt version. Thumbnails stay in a format Qt handles natively precisely so that lists never depend on this working</td>
</tr>
</table>
---
## What the deliverable folder contains
```plain text
SquiFlow/versions/<version>/
  squiflow.exe                 our modules, statically folded in
  squiflow-updater.exe         tiny, separate, so it can replace the app
  Qt6Core.dll  Qt6Gui.dll  Qt6Qml.dll  Qt6Quick.dll  ...
  plugins/platforms/           the Windows platform plugin
  plugins/imageformats/        only the formats actually used
  plugins/tls/                 the TLS backend
  msvcp140.dll  vcruntime140.dll  ...   app-local runtime
  licenses/                    Qt and third-party licence texts
  manifest.json                file list, hashes, version
  manifest.sig                 signature over the manifest
```
Every Qt DLL lives **inside the version folder**, not in a shared location. That is what allows two versions to sit side by side during an update and what makes rollback a matter of pointing at the other folder. It also means nothing is ever written to a system directory, which is the whole trace budget in one sentence.
## The directory pages
One page per directory follows, in build order. Each carries the files it contains, the contract for each file, what that directory is forbidden to depend on, and the condition for calling it finished.
<page url="https://app.notion.com/p/738a5beb9ee842a9a4b73e83876f4420">cmake/ — build logic</page>
<page url="https://app.notion.com/p/5a9fc6243f9f4a218d677db3749cee82">src/engine/ — shared mechanisms</page>
<page url="https://app.notion.com/p/3bc5585b746040f0904bc68cd59901aa">src/workflows/ — the only cross-module layer</page>
<page url="https://app.notion.com/p/9fdc6674009546508cb0d173a86bf9ce">src/app/ — startup and composition</page>
<page url="https://app.notion.com/p/8e3c108749604bbfbafcf20673539cf5">src/platform/ — the Windows boundary</page>
<page url="https://app.notion.com/p/fd6a62ab8b54480b818333006e6fc1c8">external/ — submodules</page>
<page url="https://app.notion.com/p/d2b2cd6ae17247888b271328546883ba">src/ui/ — QML, compiled into the binary</page>
<page url="https://app.notion.com/p/4af4e249f8714486b2946f7ebd578c97">src/modules/ — the twelve</page>
<page url="https://app.notion.com/p/90f7cc5cab104f47a2ed0c10d1dabfac">packaging/ — producing the deliverable</page>
<page url="https://app.notion.com/p/3b709c1bfc1c4f198ef44525d6f1bc0d">docs/ — decisions that must survive</page>
<page url="https://app.notion.com/p/fd370b2447dd40b080f70542a96474a5">src/shell/ — the frame around everything</page>
<page url="https://app.notion.com/p/640282be8de647869b44553fe750d107">tests/ — what must be proven</page>
<page url="https://app.notion.com/p/6803da2420774321be16f06a7084423c">Implementation plan — phases, and what can actually be verified</page>
<page url="https://app.notion.com/p/0a9b76c109e14056a9ea8ff8fe279500">Phase 1 — Setup and the protocol spine (compiled, tests pass)</page>
<page url="https://app.notion.com/p/a597def902aa42bb9e873585da2c7bea">Phase 2 — Engine domain: money, lifecycle, numbering, capability (93 checks pass)</page>
<page url="https://app.notion.com/p/04cc1a90df4f4ef6b9073f1becb20ad4">Header reference — the protocol surface everything builds against</page>
