#!/usr/bin/env python3
"""Keep Phase 8 third-party provider APIs inside their adapter targets."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SCAN_ROOTS = (ROOT / "server", ROOT / "src", ROOT / "tests")
SOURCE_SUFFIXES = {".hpp", ".cpp", ".h", ".cc"}

RULES = (
    (
        "Hical",
        re.compile(r"(?:#\s*include\s*[<\"]hical/|\bhical::|#\s*include\s*[<\"](?:core|asio)/)"),
        ("server/src/adapters/http/hical/", "server/tests/adapters/http/hical/"),
    ),
    (
        "Hical Boost transport",
        re.compile(r"(?:#\s*include\s*<boost/(?:asio|json)|\bboost::(?:asio|json)::)"),
        ("server/src/adapters/http/hical/", "server/tests/adapters/http/hical/"),
    ),
    (
        "libpqxx",
        re.compile(r"(?:#\s*include\s*<pqxx/|\bpqxx::)"),
        ("server/src/adapters/postgres/libpqxx/", "server/tests/adapters/postgres/libpqxx/"),
    ),
    (
        "libcurl",
        re.compile(r"(?:#\s*include\s*<curl/|\bcurl_(?:easy|multi|mime|slist)_)"),
        ("server/src/adapters/io/curl/", "server/tests/adapters/io/curl/"),
    ),
    (
        "libavif",
        re.compile(r"(?:#\s*include\s*<avif/|\bavif(?:Image|Decoder|Encoder|Result)\b)"),
        ("server/src/adapters/media/libavif/", "server/tests/adapters/media/libavif/"),
    ),
)


def relative(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def allowed(path: str, prefixes: tuple[str, ...]) -> bool:
    return any(path.startswith(prefix) for prefix in prefixes)


def main() -> int:
    errors: list[str] = []
    checked = 0
    for base in SCAN_ROOTS:
        if not base.exists():
            continue
        for path in base.rglob("*"):
            if not path.is_file() or path.suffix not in SOURCE_SUFFIXES:
                continue
            checked += 1
            rel = relative(path)
            text = path.read_text(encoding="utf-8")
            for provider, pattern, prefixes in RULES:
                if pattern.search(text) and not allowed(rel, prefixes):
                    errors.append(
                        f"{rel}: {provider} API is outside its provider adapter "
                        f"({', '.join(prefixes)})"
                    )
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"server provider boundary policy: {checked} source files passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
