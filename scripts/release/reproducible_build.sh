#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUTPUT_DIR="${1:?usage: reproducible_build.sh <output-dir>}"
COMMIT="$(git -C "$ROOT" rev-parse HEAD)"
SOURCE_DATE_EPOCH="${SOURCE_DATE_EPOCH:-$(git -C "$ROOT" show -s --format=%ct "$COMMIT")}"
WORK_DIR="$OUTPUT_DIR/work"
SOURCE_DIR="$WORK_DIR/source"
BUILD_DIR="$WORK_DIR/build"
ARTIFACT_DIR="$OUTPUT_DIR/artifacts"
METADATA_DIR="$OUTPUT_DIR/metadata"

if [[ "${ALRIOS_ALLOW_DIRTY_SOURCE:-0}" != "1" ]] && ! git -C "$ROOT" diff --quiet --ignore-submodules --; then
  printf '%s\n' "reproducible build requires a clean tracked worktree" >&2
  exit 1
fi

rm -rf "$WORK_DIR" "$ARTIFACT_DIR" "$METADATA_DIR"
mkdir -p "$SOURCE_DIR" "$ARTIFACT_DIR" "$METADATA_DIR"

if [[ "${ALRIOS_ALLOW_DIRTY_SOURCE:-0}" == "1" ]]; then
  tar --exclude=.git --exclude=build --exclude=build-linux --exclude=arcore \
      --exclude=node_modules --exclude=__pycache__ -cf - -C "$ROOT" . | tar -xf - -C "$SOURCE_DIR"
else
  git -C "$ROOT" archive --format=tar "$COMMIT" | tar -xf - -C "$SOURCE_DIR"
fi

export SOURCE_DATE_EPOCH TZ=UTC LC_ALL=C LANG=C PYTHONHASHSEED=0
unset http_proxy https_proxy HTTP_PROXY HTTPS_PROXY ALL_PROXY all_proxy NO_PROXY no_proxy

python3 - "$SOURCE_DIR" <<'PY'
import sys
from pathlib import Path

for path in Path(sys.argv[1]).rglob("CMakeLists.txt"):
    content = path.read_text(encoding="utf-8", errors="ignore")
    forbidden = ("FetchContent_Declare", "ExternalProject_Add", "file(DOWNLOAD", "git clone", "curl ", "wget ")
    if any(token in content for token in forbidden):
        raise SystemExit(f"offline build policy rejected network-capable directive in {path}")
PY

cmake -S "$SOURCE_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_BUILD_RPATH_USE_ORIGIN=ON \
  -DCMAKE_C_FLAGS="-O2 -D_FORTIFY_SOURCE=3 -ffile-prefix-map=$SOURCE_DIR=. -fdebug-prefix-map=$SOURCE_DIR=." \
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--build-id=none"
cmake --build "$BUILD_DIR" --parallel 1

if [[ ! -d "$SOURCE_DIR/alrios-release" ]]; then
  printf '%s\n' "reproducible build produced no alrios-release artifact directory" >&2
  exit 1
fi
cp -a "$SOURCE_DIR/alrios-release/." "$ARTIFACT_DIR/"

python3 "$ROOT/scripts/release/sbom.py" \
  --source "$SOURCE_DIR" \
  --output "$METADATA_DIR/sbom.json" \
  --version "$COMMIT"

(
  cd "$ARTIFACT_DIR"
  find . -type f -print0 | LC_ALL=C sort -z | xargs -0 sha256sum > "$METADATA_DIR/artifacts.sha256"
)
ARTIFACT_HASH="$(sha256sum "$METADATA_DIR/artifacts.sha256" | cut -d ' ' -f 1)"
SBOM_HASH="$(sha256sum "$METADATA_DIR/sbom.json" | cut -d ' ' -f 1)"
python3 - "$METADATA_DIR/provenance.json" "$ARTIFACT_HASH" "$SBOM_HASH" "$COMMIT" "$SOURCE_DATE_EPOCH" <<'PY'
import json
import sys
from pathlib import Path

path, artifact_hash, sbom_hash, commit, epoch = sys.argv[1:]
record = {
    "artifact_sha256": artifact_hash,
    "commit": commit,
    "network": "disabled",
    "sbom_sha256": sbom_hash,
    "source_date_epoch": int(epoch),
}
Path(path).write_text(json.dumps(record, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")
PY
sha256sum "$METADATA_DIR/provenance.json" > "$METADATA_DIR/provenance.sha256"
rm -rf "$WORK_DIR"
