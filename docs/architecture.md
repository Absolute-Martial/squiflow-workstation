# Architecture in one page

## Layers, and the direction dependencies run

    app        startup, composition, activation      knows everything
    shell      window, navigation, search            knows the registry only
    workflows  cross-module sequences                knows several modules
    modules    the twelve                            knows engine + declared deps
    engine     storage, records, sync, identity      knows platform + protocol
    platform   the Windows boundary                  knows Windows only
    protocol   modules, rights, operations, wire     knows nothing

Dependencies point downward. Never upward, never sideways.

## Three rules that must never bend

1. **A module never includes another module's header.** Crossing modules is what
   `src/workflows/` is for. Enforced by a test that reads the source tree.
2. **Core is closed under dependency.** A core module may never require an extra,
   because switching that extra off would break something unswitchable. Enforced
   at configure time and again by the protocol test.
3. **Rights, operations and modules are enumerations, not strings.** A typo
   becomes a compile error. Nothing looks up a right by name at run time except
   when decoding a payload from the network, where an unknown name is rejected.

## Where a change goes

| Change                                   | Where it goes                        |
|------------------------------------------|--------------------------------------|
| A new thing the shop can do              | an operation line, plus a service    |
| A new permission                         | `rights.def`, plus grants            |
| A sequence crossing modules              | `src/workflows/`                     |
| A new business entity                    | a module, if it passes the tests     |
| Something several modules borrow         | `src/engine/`, as a mechanism        |
| A Windows call                           | `src/platform/`, behind an interface |
