#!/usr/bin/env python3
import argparse
import re
import sys
from pathlib import Path

BANNED_APIS = ("strcpy", "strcat", "sprintf", "gets")
PATTERN = re.compile(r"\b(" + "|".join(BANNED_APIS) + r")\s*\(")
SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".h"}


def strip_comments_and_strings(source: str) -> str:
    result = []
    index = 0
    state = "code"
    while index < len(source):
        current = source[index]
        following = source[index + 1] if index + 1 < len(source) else ""
        if state == "code":
            if current == "/" and following == "/":
                state = "line_comment"
                result.extend("  ")
                index += 2
                continue
            if current == "/" and following == "*":
                state = "block_comment"
                result.extend("  ")
                index += 2
                continue
            if current == '"':
                state = "string"
                result.append(" ")
                index += 1
                continue
            if current == "'":
                state = "character"
                result.append(" ")
                index += 1
                continue
            result.append(current)
        elif state == "line_comment":
            if current == "\n":
                state = "code"
                result.append("\n")
            else:
                result.append(" ")
        elif state == "block_comment":
            if current == "*" and following == "/":
                state = "code"
                result.extend("  ")
                index += 2
                continue
            result.append("\n" if current == "\n" else " ")
        else:
            if current == "\\":
                result.extend("  ")
                index += 2
                continue
            if (state == "string" and current == '"') or (state == "character" and current == "'"):
                state = "code"
            result.append("\n" if current == "\n" else " ")
        index += 1
    return "".join(result)


def scan_file(path: Path) -> int:
    try:
        source = path.read_text(encoding="utf-8")
    except OSError as error:
        print(f"arcc: cannot read {path}: {error}", file=sys.stderr)
        return 1
    clean = strip_comments_and_strings(source)
    violations = 0
    for match in PATTERN.finditer(clean):
        line = clean.count("\n", 0, match.start()) + 1
        print(f"arcc: banned API {match.group(1)} at {path}:{line}", file=sys.stderr)
        violations += 1
    return violations


def source_paths(paths):
    for raw_path in paths:
        path = Path(raw_path)
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES:
            yield path
        elif path.is_dir():
            for candidate in sorted(path.rglob("*")):
                if candidate.is_file() and candidate.suffix.lower() in SOURCE_SUFFIXES:
                    yield candidate


def main() -> int:
    parser = argparse.ArgumentParser(description="ALRIOS banned C API scanner")
    parser.add_argument("paths", nargs="+")
    args = parser.parse_args()
    violations = sum(scan_file(path) for path in source_paths(args.paths))
    if violations:
        print(f"BANNED_APIS_GATE: FAIL ({violations} violations)", file=sys.stderr)
        return 1
    print("BANNED_APIS_GATE: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
