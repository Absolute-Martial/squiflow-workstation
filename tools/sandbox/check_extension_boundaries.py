#!/usr/bin/env python3
"""Reject same-process server extension loading and embedded script runtimes."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SCAN_ROOT = ROOT / "server"
SOURCE_SUFFIXES = {".hpp", ".cpp", ".h", ".cc", ".cxx"}

FORBIDDEN = (
    (
        "runtime native-library loading",
        re.compile(
            r"(?:\bLoadLibrary(?:Ex)?[AW]?\s*\(|\bGetProcAddress\s*\(|"
            r"#\s*include\s*<dlfcn\.h>|\bdlopen\s*\(|\bdlsym\s*\(|"
            r"\bQPluginLoader\b|\bQLibrary\b|\bboost::dll\b|"
            r"\blt_dlopen\s*\(|\buv_dlopen\s*\()"
        ),
    ),
    (
        "embedded Python runtime",
        re.compile(r"(?:#\s*include\s*[<\"]Python\.h[>\"]|\bPy_Initialize\s*\()"),
    ),
    (
        "embedded Lua runtime",
        re.compile(
            r"(?:#\s*include\s*[<\"]lua(?:\.h|/lua\.h)[>\"]|"
            r"\bluaL_newstate\s*\()"
        ),
    ),
    (
        "embedded QuickJS runtime",
        re.compile(r"(?:#\s*include\s*[<\"]quickjs\.h[>\"]|\bJS_NewRuntime\s*\()"),
    ),
)

EXTENSION_PROVIDER_ACCESS = re.compile(
    r"(?:#\s*include\s*[<\"](?:pqxx/|hical/|curl/|avif/)|"
    r"#\s*include\s*[\"](?:\.\./)*adapters/|"
    r"\b(?:pqxx|hical|boost::asio|boost::json)::|\bcurl_easy_)"
)


def violations(path: str, text: str) -> list[str]:
    found = [name for name, pattern in FORBIDDEN if pattern.search(text)]
    if path.startswith("server/src/extensions/") and EXTENSION_PROVIDER_ACCESS.search(text):
        found.append("extension contract accessing provider adapter API")
    return found


def self_test() -> None:
    bad = {
        "server/src/x.cpp": "auto h = LoadLibraryW(L\"plugin.dll\");",
        "server/src/x.cpp#2": "#include <dlfcn.h>\nauto p = dlopen(name, 0);",
        "server/src/x.cpp#3": "QPluginLoader loader(path);",
        "server/src/x.cpp#4": "#include <Python.h>\nPy_Initialize();",
        "server/src/extensions/x.cpp": "#include <pqxx/pqxx>",
    }
    for fake_path, text in bad.items():
        assert violations(fake_path.split("#", 1)[0], text), fake_path
    assert not violations(
        "server/src/extensions/manifest.cpp",
        '#include "extension_manifest.hpp"\nnamespace squiflow {}',
    )


def main() -> int:
    self_test()
    errors: list[str] = []
    checked = 0
    if SCAN_ROOT.exists():
        for path in SCAN_ROOT.rglob("*"):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            checked += 1
            rel = path.relative_to(ROOT).as_posix()
            text = path.read_text(encoding="utf-8")
            for problem in violations(rel, text):
                errors.append(f"{rel}: {problem} is forbidden by ADR 0015")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"extension trust boundary policy: {checked} server sources passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
