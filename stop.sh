#!/bin/bash
# Copyright (c) ALRIGROUP and its affiliates.
#
# This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
# found in the LICENSE file in the root directory of this source tree
# and at: https://github.com/alrigroup/licenses/tree/main
echo "=== Stopping ALRIOS (arcore) ==="

# 1. Native ALRIOS daemons/tools — exact process-name match ONLY (-x).
#    -x matches the process name exactly, never unrelated processes.
pkill -9 -x arcore 2>/dev/null || true
pkill -9 -x armake 2>/dev/null || true
pkill -9 -x arinstall 2>/dev/null || true
pkill -9 -x arproxy 2>/dev/null || true

# 2. Node app adapters — matched by the unique ALRIOS paths they run from.
pkill -9 -f "/arcore/programfiles/" 2>/dev/null || true
pkill -9 -f "/arcore/src/apps/" 2>/dev/null || true
pkill -9 -f "/arcore/run/" 2>/dev/null || true

# 3. IPC/admin port 9500 — free it only if arcore itself still holds it.
if command -v lsof &>/dev/null; then
    for pid in $(lsof -t -iTCP:9500 -sTCP:LISTEN 2>/dev/null); do
        exe=$(readlink -f "/proc/$pid/exe" 2>/dev/null || true)
        case "$exe" in
            *"/arcore/arcore"*) kill -9 "$pid" 2>/dev/null || true ;;
        esac
    done
fi

sleep 0.3
echo "OK: ALRIOS stopped."
