#!/usr/bin/env python3
"""Per-file integrity checks.

This is not a compiler and not a test runner. It checks the properties that a
compiler will never complain about but that go wrong quietly and are painful
later: encoding, line endings, orphaned files, headers that only compile
because something else was included first, and .def files that drifted out of
their X-macro shape.

Run:  python3 tools/sandbox/integrity.py
Exit code is non-zero if any check fails.
"""

from __future__ import annotations

import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

SOURCE_DIRS = ["external/protocol", "src", "tests"]
CODE_EXT = {".hpp", ".cpp"}
DEF_EXT = {".def"}

# Files allowed to be absent from any build list.
BUILD_LIST_EXEMPT = {".hpp", ".def"}

failures: list[tuple[str, str]] = []
checked = 0


def fail(path: str, message: str) -> None:
    failures.append((path, message))


def collect(exts: set[str]) -> list[str]:
    found = []
    for base in SOURCE_DIRS:
        base_abs = os.path.join(ROOT, base)
        for dirpath, dirnames, filenames in os.walk(base_abs):
            dirnames[:] = [d for d in dirnames if d != "build"]
            for name in filenames:
                if os.path.splitext(name)[1] in exts:
                    full = os.path.join(dirpath, name)
                    found.append(os.path.relpath(full, ROOT))
    return sorted(found)


def read_bytes(rel: str) -> bytes:
    with open(os.path.join(ROOT, rel), "rb") as handle:
        return handle.read()


# ---------------------------------------------------------------- byte level

def check_bytes(rel: str, raw: bytes) -> str | None:
    """Encoding and whitespace. Returns decoded text, or None if undecodable."""
    try:
        text = raw.decode("utf-8")
    except UnicodeDecodeError as error:
        fail(rel, f"not valid UTF-8: {error}")
        return None

    if raw.startswith(b"\xef\xbb\xbf"):
        fail(rel, "starts with a UTF-8 BOM")

    if b"\r\n" in raw:
        fail(rel, "contains CRLF line endings; the repository is LF only")

    if b"\t" in raw:
        fail(rel, "contains a tab character")

    if raw and not raw.endswith(b"\n"):
        fail(rel, "does not end with a newline")

    if raw.endswith(b"\n\n"):
        fail(rel, "ends with a blank line")

    for number, line in enumerate(text.splitlines(), start=1):
        if line != line.rstrip():
            fail(rel, f"line {number} has trailing whitespace")
            break

    for number, line in enumerate(text.splitlines(), start=1):
        if any(ord(char) > 127 for char in line):
            fail(rel, f"line {number} has a non-ASCII character; keep source ASCII")
            break

    return text


# --------------------------------------------------------------- code checks

BANNED_IN_HEADERS = [
    (re.compile(r"^\s*using namespace\b", re.M), "a file-scope 'using namespace'"),
    (re.compile(r"#include\s*<iostream>"), "<iostream>, which pulls in a static constructor"),
    (re.compile(r"#include\s*<bits/"), "a libstdc++ internal <bits/...> header"),
]

CXX23_NOT_VERIFIABLE = [
    (re.compile(r"#include\s*<expected>"), "<expected>"),
    (re.compile(r"#include\s*<format>"), "<format>"),
    (re.compile(r"#include\s*<print>"), "<print>"),
    (re.compile(r"#include\s*<stacktrace>"), "<stacktrace>"),
    (re.compile(r"#include\s*<flat_map>"), "<flat_map>"),
    (re.compile(r"#include\s*<generator>"), "<generator>"),
]

TRUNCATION_MARKERS = [
    (re.compile(r"\[\.\.\.\]"), "a '[...]' truncation marker"),
]


def check_header(rel: str, text: str, is_test: bool = False) -> None:
    if text.count("#pragma once") == 0:
        fail(rel, "has no '#pragma once'")
    elif text.count("#pragma once") > 1:
        fail(rel, "has more than one '#pragma once'")
    else:
        # It must be the first thing that is not a comment or blank line.
        for line in text.splitlines():
            stripped = line.strip()
            if not stripped or stripped.startswith("//") or stripped.startswith("/*") \
                    or stripped.startswith("*"):
                continue
            if stripped != "#pragma once":
                fail(rel, "'#pragma once' is not the first directive")
            break

    for pattern, description in BANNED_IN_HEADERS:
        if is_test and "iostream" in description:
            # Test support code is never linked into the product, so the cost
            # of <iostream> does not apply to it.
            continue
        if pattern.search(text):
            fail(rel, f"header contains {description}")


def check_source(rel: str, text: str) -> None:
    """A .cpp must include its own header first, so the header proves it is
    self-contained even without the dedicated self-containment pass."""
    own_header = os.path.splitext(rel)[0] + ".hpp"
    if not os.path.exists(os.path.join(ROOT, own_header)):
        return
    includes = re.findall(r'^\s*#include\s+["<]([^">]+)[">]', text, re.M)
    if not includes:
        fail(rel, f"includes nothing, but {os.path.basename(own_header)} exists")
        return
    first = includes[0]
    if not own_header.endswith(first):
        fail(rel, f"first include is '{first}'; it must be its own header")


def check_language_policy(rel: str, text: str) -> None:
    for pattern, name in CXX23_NOT_VERIFIABLE:
        if pattern.search(text):
            fail(rel, f"includes {name}: a C++23 library header this toolchain "
                      f"cannot compile, so it would break the verification lane")
    for pattern, description in TRUNCATION_MARKERS:
        if pattern.search(text):
            fail(rel, f"contains {description}; source must never contain "
                      "transcript or archive elision placeholders")


# ---------------------------------------------------------------- def checks

DEF_CALL = re.compile(r"^\s*SQF_[A-Z_]+\(.*\)\s*$")
DEF_INCLUDE = re.compile(r'^\s*#include\s+(["<][^">]+[">])\s*$')


def check_def(rel: str, text: str) -> None:
    if "#pragma once" in text:
        fail(rel, "a .def file must not have '#pragma once'; it is included many times")
    for number, line in enumerate(text.splitlines(), start=1):
        stripped = line.strip()
        if not stripped or stripped.startswith("//"):
            continue
        if DEF_CALL.match(line) or DEF_INCLUDE.match(line):
            continue
        fail(rel, f"line {number} is neither a comment, an include, nor an "
                  f"SQF_ macro call: {stripped[:60]!r}")
        break


# ------------------------------------------------------------ orphan sources

def check_orphans(sources: list[str]) -> None:
    """Every .cpp must be named by the sandbox makefile or by a CMakeLists.
    An unreferenced source compiles nowhere and rots silently."""
    haystack = []
    for dirpath, dirnames, filenames in os.walk(ROOT):
        dirnames[:] = [d for d in dirnames if d not in ("build", ".git")]
        for name in filenames:
            if name == "CMakeLists.txt" or name == "Makefile":
                with open(os.path.join(dirpath, name), "r", encoding="utf-8") as handle:
                    haystack.append(handle.read())
    blob = "\n".join(haystack)
    for rel in sources:
        base = os.path.basename(rel)
        if base not in blob:
            fail(rel, "is not named by any CMakeLists.txt or by the sandbox makefile")


# ------------------------------------------------------------------ run them

def main() -> int:
    global checked

    code_files = collect(CODE_EXT)
    def_files = collect(DEF_EXT)

    for rel in code_files:
        checked += 1
        text = check_bytes(rel, read_bytes(rel))
        if text is None:
            continue
        check_language_policy(rel, text)
        if rel.endswith(".hpp"):
            check_header(rel, text, is_test=rel.startswith("tests/"))
        else:
            check_source(rel, text)

    for rel in def_files:
        checked += 1
        text = check_bytes(rel, read_bytes(rel))
        if text is None:
            continue
        check_def(rel, text)

    check_orphans([r for r in code_files if r.endswith(".cpp")])

    print(f"integrity: {checked} files checked")
    if failures:
        print(f"integrity: {len(failures)} problem(s)\n")
        for path, message in failures:
            print(f"  {path}\n      {message}")
        return 1
    print("integrity: all files pass")
    return 0


if __name__ == "__main__":
    sys.exit(main())
