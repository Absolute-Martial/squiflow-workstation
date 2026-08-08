# Qt Image Formats 6.11.1 source pin

- Upstream: https://code.qt.io/cgit/qt/qtimageformats.git/
- Supplied archive: `qtimageformats-everywhere-src-6.11.1.tar.xz`
- Version: 6.11.1
- Archive SHA-256: `b2bf6c6845ac175ed7f819145483ba4676f617aaa6a5012c8efee63c8bbac413`
- Licenses: see `LICENSES/`, `REUSE.toml`, and individual codec notices.
- Purpose: offline source/provenance and qualification input for workstation
  image-format plugins against the exact Qt 6.11.1 kit.

The application does not add this tree as a subdirectory. CI installs the
matching prebuilt Qt module; an offline qualification lane may build this source
against the same Qt prefix. No AVIF capability is claimed merely because the
source exists—the produced plugin and fixtures must pass the Phase 7.10 gate.
