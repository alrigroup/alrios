#!/bin/bash
# ==============================================================================
# ALRIOS ECOSYSTEM: Master Process Supervisor
# Starts all 11 Sovereign Backend APIs + all Web Frontend SPAs
# ==============================================================================

BASE="/mnt/HD/ALRIGROUP/local/alrios/arcore/.staging"

# 1. Kill old instances
echo "[SUPERVISOR] Stopping existing daemons..."
pkill -9 -f "arauth/arauth" || true
pkill -9 -f arapiauth || true
pkill -9 -f arapilogs || true
pkill -9 -f arapiwork || true
pkill -9 -f arapibus || true
pkill -9 -f arapiconn || true
pkill -9 -f arapidash || true
pkill -9 -f arapichat || true
pkill -9 -f arapicloud || true
pkill -9 -f arapistock || true
pkill -9 -f arapictrl || true

pkill -9 -f home_web || true
pkill -9 -f arwork_web || true
pkill -9 -f arbus_web || true
pkill -9 -f archat_web || true
pkill -9 -f ardash_web || true
pkill -9 -f arconn_web || true
pkill -9 -f arcloud_web || true
pkill -9 -f arstock_web || true
pkill -9 -f arctrl_web || true
pkill -9 -f arlogs_web || true

sleep 1

# 2. Start Sovereign Vault & SSO
echo "[SUPERVISOR] Starting ARAUTH & ARAPIAUTH..."
(cd "$BASE/arauth" && setsid ./arauth > /tmp/arauth.log 2>&1 &)
sleep 1
(cd "$BASE/arapiauth" && setsid ./arapiauth > /tmp/arapiauth.log 2>&1 &)
(cd "$BASE/arapilogs" && setsid ./arapilogs > /tmp/arapilogs.log 2>&1 &)

# 3. Start Business & Operation APIs
echo "[SUPERVISOR] Starting Application Backend APIs..."
(cd "$BASE/arapiwork" && setsid ./arapiwork > /tmp/arapiwork.log 2>&1 &)
(cd "$BASE/arapibus" && setsid ./arapibus > /tmp/arapibus.log 2>&1 &)
(cd "$BASE/arapichat" && setsid ./arapichat > /tmp/arapichat.log 2>&1 &)
(cd "$BASE/arapidash" && setsid ./arapidash > /tmp/arapidash.log 2>&1 &)
(cd "$BASE/arapistock" && setsid ./arapistock > /tmp/arapistock.log 2>&1 &)
(cd "$BASE/arapiconn" && setsid ./arapiconn > /tmp/arapiconn.log 2>&1 &)
(cd "$BASE/arapicloud" && setsid ./arapicloud > /tmp/arapicloud.log 2>&1 &)
(cd "$BASE/arapictrl" && setsid ./arapictrl > /tmp/arapictrl.log 2>&1 &)

# 4. Start Web Frontend SPAs
echo "[SUPERVISOR] Starting Web Frontend SPAs..."
(cd "$BASE/home-web" && setsid ./home_web > /tmp/home_web.log 2>&1 &)
(cd "$BASE/arwork-web" && setsid ./arwork_web > /tmp/arwork_web.log 2>&1 &)
(cd "$BASE/arbus-web" && setsid ./arbus_web > /tmp/arbus_web.log 2>&1 &)
(cd "$BASE/archat-web" && setsid ./archat_web > /tmp/archat_web.log 2>&1 &)
(cd "$BASE/ardash-web" && setsid ./ardash_web > /tmp/ardash_web.log 2>&1 &)
(cd "$BASE/arconn-web" && setsid ./arconn_web > /tmp/arconn_web.log 2>&1 &)
(cd "$BASE/arcloud-web" && setsid ./arcloud_web > /tmp/arcloud_web.log 2>&1 &)
(cd "$BASE/arstock-web" && setsid ./arstock_web > /tmp/arstock_web.log 2>&1 &)
(cd "$BASE/arctrl-web" && setsid ./arctrl_web > /tmp/arctrl_web.log 2>&1 &)
(cd "$BASE/arlogs-web" && setsid ./arlogs_web > /tmp/arlogs_web.log 2>&1 &)

sleep 2
echo "[SUPERVISOR] All sovereign services online!"
