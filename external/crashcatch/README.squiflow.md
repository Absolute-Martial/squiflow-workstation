# CrashCatch in SquiFlow

CrashCatch is the crash-reporting engine used only behind SquiFlow's platform crash-handler adapter.

- Upstream: CrashCatch by Keith Pottratz
- Supplied header version: 1.5.0
- Supplied archive CMake project version: 1.4.0 (recorded mismatch; the header version is used)
- Header SHA-256: `696f9c78f52780aac683916c4d0b718f002274d9a51412eff873076f7b44f796`
- License: MIT; the required notice is stored once at `packaging/licenses/CrashCatch-MIT.txt`
- Imported: 2026-08-03 from the user-supplied `CrashCatch-main.zip`

The upstream header is pinned unchanged. Application code must not include it directly. Only files inside `src/platform/` may configure or call CrashCatch. SquiFlow disables CrashCatch's built-in dialog and upload callback; restart UI belongs to the application shell.
