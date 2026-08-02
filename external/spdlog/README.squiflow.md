# spdlog in SquiFlow

This is the header-only distribution of **spdlog 1.17.0**, copied without
modification from the user-supplied `spdlog-1.x.zip` on 2026-08-03.

Only `include/` and the upstream MIT `LICENSE` are vendored. Examples, tests,
benchmarks, build scripts and the original archive are deliberately excluded.
The archive itself stays in `third_party/provided-build-sources/`, which is
gitignored and never enters a checkpoint.

spdlog is used only inside `src/platform/`. No spdlog type appears in any
SquiFlow header, so nothing above the platform boundary depends on it.
SquiFlow keeps ownership of the three properties spdlog does not provide:
escaping so one record cannot forge a second line, credential-field redaction,
and a hard byte budget across the whole log family.

The distribution selects its own header-only mode; do not define
`SPDLOG_HEADER_ONLY` on the command line, because spdlog defines it internally
and a second definition is an error under `-Werror`.
