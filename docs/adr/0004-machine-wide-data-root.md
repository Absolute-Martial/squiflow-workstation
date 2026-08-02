# 0004 - Shop data lives in the machine-wide program-data location

Status: Accepted, 2026-08-03

## Context

A shop counter machine is shared. Windows accounts get created for a new
employee, a technician logs in with a different account, or the owner uses a
second profile. If the database sits under the per-user application data
folder, the shop's records appear to vanish the moment somebody logs in as
someone else, and a second copy quietly starts accumulating.

## Decision

The database, logs, backups, crash dumps, and the protected secrets store all
live under the machine-wide program-data location, derived at runtime from the
platform path layer. Only the cache is per-user, and the cache is disposable by
definition: deleting all of it may cost time, never a record.

The location is never written literally into the source. On Windows it is
derived from `QStandardPaths::standardLocations(QStandardPaths::AppDataLocation)`,
from which the entry that is neither the per-user writable location nor inside
the application directory is the program-data rooted one. If no such entry
exists, startup refuses loudly rather than falling back to a per-user folder.
An explicit override through the environment exists for support and for
testing.

## Alternatives rejected

- Per-user application data: the failure described above.
- The installation directory: normally not writable, and lost on upgrade.
- A literal drive path: forbidden outright by the code of conduct.

## Consequences

The installer must grant write access on the data directory to the accounts
that run the application; that is a packaging task recorded for Phase 9. In
return, every account on the machine sees exactly the same shop.
