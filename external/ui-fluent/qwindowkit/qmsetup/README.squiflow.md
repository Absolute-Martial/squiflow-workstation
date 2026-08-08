# qmsetup (vendored for QWindowKit)

- Source: https://github.com/stdware/qmsetup (user-supplied snapshot)
- License: MIT (`LICENSE`)
- Purpose: host-build CMake helpers required by QWindowKit.
- Nested dependency: `src/syscmdline` is vendored from the supplied archive at
  commit `0c9f3de8b11bd2f33b03bea5521bf446af4ead69`; see its source pin and
  `README.squiflow.md`.

qmsetup and syscmdline are build-time dependencies only and are not workstation
plugins or business-logic dependencies.
