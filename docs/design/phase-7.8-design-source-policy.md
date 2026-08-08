# Phase 7.8 design source policy

## Runtime source of truth

SquiFlow uses Qt 6.11.1 Quick Controls with `FluentWinUI3` as the base desktop
style and the supplied FluentControls `Fluent.qml` singleton as the runtime
semantic-token source. First-party QML consumes those values through
`SquiFlow.Theme`; module pages never bind directly to a vendor singleton.

Priority is fixed:

1. Qt Quick Controls / FluentWinUI3 for native controls and interaction;
2. supplied FluentControls for missing Fluent patterns;
3. supplied FluentUI only for a unique approved capability that justifies its
   Widgets/PrintSupport dependencies;
4. supplied Rin-UI only as a qualified gap-fill;
5. a small SquiFlow component only when none of the above provides the required
   application-specific semantic behavior.

No first-party QML file imports two extra component libraries. Vendor sources
are not edited; compatibility is implemented in SquiFlow-owned wrappers.

## Supplied reference kits

### Material 3 Design Kit (Community)

- Supplied `.fig` SHA-256:
  `8dd30e1099f7cf067f2e14279d6d5a4dd159d2f2a96a6a5534b6788b7e86fff0`
- Figma export metadata timestamp: `2026-08-06T12:16:29.177Z`.
- Verified canvas content includes M3 light/dark semantic surface and primary
  roles, Roboto type roles, state overlays, and component references.
- Used as a reference for semantic role naming, 4-point spacing rhythm, state
  coverage, concise feedback, and purposeful 100–300 ms motion. Its palette is
  not copied over FluentWinUI3.

### macOS 27 (Community)

- Supplied `.fig` SHA-256:
  `0f86f22d158ff22cea4d95459ef50d959fa1d31cf80837dff242e246638ed604`
- Figma export metadata timestamp: `2026-08-06T12:17:00.079Z`.
- Verified canvas content includes system/tint colors, label hierarchy, SF Pro
  text roles, and desktop window/sidebar references.
- Used as a reference for restrained hierarchy, native desktop density,
  keyboard/menu behavior, and platform fallback typography on macOS.

The original large `.fig` archives are not committed to the source repository.
Their hashes and extracted previews preserve provenance without adding roughly
138 MB of opaque binary history. The local supplied files remain qualification
inputs for this implementation run.

## Adaptation rules

- Fluent values win when a reference kit disagrees with Windows desktop style.
- Material/macOS references may influence hierarchy, spacing, state coverage,
  motion, and accessibility, but do not create parallel themes.
- Typography uses point-sized SquiFlow roles and platform-appropriate family
  fallbacks; no module page chooses a raw pixel size.
- Motion must be state-related and disabled when reduced motion is selected.
- Business state, authorization, exact money, storage, and validation never
  move into visual components.
