# Changelog

All notable changes to SquiFlow are recorded here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/), and the project uses
[Semantic Versioning](https://semver.org/spec/v2.0.0.html). Until the first
Windows release the version stays at `0.1.0` and everything lands under
"Unreleased"; each entry names the sub-phase that produced it and the quality
gate that proved it.

## [Unreleased]

### Added

- **Phase 6.1 - application paths.** Records, logs, backups, crash dumps and
  the secrets store now resolve to one machine-wide location shared by every
  Windows account; only the cache is per account. Names and roots are validated
  before use, every directory is created and then proven writable by an actual
  write, and an unusable location stops startup with a plain explanation rather
  than failing at the first invoice. An unusable cache degrades to a warning
  and a fallback instead of a refusal. Evidence:
  `docs/qa/phase-6.1-paths-gate.md`.
- **Engineering code of conduct** (`docs/engineering/code-of-conduct.md`) and
  the first five architecture decision records (`docs/adr/`), including the
  single error-propagation policy, Qt as infrastructure only, the machine-wide
  data root, and the filesystem probe seam.
- **`.clang-format` and `.clang-tidy`** for the Windows development lane and
  CI. Neither tool is installed on the verification machine, and the quality
  gates say so rather than claiming a pass.
- **Phase 5.8 - remote document delivery.** Optional outbound delivery of an
  issued invoice, an exact issued quotation revision, or an agreement. The
  workstation never sends anything itself: preparation freezes the content and
  its hash offline, and the remote backend performs the send only when the
  machine is online and a person confirms. Transport acceptance is never
  approval. Evidence: `docs/qa/phase-5.8-remote-document-delivery-gate.md`.
- **Phase 5.1 to 5.7 - the workflow spine.** Quotation to order, order to
  jobs, invoice issue from a device-reserved number block, cancel and reissue,
  manual payment and allocation with optional tracking evidence, and agreement
  quantity caps consumed at issue and released at cancellation.
- **Phase 4.1 to 4.13 - the twelve modules.** Administration, parties,
  catalog, pricing, orders, receivables, jobs, quotations, agreements,
  sourcing, companion and files, each with its own migration and gate.
- **Phase 1 to 3 - the protocol spine, the domain engine and the storage
  half**, including the offline outbox, the sync cursor and conflict rules, and
  the strict verification harness.

### Changed

- The verification lane now builds a dedicated `squiflow::platform` library and
  registers `platform.platform_paths_test`, taking the independent CMake run to
  30 of 30 tests.

### Security

- Path input from the environment is treated as hostile: traversal, control
  characters, stray stream separators, reserved Windows device names and
  over-long roots are all refused before any directory is touched.

[Unreleased]: https://example.invalid/squiflow/compare/v0.1.0...HEAD
