#!/usr/bin/env bash
# Copyright (c) 2026 ALRIGROUP and its affiliates.
# Engineered and maintained by ALRI Development.
#
# This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
# found in the LICENSE file in the root directory of this source tree.

set -euo pipefail
IFS=$'\n\t'
umask 077

fail() {
    printf '[test_reproducibility] ERROR: %s\n' "$*" >&2
    exit 1
}

[[ $# -eq 1 ]] || fail "expected repository root argument"
ROOT="$(cd -- "$1" && pwd -P)"
[[ -f "$ROOT/scripts/release/reproducible_build.sh" ]] || fail "release build script is missing"
[[ -f "$ROOT/scripts/release/sbom.py" ]] || fail "SBOM tool is missing"
[[ -n "${ALRIOS_CMAKE_BIN:-}" && -x "$ALRIOS_CMAKE_BIN" ]] || fail "ALRIOS_CMAKE_BIN is unavailable"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/alrios-repro-test.XXXXXXXX")"
cleanup() {
    rm -rf -- "$WORK"
}
trap cleanup EXIT HUP INT TERM

mkdir -p -- "$WORK/bin" "$WORK/release"
CMAKE_WRAPPER="$WORK/bin/cmake"
printf '%s\n' \
    '#!/usr/bin/env bash' \
    'set -euo pipefail' \
    'if [[ "${1:-}" == "-S" || "${1:-}" == "--build" ]]; then' \
    '    mapfile -t interfaces < <(python3 -c '\''import socket; print("\\n".join(sorted(name for _, name in socket.if_nameindex())))'\'')' \
    '    [[ ${#interfaces[@]} -eq 1 && "${interfaces[0]}" == "lo" ]] || {' \
    '        printf "build command has network access: %s\\n" "${interfaces[*]}" >&2' \
    '        exit 97' \
    '    }' \
    'fi' \
    "exec $(printf '%q' "$ALRIOS_CMAKE_BIN") \"\$@\"" \
    > "$CMAKE_WRAPPER"
chmod 0755 -- "$CMAKE_WRAPPER"

PATH="$WORK/bin:$PATH" SOURCE_DATE_EPOCH=1700000000 \
    bash "$ROOT/scripts/release/reproducible_build.sh" \
        --output "$WORK/release" \
        --target arcc

for expected in arcc SHA256SUMS alrios.spdx.json alrios.provenance.json; do
    [[ -f "$WORK/release/$expected" && ! -L "$WORK/release/$expected" ]] || fail "missing release output: $expected"
done
(
    cd -- "$WORK/release"
    sha256sum --check SHA256SUMS
)

python3 "$ROOT/scripts/release/sbom.py" verify \
    --source-root "$ROOT" \
    --artifact "$WORK/release/arcc" \
    --sbom "$WORK/release/alrios.spdx.json" \
    --provenance "$WORK/release/alrios.provenance.json"

if grep -F -- "$ROOT" "$WORK/release/alrios.spdx.json" "$WORK/release/alrios.provenance.json" >/dev/null; then
    fail "metadata leaked the local repository path"
fi
if grep -F -- "$WORK" "$WORK/release/alrios.spdx.json" "$WORK/release/alrios.provenance.json" >/dev/null; then
    fail "metadata leaked the local build path"
fi

REVISION="$(git -C "$ROOT" rev-parse HEAD)"
for copy in one two; do
    python3 "$ROOT/scripts/release/sbom.py" generate \
        --source-root "$ROOT" \
        --artifact "$WORK/release/arcc" \
        --sbom "$WORK/$copy.spdx.json" \
        --provenance "$WORK/$copy.provenance.json" \
        --revision "$REVISION" \
        --epoch 1700000000 \
        --dependency 'OpenSSL=test-version' \
        --toolchain 'CCompiler=test-version' \
        --parameter 'networkIsolation=LinuxNetworkNamespace'
done
cmp --silent "$WORK/one.spdx.json" "$WORK/two.spdx.json" || fail "SBOM generation is not deterministic"
cmp --silent "$WORK/one.provenance.json" "$WORK/two.provenance.json" || fail "provenance generation is not deterministic"

cp -- "$WORK/release/arcc" "$WORK/arcc.original"
printf '\0' >> "$WORK/release/arcc"
if python3 "$ROOT/scripts/release/sbom.py" verify \
    --source-root "$ROOT" \
    --artifact "$WORK/release/arcc" \
    --sbom "$WORK/release/alrios.spdx.json" \
    --provenance "$WORK/release/alrios.provenance.json"; then
    fail "provenance verification accepted a modified artifact"
fi
mv -- "$WORK/arcc.original" "$WORK/release/arcc"

printf '[test_reproducibility] PASS\n'
