# src/ui/ — QML, compiled into the binary

Source page id: d2b2cd6ae17247888b271328546883ba

---

<callout icon="🎨">
	**Purpose.** The visual layer. **Compiled into the executable, never shipped as loose ****`.qml`**** files** — faster to start, nothing to tamper with, nothing to accidentally leave behind on disk.
</callout>
## Layout
```plain text
src/ui/
  Theme/            colours, spacing, type scale, density
  Controls/         buttons, fields, pickers — the vocabulary
  Patterns/         list-with-filters, record page, wizard step, empty state
  Dialogs/          confirm, conflict resolution, signature capture
  Branding/         swappable by configuration, never by fork
  Icons/            one compiled resource
```
Module screens live in each module's `view/`, not here. This directory is the vocabulary; modules write sentences with it.
## Performance rules, on an integrated-graphics machine with a spinning disk
- **No JavaScript in a binding that runs during scrolling.** Bindings run far more often than anyone expects.
- Lists are reused and windowed. A list that instantiates every row is a list that stalls.
- **No image is decoded on the interface thread.** Thumbnails are pre-sized and cached at the size shown.
- Effects, shadows and blurs are avoided — they cost more than they are worth without a discrete graphics card.
- A screen's construction is measured; anything slow is built after the first frame, not before it.
## Branding
A branding package is **data**: colours, logo, document headers. The white-label rule holds absolutely — **configuration, never a fork, and never a condition on who the tenant is** anywhere in the source.
## Done when
The interface starts to a usable window quickly on the real machine, scrolls a long list without a stall, and contains no loose QML in the shipped folder.
