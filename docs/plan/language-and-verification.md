# Language and verification

## What "C++23" means in this sandbox

Both build lanes (the CMake configuration meant for a real machine, and the
plain Makefile that actually runs here) compile at `-std=c++23`. That flag is
necessary but not sufficient: a compiler can accept it and still ship only
part of the C++23 *library*.

Measured directly in this sandbox, not assumed:

```
compiler    g++ (GCC) 11.5.0
__cplusplus 202100
```

`202100` is the C++23 *draft* value GCC 11 reports, not `202302` (the final
published standard's value). In practice that means:

| Available | Not available |
| --- | --- |
| `std::span` | `std::expected` |
| `std::ranges` | `std::format` |
| `std::source_location` | `std::print` |
| `std::to_underlying` | `std::byteswap` |
| | `if consteval` |
| | deducing `this` |

## The rule is enforced, not trusted

Because a missing header is easy to use accidentally and only notice much
later, the integrity gate actively **fails the build** if any of the
following are included anywhere in the tree:

- `<expected>`
- `<format>`
- `<print>`
- `<stacktrace>`
- `<flat_map>`
- `<generator>`

This keeps the two build lanes from silently drifting apart: code that only
compiles on a newer standard library would otherwise pass here and fail on
the shop machine (or vice versa) without anyone noticing until it mattered.

## What this means for how code gets written

- Error handling uses exceptions (`RuleViolation` and friends) and
  `std::optional`, not `std::expected`.
- String formatting uses `std::string` concatenation and `std::to_string`,
  not `std::format`.
- Diagnostics print through `std::ostream` (`<<`), not `std::print`.
- Associative containers use `std::map` / `std::unordered_map`, not
  `std::flat_map`.

## Measuring, not assuming

The verification harness (Phase 1.6) includes a language probe that prints
the compiler identity and the measured `__cplusplus` value on every run. A
probe that reports success without measuring anything is worse than no
probe -- this project had that bug once already (the makefile said
`__cplusplus 0` for a period because the probe's output was never actually
captured), and the fix was to make the probe's own output part of what
`make check` verifies.
