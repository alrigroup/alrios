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
command -v node    >/dev/null || { echo "[ERROR] node not found";  exit 1; }
command -v npm     >/dev/null || { echo "[ERROR] npm not found";   exit 1; }

NPROC="$(nproc 2>/dev/null || echo 4)"

echo "[1/4] Configuring CMake (Release) in ${BUILD_DIR}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

echo "[2/4] Compiling kernel + developer tools"
cmake --build "${BUILD_DIR}" --target arcore alrios armake -- -j"${NPROC}"

echo "[3/4] Compiling + packaging native apps"
rm -rf "${ROOT}"/src/apps/*/web/node_modules "${ROOT}"/src/apps/*/node_modules 2>/dev/null || true
cmake --build "${BUILD_DIR}" \
    --target arws_pack home_web_pack detroit_web_pack \
    -- -j"${NPROC}"

echo "[4/4] Validating generated .arapp packages:"
for app in arws home.web detroit.web; do
    file="${ROOT}/arcore/apps/${app}.arapp"
    if [ -f "${file}" ]; then
        echo "--- ${file}"
        "${ROOT}/arcore/armake" list "${file}"
    else
        echo "[ERROR] ${file} not generated"
        exit 1
    fi
done

echo ""
echo "== OK. Linux ecosystem ready in ${ROOT}/arcore"
echo "   Run with: ./alrios power on   (or ./arcore in foreground to view logs)"
