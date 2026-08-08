#!/usr/bin/env python3
"""Forbid in-process third-party extension loading in trusted code."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SEARCH_ROOTS = (ROOT / "src", ROOT / "server" / "src")
SUFFIXES = {".cpp", ".cc", ".cxx", ".hpp", ".h"}
FORBIDDEN = {
    r"\bdlopen\s*\(": "POSIX dynamic loading",
    r"\bdlsym\s*\(": "POSIX symbol lookup",
    r"\bLoadLibrary(?:A|W)?\s*\(": "Windows dynamic loading",
    r"\bGetProcAddress\s*\(": "Windows symbol lookup",
    r"\bQLibrary\b": "Qt in-process library loading",
    r"\bQPluginLoader\b": "Qt in-process plugin loading",
    r"\bPyImport_": "embedded Python addon loading",
    r"\b(?:luaL_load|lua_pcall)\b": "embedded Lua addon execution",
}
errors: list[str] = []
for base in SEARCH_ROOTS:
    if not base.exists():
        continue
    for path in sorted(base.rglob("*")):
        if path.suffix not in SUFFIXES:
            continue
        text = path.read_text(encoding="utf-8", errors="replace")
        for pattern, reason in FORBIDDEN.items():
            if re.search(pattern, text):
                errors.append(f"{path.relative_to(ROOT)}: {reason} is prohibited")
for required in (
    "docs/adr/0015-external-extensions-never-share-core-process.md",
    "docs/research/open-source-extension-security-review.md",
):
    if not (ROOT / required).exists():
        errors.append(f"{required}: missing")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print("extension boundary policy: trusted processes contain no external loaders")
