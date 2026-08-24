#!/usr/bin/env bash
# Copyright (c) ALRIGROUP and its affiliates.
#
# This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
# found in the LICENSE file in the root directory of this source tree
# and at: https://github.com/alrigroup/licenses/tree/main
# Build & package the full ALRIOS ecosystem for Linux (native C + SPA).
# Includes: arcore (kernel), alrios + armake (tools), and app packages
# (arws, home.web, detroit.web).
# Usage (from repo root, inside WSL/Linux):
#   bash build_linux.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build-linux"

echo "== ALRIOS - Full Linux Build =="

command -v cmake   >/dev/null || { echo "[ERROR] cmake not found"; exit 1; }
command -v gcc     >/dev/null || { echo "[ERROR] gcc not found";   exit 1; }
command -v make    >/dev/null || { echo "[ERROR] make not found";  exit 1; }
command -v node    >/dev/null || echo "[WARN] node not found"
command -v npm     >/dev/null || echo "[WARN] npm not found"

NPROC="$(nproc 2>/dev/null || echo 4)"

echo "[1/4] Configuring CMake (Release) in ${BUILD_DIR}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

echo "[2/4] Compiling kernel + developer tools"
cmake --build "${BUILD_DIR}" --target arcore alrios armake -- -j"${NPROC}"
ln -sf arcore/alrios "${ROOT}/alrios"
chmod +x "${ROOT}/arcore/alrios" "${ROOT}/arcore/armake" "${ROOT}/arcore/arcore" "${ROOT}/alrios" 2>/dev/null || true

echo "[3/4] Packaging all apps modularly via armake"
for appdir in "${ROOT}"/src/apps/*/; do
    if [ -d "${appdir}" ]; then
        appname="$(basename "${appdir}")"
        echo "--> Building modular app: ${appname}..."
        "${ROOT}/arcore/armake" build "${appdir}" "${ROOT}/arcore/apps/${appname}.arapp" || {
            echo "[WARN] Could not pack ${appname}, checking arappmake..."
        }
    fi
done

echo "[4/4] Validating generated .arapp packages:"
ls -lh "${ROOT}/arcore/apps/"*.arapp

echo ""
echo "== OK. Linux ecosystem ready in ${ROOT}/arcore"
echo "   Run with: ./alrios power on   (or ./arcore in foreground to view logs)"
