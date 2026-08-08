# syscmdline source pin

- Upstream: https://github.com/SineStriker/syscmdline
- Supplied archive: `syscmdline-main.zip`
- Archive commit: `0c9f3de8b11bd2f33b03bea5521bf446af4ead69`
- Archive SHA-256: `97f6bb4d0e7a6f28767d6dd5df9b09625f6892326ce1d683cedf17ce9f3d5481`
- License: MIT (`LICENSE`)
- Purpose: host-build command parser used by qmsetup's `qmcorecmd` while
  configuring QWindowKit.

The source is nested at the exact path expected by qmsetup. It is not linked to
SquiFlow runtime code, does not parse workstation arguments, and is not an
extension mechanism. Update the commit and hash together after a reviewed
upstream upgrade.
