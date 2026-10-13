#!/usr/bin/env bash
# Copyright (c) 2026 ALRIGROUP and its affiliates.
# Engineered and maintained by ALRI Development.
#
# This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
# found in the LICENSE file in the root directory of this source tree.

set -euo pipefail
IFS=$'\n\t'
umask 022

fail() {
    printf '[reproducible-build] ERROR: %s\n' "$*" >&2
    exit 1
}

usage() {
    cat <<'EOF'
Usage: reproducible_build.sh --output DIRECTORY [--target TARGET ...]

Builds release targets twice from clean Git archives in separate build trees,
with network namespaces and reproducibility normalization. The default targets
are arcore, alrios, armake, and arcc.
EOF
}

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd -P)"
OUTPUT=""
declare -a TARGETS=()

while (($# > 0)); do
    case "$1" in
        --output)
            (($# >= 2)) || fail "--output requires a directory"
            OUTPUT="$2"
            shift 2
            ;;
        --target)
            (($# >= 2)) || fail "--target requires a target name"
            [[ "$2" =~ ^[A-Za-z0-9][A-Za-z0-9_.+-]{0,63}$ ]] || fail "invalid target name: $2"
            TARGETS+=("$2")
            shift 2
            ;;
        --help|-h)
            usage
            exit 0
            ;;
        *)
            fail "unknown argument: $1"
            ;;
    esac
done

[[ -n "$OUTPUT" ]] || fail "--output is required"
if ((${#TARGETS[@]} == 0)); then
    TARGETS=(arcore alrios armake arcc)
fi

for required in git tar cmake python3 sha256sum unshare; do
    command -v "$required" >/dev/null 2>&1 || fail "required tool is unavailable: $required"
done

GIT_BIN="$(command -v git)"
TAR_BIN="$(command -v tar)"
CMAKE_BIN="$(command -v cmake)"
PYTHON_BIN="$(command -v python3)"
SHA256_BIN="$(command -v sha256sum)"
UNSHARE_BIN="$(command -v unshare)"
CC_BIN="$(command -v "${CC:-cc}" 2>/dev/null || true)"
[[ -n "$CC_BIN" ]] || fail "C compiler is unavailable or CC is not a single executable name"

[[ -d "$ROOT/.git" || -f "$ROOT/.git" ]] || fail "repository root is not a Git working tree"
"$GIT_BIN" -C "$ROOT" diff --quiet -- ':!arcore/registry.json' || fail "tracked source tree has unstaged modifications"
"$GIT_BIN" -C "$ROOT" diff --cached --quiet -- ':!arcore/registry.json' || fail "tracked source tree has staged modifications"

REVISION="$("$GIT_BIN" -C "$ROOT" rev-parse --verify HEAD)"
[[ "$REVISION" =~ ^[0-9a-f]{40,64}$ ]] || fail "Git returned an invalid source revision"

if [[ -n "${SOURCE_DATE_EPOCH:-}" ]]; then
    EPOCH="$SOURCE_DATE_EPOCH"
else
    EPOCH="$("$GIT_BIN" -C "$ROOT" show -s --format=%ct "$REVISION")"
fi
[[ "$EPOCH" =~ ^[0-9]+$ ]] || fail "SOURCE_DATE_EPOCH must be a non-negative integer"
((EPOCH <= 253402300799)) || fail "SOURCE_DATE_EPOCH is outside the supported UTC timestamp range"

mkdir -p -- "$OUTPUT"
[[ ! -L "$OUTPUT" ]] || fail "output directory must not be a symlink"
OUTPUT="$(cd -- "$OUTPUT" && pwd -P)"
case "$OUTPUT/" in
    "$ROOT/"*) fail "output directory must be outside the source working tree" ;;
esac
[[ -z "$(find "$OUTPUT" -mindepth 1 -maxdepth 1 -print -quit)" ]] || fail "output directory must be empty"
OUTPUT_ID="$($PYTHON_BIN - "$OUTPUT" <<'PY'
import os, stat, sys
path = sys.argv[1]
value = os.stat(path, follow_symlinks=False)
if not stat.S_ISDIR(value.st_mode) or value.st_uid != os.geteuid():
    raise SystemExit("output directory must be owned by the current user")
if value.st_mode & 0o022:
    raise SystemExit("output directory must not be group/world writable")
print(f"{value.st_dev}:{value.st_ino}")
PY
)" || fail "output directory ownership or mode is unsafe"

WORK="$(mktemp -d "${TMPDIR:-/tmp}/alrios-reproducible.XXXXXXXX")"
cleanup() {
    rm -rf -- "$WORK"
}
trap cleanup EXIT HUP INT TERM
RELEASE_STAGE="$WORK/release"
mkdir -m 0700 -- "$RELEASE_STAGE"

ARCHIVE="$WORK/source.tar"
"$GIT_BIN" -C "$ROOT" archive --format=tar --output="$ARCHIVE" "$REVISION"

export SOURCE_DATE_EPOCH="$EPOCH"
export TZ=UTC
export LC_ALL=C
export LANG=C
export PYTHONHASHSEED=0

PATH_VALUE="${PATH:-/usr/bin:/bin}"

network_probe=("$PYTHON_BIN" -c 'import socket, sys
interfaces = {name for _, name in socket.if_nameindex()}
if interfaces != {"lo"}:
    sys.exit("unexpected network interfaces: " + repr(sorted(interfaces)))
try:
    socket.getaddrinfo("example.com", 443)
except socket.gaierror:
    pass
else:
    sys.exit("offline namespace resolved external DNS")
for family, address in ((socket.AF_INET, ("192.0.2.1", 9)), (socket.AF_INET6, ("2001:db8::1", 9))):
    sock = socket.socket(family, socket.SOCK_STREAM)
    sock.settimeout(0.1)
    try:
        sock.connect(address)
    except OSError:
        pass
    else:
        sys.exit("offline namespace allowed an external connection")
    finally:
        sock.close()')

if ! "$UNSHARE_BIN" --user --map-root-user --net -- "${network_probe[@]}"; then
    fail "cannot establish and verify an offline network namespace"
fi

offline_run() {
    local home="$1"
    shift
    "$UNSHARE_BIN" --user --map-root-user --net -- \
        env -i \
        HOME="$home" \
        PATH="$PATH_VALUE" \
        SOURCE_DATE_EPOCH="$EPOCH" \
        TZ=UTC \
        LC_ALL=C \
        LANG=C \
        PYTHONHASHSEED=0 \
        CC="$CC_BIN" \
        ARFLAGS=rcD \
        ZERO_AR_DATE=1 \
        "$@"
}

for build_number in 1 2; do
    SOURCE_DIR="$WORK/source-$build_number"
    BUILD_DIR="$WORK/build-$build_number"
    HOME_DIR="$WORK/home-$build_number"
    mkdir -p -- "$SOURCE_DIR" "$BUILD_DIR" "$HOME_DIR"
    "$TAR_BIN" -xf "$ARCHIVE" -C "$SOURCE_DIR" --no-same-owner --no-same-permissions

    PREFIX_FLAGS="-ffile-prefix-map=$SOURCE_DIR=/usr/src/alrios -fdebug-prefix-map=$SOURCE_DIR=/usr/src/alrios -fmacro-prefix-map=$SOURCE_DIR=/usr/src/alrios -ffile-prefix-map=$BUILD_DIR=/usr/build/alrios -fdebug-prefix-map=$BUILD_DIR=/usr/build/alrios"
    offline_run "$HOME_DIR" "$CMAKE_BIN" \
        -S "$SOURCE_DIR" \
        -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_COMPILER="$CC_BIN" \
        -DCMAKE_C_FLAGS_RELEASE="-O2 -DNDEBUG $PREFIX_FLAGS" \
        -DCMAKE_EXE_LINKER_FLAGS="-Wl,--build-id=sha1" \
        -DCMAKE_SHARED_LINKER_FLAGS="-Wl,--build-id=sha1" \
        -DCMAKE_SKIP_RPATH=OFF \
        -DALRIOS_ENABLE_ML_DSA_65=OFF \
        -DALRIOS_ENABLE_ML_KEM_768=OFF
    offline_run "$HOME_DIR" "$CMAKE_BIN" --build "$BUILD_DIR" --parallel 1 --target "${TARGETS[@]}"
done

for target in "${TARGETS[@]}"; do
    FIRST="$WORK/source-1/arcore/$target"
    SECOND="$WORK/source-2/arcore/$target"
    [[ -f "$FIRST" && ! -L "$FIRST" ]] || fail "target did not produce expected artifact: $target"
    [[ -f "$SECOND" && ! -L "$SECOND" ]] || fail "second build did not produce expected artifact: $target"
    FIRST_HASH="$("$SHA256_BIN" "$FIRST" | cut -d' ' -f1)"
    SECOND_HASH="$("$SHA256_BIN" "$SECOND" | cut -d' ' -f1)"
    [[ "$FIRST_HASH" == "$SECOND_HASH" ]] || fail "artifact is not reproducible: $target ($FIRST_HASH != $SECOND_HASH)"
    install -m 0755 -- "$FIRST" "$RELEASE_STAGE/$target"
    "$PYTHON_BIN" -c 'import os, sys
stamp = int(sys.argv[1])
os.utime(sys.argv[2], (stamp, stamp))' "$EPOCH" "$RELEASE_STAGE/$target"
done

(
    cd -- "$RELEASE_STAGE"
    "$SHA256_BIN" "${TARGETS[@]}" | LC_ALL=C sort -k2
) > "$RELEASE_STAGE/SHA256SUMS"
"$PYTHON_BIN" -c 'import os, sys
stamp = int(sys.argv[1])
os.utime(sys.argv[2], (stamp, stamp))' "$EPOCH" "$RELEASE_STAGE/SHA256SUMS"

CC_VERSION="$("$CC_BIN" --version | sed -n '1p')"
CMAKE_VERSION="$("$CMAKE_BIN" --version | sed -n '1p')"
OPENSSL_BIN="$(command -v openssl 2>/dev/null || true)"
[[ -n "$OPENSSL_BIN" ]] || fail "OpenSSL CLI is unavailable"
OPENSSL_VERSION="$("$OPENSSL_BIN" version | sed -n '1p')"

declare -a METADATA_ARTIFACTS=()
declare -a METADATA_DEPENDENCIES=(--dependency "OpenSSL=$OPENSSL_VERSION")
for target in "${TARGETS[@]}"; do
    METADATA_ARTIFACTS+=(--artifact "$RELEASE_STAGE/$target")
done
if command -v ldd >/dev/null 2>&1; then
    mapfile -t LINKED_LIBRARIES < <(
        for target in "${TARGETS[@]}"; do
            ldd "$RELEASE_STAGE/$target"
        done | awk '
            /=>/ && $1 != "linux-vdso.so.1" { print $1 }
            $1 ~ /^\// { count = split($1, parts, "/"); print parts[count] }
        ' | sed 's#^.*/##' | LC_ALL=C sort -u
    )
    for library in "${LINKED_LIBRARIES[@]}"; do
        [[ "$library" =~ ^[A-Za-z0-9][A-Za-z0-9._+-]{0,127}$ ]] || fail "ldd returned an unsafe dependency name"
        METADATA_DEPENDENCIES+=(--dependency "$library=runtime-resolved")
    done
fi

CONTRACT_ARGS=(
    --source-root "$ROOT"
    "${METADATA_ARTIFACTS[@]}"
    --revision "$REVISION"
    --epoch "$EPOCH"
    "${METADATA_DEPENDENCIES[@]}"
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
for target in "${TARGETS[@]}"; do
    CONTRACT_ARGS+=(--target "$target")
done

"$PYTHON_BIN" "$ROOT/scripts/release/sbom.py" generate \
    "${CONTRACT_ARGS[@]}" \
    --sbom "$RELEASE_STAGE/alrios.spdx.json" \
    --provenance "$RELEASE_STAGE/alrios.provenance.json"

"$PYTHON_BIN" "$ROOT/scripts/release/sbom.py" verify \
    "${CONTRACT_ARGS[@]}" \
    --sbom "$RELEASE_STAGE/alrios.spdx.json" \
    --provenance "$RELEASE_STAGE/alrios.provenance.json"

"$PYTHON_BIN" - "$OUTPUT" "$OUTPUT_ID" "$RELEASE_STAGE" "$EPOCH" <<'PY'
import os, stat, sys
output, expected, stage, epoch = sys.argv[1], sys.argv[2], sys.argv[3], int(sys.argv[4])
value = os.stat(output, follow_symlinks=False)
if f"{value.st_dev}:{value.st_ino}" != expected or value.st_uid != os.geteuid() or value.st_mode & 0o022:
    raise SystemExit("output directory was replaced or made unsafe")
for name in sorted(os.listdir(stage)):
    source = os.path.join(stage, name)
    destination = os.path.join(output, name)
    if os.path.islink(source) or os.path.lexists(destination) or not stat.S_ISREG(os.stat(source, follow_symlinks=False).st_mode):
        raise SystemExit("unsafe release publication entry")
    os.link(source, destination, follow_symlinks=False)
    os.utime(destination, (epoch, epoch), follow_symlinks=False)
PY

printf '[reproducible-build] OK: %d artifact(s) reproduced and verified in %s\n' "${#TARGETS[@]}" "$OUTPUT"
