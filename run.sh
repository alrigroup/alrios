#!/bin/bash
# Copyright (c) ALRIGROUP and its affiliates.
#
# This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
# found in the LICENSE file in the root directory of this source tree
# and at: https://github.com/alrigroup/licenses/tree/main
DIR="$(cd "$(dirname "$0")" && pwd)"

# Always kill old processes and free ports dynamically before starting
"$DIR/stop.sh"

cd "$DIR/arcore" || { echo "arcore/ not found"; exit 1; }

# Auto-generate self-signed SSL certs if missing
if [ ! -f storage/arws/certs/cert.pem ] || [ ! -f storage/arws/certs/key.pem ]; then
    echo "=== Auto-generating SSL certificates (storage/arws/certs/) ==="
    mkdir -p storage/arws/certs
    if command -v openssl &>/dev/null; then
        openssl req -x509 -newkey rsa:2048 -keyout storage/arws/certs/key.pem -out storage/arws/certs/cert.pem -days 365 -nodes -subj "/CN=localhost" 2>/dev/null || true
    fi
fi

# Check if any privileged port (<1024) is in use or configured
NEEDS_ROOT=0
if grep -qE '^port=(80|443)$' storage/arws/arws.cfg 2>/dev/null; then
    NEEDS_ROOT=1
fi
if [ "$MODE" = "production" ] || grep -q '^mode=production$' storage/arws/arws.cfg 2>/dev/null; then
    if ! grep -qE '^port=(8080|[1-9][0-9]{4,})$' storage/arws/arws.cfg 2>/dev/null; then
        NEEDS_ROOT=1
    fi
fi

if [ $NEEDS_ROOT -eq 1 ] && [ "$(id -u)" -ne 0 ]; then
    echo "Root privileges required to bind to privileged ports (80/443)."
    exec sudo -E ARWS_STAY_ROOT=1 "$0" "$@"
fi

export ARWS_STAY_ROOT=1

./arcore "$@" &
ARWS_PID=$!

# When started via sudo (root), hand generated files back to the invoking
# user: configs (arws.cfg), runtimes, extracted apps and staging. Without
# this the deployment account cannot edit arws.cfg or install runtimes
# (files created by the root arcore process).
if [ "$(id -u)" -eq 0 ] && [ -n "$SUDO_UID" ] && [ -n "$SUDO_GID" ]; then
    for i in $(seq 1 20); do
        sleep 1
        kill -0 "$ARWS_PID" 2>/dev/null || break
    done
    chown -R "$SUDO_UID:$SUDO_GID" storage programfiles run apps system .staging etc 2>/dev/null || true
    echo "ALRIOS: ownership of runtime dirs handed back to ${SUDO_USER:-user}"
fi

wait "$ARWS_PID"