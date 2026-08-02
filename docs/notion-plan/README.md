# SquiFlow planning checkpoint — exported from Notion

This folder is a snapshot of every SquiFlow planning page from the Notion workspace, exported as Markdown so the planning record travels with the codebase instead of living only online. It is a checkpoint, not a replacement for Notion — if the Notion pages change later, this folder will be stale until re-exported.

## Contents, in reading order

| File | Page |
| --- | --- |
| 00-codebase-production-root.md | SquiFlow Workstation - Codebase Production (root index of the pages below) |
| 01-workstation-and-server-file-structure.md | Workstation & Server File Structure |
| 02-codebase-structure-modules-updates-build-pipeline.md | Codebase Structure, Modules, Updates & Build Pipeline |
| 03-runtime-architecture-sync-and-delivery.md | Runtime Architecture, Sync & Delivery |
| 04-feature-set-and-usage-specification.md | Feature Set & Usage Specification |
| 05-full-implementation-plan.md | Full Implementation Plan |
| 06-implementation-plan-phases-and-verification.md | Implementation plan - phases, and what can actually be verified |
| 07-phase-1-protocol-spine.md | Phase 1 - Setup and the protocol spine |
| 08-phase-2-engine-domain.md | Phase 2 - Engine domain |
| 09-design-file-search-execution-plan.md | Design-File Search: Execution Plan |
| 10-quoting-and-quote-to-order-full-plan.md | Quoting & Quote-to-Order: Full Plan |
| 11-src-modules-the-twelve.md | src/modules/ - the twelve |
| 12-cmake-build-logic.md | cmake/ - build logic |
| 13-external-submodules.md | external/ - submodules |
| 14-src-app-startup-and-composition.md | src/app/ - startup and composition |
| 15-src-platform-the-windows-boundary.md | src/platform/ - the Windows boundary |
| 16-src-engine-shared-mechanisms.md | src/engine/ - shared mechanisms |
| 17-src-workflows-the-only-cross-module-layer.md | src/workflows/ - the only cross-module layer |
| 18-src-ui-qml-compiled-into-the-binary.md | src/ui/ - QML, compiled into the binary |
| 19-src-shell-the-frame-around-everything.md | src/shell/ - the frame around everything |
| 20-tests-what-must-be-proven.md | tests/ - what must be proven |
| 21-packaging-producing-the-deliverable.md | packaging/ - producing the deliverable |
| 22-docs-decisions-that-must-survive.md | docs/ - decisions that must survive |
| 23-header-reference-protocol-surface.md | Header reference - the protocol surface everything builds against |

Each file carries the source Notion page id in its header so it can be matched back to the live page.

This is separate from `docs/plan/` in this repository, which is the working build plan (phases, sub-phases, status) written directly against this codebase rather than exported from Notion.
