#!/usr/bin/env bash
# ==============================================================================
# ALRIOS — Sovereign Operating System & Microkernel Ecosystem
# Unified One-Click Launcher for ALRIOS Master Installer (Linux)
# ==============================================================================
set -e

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Build arinstall if not present
if [ ! -x "${DIR}/arcore/arinstall" ]; then
    echo "-> Compilando arinstall local..."
    cmake -B "${DIR}/build-linux" -S "${DIR}" && cmake --build "${DIR}/build-linux" --target arinstall
fi

chmod +x "${DIR}/arcore/arinstall" 2>/dev/null || true
exec "${DIR}/arcore/arinstall" "$@"
