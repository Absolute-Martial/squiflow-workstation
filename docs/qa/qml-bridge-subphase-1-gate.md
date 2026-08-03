# QML bridge Sub-phase 1 architecture gate

## Delivered

- Enforced a one-way Qt seam: engine, modules, workflows and Phase 6 lifecycle code reject Qt/QObject/QML registration tokens.
- Rejected direct Domain/Engine/Modules imports from QML.
- Rejected raw domain pointers and STL types in shell `Q_PROPERTY` declarations.
- Added malformed-policy fixtures proving the checker fails closed.
- Added a project `Result<T, E>` boundary, including move-only values, equal value/error types and `void` success.
- Added explicit `DomainError` codes and retry classification.
- Added immutable, owned `RequestContext` with tenant, user, permission snapshot, correlation ID and session generation validation.

## Gate evidence

- Focused C++ contracts: **16 checks, 0 failed**.
- Architecture checker fixtures: **6 cases passed**.
- Full strict gate: **5,281 assertions, 0 failed**.
- Independent CMake build: passed.
- CTest: **34/34 passed**.

## Compiler compatibility decision

The verification compiler's standard library does not provide `<expected>`. Service interfaces use the project `Result<T, E>` subset so errors are explicit today. It is intentionally shaped for an internal switch to `std::expected` when the minimum compiler is raised; no service signature will need to change.
