#!/usr/bin/env bash
# Copyright (c) 2026 ALRIGROUP and its affiliates.
# Engineered and maintained by ALRI Development.
#
# This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
# found in the LICENSE file in the root directory of this source tree.
# Build the ALRIOS sovereign kernel, supervisor, and developer tools for Linux.
# Includes: arcore (supervisor), arkernel (HAL), alrios (CLI), armake (packager), arpm.
# Usage:
#   bash build_linux.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT}/build-linux"

echo "== ALRIOS — Sovereign Linux Kernel & Tools Build =="

command -v cmake >/dev/null || { echo "[ERROR] cmake not found"; exit 1; }
command -v gcc   >/dev/null || { echo "[ERROR] gcc not found";   exit 1; }
command -v make  >/dev/null || { echo "[ERROR] make not found";  exit 1; }

NPROC="$(nproc 2>/dev/null || echo 4)"

echo "[1/2] Configuring CMake (Release) in ${BUILD_DIR}"
cmake -S "${ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release

echo "[2/2] Compiling sovereign kernel + developer tools"
cmake --build "${BUILD_DIR}" --target arcore alrios armake -- -j"${NPROC}"

ln -sf arcore/alrios "${ROOT}/alrios"
ln -sf alrios "${ROOT}/arcore/arpm"
chmod +x "${ROOT}/arcore/alrios" "${ROOT}/arcore/arpm" "${ROOT}/arcore/armake" "${ROOT}/arcore/arcore" "${ROOT}/alrios" 2>/dev/null || true

echo ""
echo "== OK. ALRIOS core binaries ready in ${ROOT}/arcore"
echo "   Run supervisor: ./alrios power on"
echo "   Inspect status: ./alrios status"
