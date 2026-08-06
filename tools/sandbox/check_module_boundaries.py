#!/usr/bin/env python3
"""Keep protocol, CMake, and source-level module dependencies aligned."""

from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
MODULES = ROOT / "src" / "modules"
REQUIRES = ROOT / "external" / "protocol" / "include" / "squiflow" / "protocol" / "module_requires.def"


def protocol_graph() -> dict[str, set[str]]:
    graph = {path.name: set() for path in MODULES.iterdir() if path.is_dir()}
    pattern = re.compile(r"^\s*SQF_REQUIRES\(\s*([a-z_]+)\s*,\s*([a-z_]+)\s*\)", re.M)
    for dependent, dependency in pattern.findall(REQUIRES.read_text(encoding="utf-8")):
        graph.setdefault(dependent, set()).add(dependency)
    return graph


def cmake_requirements(path: Path) -> tuple[str, set[str]]:
    text = path.read_text(encoding="utf-8")
    match = re.search(r"squiflow_add_module\(\s*([a-z_]+)(.*?)\)", text, re.S)
    if match is None:
        raise ValueError("missing squiflow_add_module declaration")
    name, body = match.groups()
    requires = re.search(
        r"\bREQUIRES\b(.*?)(?=\b(?:SOURCES|UI|TEST_SOURCES)\b|$)", body, re.S
    )
    return name, set(re.findall(r"[a-z_]+", requires.group(1))) if requires else set()


def source_dependencies(module: str) -> set[str]:
    dependencies: set[str] = set()
    pattern = re.compile(r'^\s*#include\s+"modules/([a-z_]+)/', re.M)
    for path in (MODULES / module).rglob("*"):
        if path.suffix not in {".cpp", ".hpp"} or "tests" in path.parts:
            continue
        for dependency in pattern.findall(path.read_text(encoding="utf-8")):
            if dependency != module:
                dependencies.add(dependency)
    return dependencies


def main() -> int:
    expected = protocol_graph()
    errors: list[str] = []
    for module in sorted(expected):
        try:
            declared_name, declared = cmake_requirements(MODULES / module / "CMakeLists.txt")
        except (OSError, ValueError) as error:
            errors.append(f"{module}: {error}")
            continue
        if declared_name != module:
            errors.append(f"{module}: CMake declares module {declared_name}")
        if declared != expected[module]:
            errors.append(
                f"{module}: CMake requirements {sorted(declared)} != protocol {sorted(expected[module])}"
            )
        undeclared = source_dependencies(module) - expected[module]
        if undeclared:
            errors.append(f"{module}: source includes undeclared modules {sorted(undeclared)}")
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    edge_count = sum(len(dependencies) for dependencies in expected.values())
    print(f"Module boundary policy: {len(expected)} modules, {edge_count} declared edges passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
