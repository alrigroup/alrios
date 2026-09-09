#!/usr/bin/env bash
# Copyright (c) 2026 ALRIGROUP and its affiliates.
# Engineered and maintained by ALRI Development.
#
# This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
# found in the LICENSE file in the root directory of this source tree.
set -euo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Always stop old instances before bootstrapping
"$DIR/stop.sh"

cd "$DIR/arcore" || { echo "[ERROR] arcore directory not found"; exit 1; }

if [ ! -x "./arcore" ]; then
    echo "[ERROR] arcore binary not found. Please compile first via: bash build_linux.sh"
    exit 1
fi

echo "=== Starting ALRIOS Supervisor Daemon (arcore) ==="

./arcore "$@" &
ARCORE_PID=$!

# Hand ownership back to invoking sudo user if executed via sudo
if [ "$(id -u)" -eq 0 ] && [ -n "${SUDO_UID:-}" ] && [ -n "${SUDO_GID:-}" ]; then
    for i in $(seq 1 10); do
        sleep 0.5
        kill -0 "$ARCORE_PID" 2>/dev/null || break
    done
    chown -R "$SUDO_UID:$SUDO_GID" storage programfiles run apps system .staging etc 2>/dev/null || true
fi

wait "$ARCORE_PID"
