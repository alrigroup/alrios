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

REVISION="$(git -C "$ROOT" rev-parse HEAD)"
CC_VERSION="$("${CC:-cc}" --version | sed -n '1p')"
CMAKE_VERSION="$("$ALRIOS_CMAKE_BIN" --version | sed -n '1p')"
OPENSSL_VERSION="$(openssl version | sed -n '1p')"

declare -a BASE_CONTRACT=(
    --source-root "$ROOT"
    --artifact "$WORK/release/arcc"
    --revision "$REVISION"
    --epoch 1700000000
    --target arcc
    --dependency "OpenSSL=$OPENSSL_VERSION"
    --toolchain "CCompiler=$CC_VERSION"
    --toolchain "CMake=$CMAKE_VERSION"
    --build-type Release
    --network-policy LinuxNetworkNamespace
    --normalized-flag=-O2
    --normalized-flag=-DNDEBUG
    --normalized-flag=-ffile-prefix-map
    --normalized-flag=-fdebug-prefix-map
    --normalized-flag=-fmacro-prefix-map
    --normalized-flag=-Wl,--build-id=sha1
    --normalized-flag=ARFLAGS=rcD
    --normalized-flag=ZERO_AR_DATE=1
)
if command -v ldd >/dev/null 2>&1; then
    mapfile -t LIBS < <(ldd "$WORK/release/arcc" | awk '/=>/ && $1 != "linux-vdso.so.1" { print $1 } $1 ~ /^\// { count = split($1, parts, "/"); print parts[count] }' | sed 's#^.*/##' | LC_ALL=C sort -u)
    for lib in "${LIBS[@]}"; do
        BASE_CONTRACT+=(--dependency "$lib=runtime-resolved")
    done
fi

python3 "$ROOT/scripts/release/sbom.py" verify \
    "${BASE_CONTRACT[@]}" \
    --sbom "$WORK/release/alrios.spdx.json" \
    --provenance "$WORK/release/alrios.provenance.json"

if grep -F -- "$ROOT" "$WORK/release/alrios.spdx.json" "$WORK/release/alrios.provenance.json" >/dev/null; then
    fail "metadata leaked the local repository path"
fi
if grep -F -- "$WORK" "$WORK/release/alrios.spdx.json" "$WORK/release/alrios.provenance.json" >/dev/null; then
    fail "metadata leaked the local build path"
fi

for copy in one two; do
    python3 "$ROOT/scripts/release/sbom.py" generate \
        --source-root "$ROOT" \
        --artifact "$WORK/release/arcc" \
        --revision "$REVISION" \
        --epoch 1700000000 \
        --target arcc \
        --toolchain "CCompiler=$CC_VERSION" \
        --toolchain "CMake=$CMAKE_VERSION" \
        --dependency "OpenSSL=$OPENSSL_VERSION" \
        --normalized-flag=-O2 \
        --sbom "$WORK/$copy.spdx.json" \
        --provenance "$WORK/$copy.provenance.json"
done
cmp --silent "$WORK/one.spdx.json" "$WORK/two.spdx.json" || fail "SBOM generation is not deterministic"
cmp --silent "$WORK/one.provenance.json" "$WORK/two.provenance.json" || fail "provenance generation is not deterministic"

mutate_and_expect_failure() {
    local label="$1"
    local json_file="$2"
    local python_expr="$3"
    python3 - "$json_file" "$WORK/mutated.json" "$python_expr" <<'PY'
import json, sys
source, destination, expr = sys.argv[1], sys.argv[2], sys.argv[3]
with open(source, "r", encoding="utf-8") as stream:
    data = json.load(stream)
scope = {"d": data}
exec(expr, scope)
with open(destination, "w", encoding="utf-8") as stream:
    json.dump(data, stream, indent=2)
PY
    local sbom_arg="$WORK/release/alrios.spdx.json"
    local prov_arg="$WORK/release/alrios.provenance.json"
    if [[ "$json_file" == *spdx* ]]; then
        sbom_arg="$WORK/mutated.json"
    else
        prov_arg="$WORK/mutated.json"
    fi
    if python3 "$ROOT/scripts/release/sbom.py" verify "${BASE_CONTRACT[@]}" --sbom "$sbom_arg" --provenance "$prov_arg" >/dev/null 2>&1; then
        fail "mutation was unexpectedly accepted: $label"
    fi
}

mutate_and_expect_failure "toolchain version modified" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["internalParameters"]["toolchains"][0]["version"] = "99.0"'
mutate_and_expect_failure "toolchain name modified" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["internalParameters"]["toolchains"][0]["name"] = "OtherCC"'
mutate_and_expect_failure "toolchain path embedded" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["internalParameters"]["toolchains"][0]["version"] = "GCC /home/user/toolchain"'
mutate_and_expect_failure "buildType Debug" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["externalParameters"]["buildType"] = "Debug"'
mutate_and_expect_failure "builder ID modified" "$WORK/release/alrios.provenance.json" 'd["predicate"]["runDetails"]["builder"]["id"] = "https://example.com/builder"'
mutate_and_expect_failure "source URI modified" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["resolvedDependencies"][0]["uri"] = "git+https://example.com/alrios.git"'
mutate_and_expect_failure "invocation ID zeroed" "$WORK/release/alrios.provenance.json" 'd["predicate"]["runDetails"]["metadata"]["invocationId"] = "sha256:" + "0"*64'
mutate_and_expect_failure "duplicate toolchains" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["internalParameters"]["toolchains"].append(dict(d["predicate"]["buildDefinition"]["internalParameters"]["toolchains"][0]))'
mutate_and_expect_failure "missing builder ID" "$WORK/release/alrios.provenance.json" 'del d["predicate"]["runDetails"]["builder"]["id"]'
mutate_and_expect_failure "networkAccess true" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["externalParameters"]["networkAccess"] = True'
mutate_and_expect_failure "targets reordered" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["externalParameters"]["targets"] = ["extra", "arcc"]'
mutate_and_expect_failure "source tree digest altered" "$WORK/release/alrios.provenance.json" 'd["predicate"]["buildDefinition"]["resolvedDependencies"][0]["digest"]["gitTree"] = "0"*40'
mutate_and_expect_failure "missing creationInfo" "$WORK/release/alrios.spdx.json" 'del d["creationInfo"]'
mutate_and_expect_failure "missing documentNamespace" "$WORK/release/alrios.spdx.json" 'del d["documentNamespace"]'
mutate_and_expect_failure "missing SPDXID" "$WORK/release/alrios.spdx.json" 'del d["SPDXID"]'
mutate_and_expect_failure "bad SBOM revision" "$WORK/release/alrios.spdx.json" 'd["packages"][0]["versionInfo"] = "0"*40'
mutate_and_expect_failure "dangling SPDX relationship" "$WORK/release/alrios.spdx.json" 'd["relationships"].append({"spdxElementId":"SPDXRef-DOCUMENT","relationshipType":"CONTAINS","relatedSpdxElement":"SPDXRef-Dangling"})'
mutate_and_expect_failure "duplicate SPDX file ID" "$WORK/release/alrios.spdx.json" 'd["files"].append(dict(d["files"][0]))'
mutate_and_expect_failure "embedded Windows path" "$WORK/release/alrios.spdx.json" 'd["packages"][1]["versionInfo"] = "C:\\private\\lib"'
mutate_and_expect_failure "embedded UNC path" "$WORK/release/alrios.spdx.json" 'd["packages"][1]["versionInfo"] = "\\\\server\\share\\lib"'

cp -- "$WORK/release/arcc" "$WORK/arcc.original"
printf '\0' >> "$WORK/release/arcc"
if python3 "$ROOT/scripts/release/sbom.py" verify "${BASE_CONTRACT[@]}" --sbom "$WORK/release/alrios.spdx.json" --provenance "$WORK/release/alrios.provenance.json" >/dev/null 2>&1; then
    fail "provenance verification accepted a modified artifact"
fi
mv -- "$WORK/arcc.original" "$WORK/release/arcc"

WORKTREE_DIR="$WORK/dirty-worktree"
git -C "$ROOT" worktree add --detach "$WORKTREE_DIR" "$REVISION" >/dev/null
TRACKED_MUTATION="$WORKTREE_DIR/README.md"
printf '\nDIRTY_CONTENT\n' >> "$TRACKED_MUTATION"
python3 "$ROOT/scripts/release/sbom.py" generate \
    --source-root "$WORKTREE_DIR" \
    --artifact "$WORK/release/arcc" \
    --revision "$REVISION" \
    --epoch 1700000000 \
    --target arcc \
    --toolchain "CCompiler=$CC_VERSION" \
    --toolchain "CMake=$CMAKE_VERSION" \
    --dependency "OpenSSL=$OPENSSL_VERSION" \
    --normalized-flag=-O2 \
    --sbom "$WORK/dirty.spdx.json" \
    --provenance "$WORK/dirty.provenance.json"
cmp --silent "$WORK/one.spdx.json" "$WORK/dirty.spdx.json" || fail "dirty working tree altered immutable commit SBOM"
cmp --silent "$WORK/one.provenance.json" "$WORK/dirty.provenance.json" || fail "dirty working tree altered immutable commit provenance"
git -C "$ROOT" worktree remove --force "$WORKTREE_DIR" >/dev/null

printf '[test_reproducibility] PASS\n'
