# Phase 7.4-7.6 UI gate

Date: 2026-08-06

Portable implementation is complete and strict-gated. Phase 7.4 provides bounded forms, exact minor-unit parsing, pending/cancellation generations, and authoritative domain errors. Phase 7.5 provides immutable quotation/invoice/statement snapshots, three templates, and an atomic Qt Core/Gui `QPdfWriter` renderer without QtWidgets. Phase 7.6 provides a bounded stable-id + content-hash thumbnail cache, trusted-source image provider, visible fallback, QML preview, and a runtime AVIF capability check.

The Qt SDK is unavailable in this sandbox, so all three remain `[~]` until the registered sources compile and visual/runtime tests pass on Qt 6.11.1. QWindowKit also remains blocked by the empty `syscmdline` submodule inside the uploaded `qmsetup` zip.
