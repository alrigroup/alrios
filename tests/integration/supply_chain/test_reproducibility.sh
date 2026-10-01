#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
TMP_ROOT="$(mktemp -d)"
trap 'rm -rf "$TMP_ROOT"' EXIT

ALRIOS_ALLOW_DIRTY_SOURCE=1 SOURCE_DATE_EPOCH=0 "$ROOT/scripts/release/reproducible_build.sh" "$TMP_ROOT/first"
ALRIOS_ALLOW_DIRTY_SOURCE=1 SOURCE_DATE_EPOCH=0 "$ROOT/scripts/release/reproducible_build.sh" "$TMP_ROOT/second"

cmp "$TMP_ROOT/first/metadata/artifacts.sha256" "$TMP_ROOT/second/metadata/artifacts.sha256"
cmp "$TMP_ROOT/first/metadata/sbom.json" "$TMP_ROOT/second/metadata/sbom.json"
cmp "$TMP_ROOT/first/metadata/provenance.json" "$TMP_ROOT/second/metadata/provenance.json"

test -s "$TMP_ROOT/first/metadata/sbom.json"
test -s "$TMP_ROOT/first/metadata/provenance.json"
python3 - "$TMP_ROOT/first/metadata/provenance.json" <<'PY'
import json
import sys
record = json.load(open(sys.argv[1], encoding="utf-8"))
assert record["network"] == "disabled"
assert record["artifact_sha256"]
assert record["sbom_sha256"]
assert len(record["commit"]) == 40
PY
