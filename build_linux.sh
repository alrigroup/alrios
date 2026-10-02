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
cmake --build "${BUILD_DIR}" --target arcore alrios armake arsdk ar_auditd ar_resourced ar_chronod ar_cored ar_eventd arinstall alrios-ca arsign arcc -- -j"${NPROC}"

ln -sf alrios-release/alrios "${ROOT}/alrios"
chmod +x "${ROOT}/alrios-release/alrios" "${ROOT}/alrios-release/bin/"* "${ROOT}/alrios-release/libexec/"* "${ROOT}/alrios" 2>/dev/null || true

echo ""
echo "== OK. ALRIOS core binaries ready in ${ROOT}/alrios-release"
echo "   Run supervisor: ./alrios power on"
echo "   Inspect status: ./alrios status"
