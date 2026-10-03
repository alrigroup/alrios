#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".h", ".py", ".sh", ".cmake", ".txt", ".json"}
EXCLUDED_DIRECTORIES = {".git", "build", "build-linux", "arcore", "node_modules", "__pycache__"}


def file_hash(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def source_files(root: Path):
    for path in sorted(root.rglob("*")):
        if not path.is_file() or any(part in EXCLUDED_DIRECTORIES for part in path.parts):
            continue
        if path.suffix.lower() in SOURCE_SUFFIXES or path.name in {"CMakeLists.txt", "LICENSE"}:
            yield path


def main() -> int:
    parser = argparse.ArgumentParser(description="Generate deterministic ALRIOS SPDX-like SBOM")
    parser.add_argument("--source", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--version", required=True)
    args = parser.parse_args()

    root = args.source.resolve()
    components = []
    for path in source_files(root):
        components.append({
            "name": path.relative_to(root).as_posix(),
            "sha256": file_hash(path),
            "type": "file",
        })
    document = {
        "bomFormat": "ALRIOS-SBOM",
        "specVersion": "1.0",
        "version": 1,
        "metadata": {
            "component": {"name": "ALRIOS", "version": args.version, "type": "operating-system"}
        },
        "components": components,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
