#!/bin/bash
# Start arauth (vault) and all 7 sovereign ARApi services
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

sleep 1

setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arauth/arauth > /tmp/arauth.log 2>&1 &
sleep 1
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapiauth/arapiauth > /tmp/arapiauth.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapilogs/arapilogs > /tmp/arapilogs.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapiwork/arapiwork > /tmp/arapiwork.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapibus/arapibus > /tmp/arapibus.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapiconn/arapiconn > /tmp/arapiconn.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapidash/arapidash > /tmp/arapidash.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapichat/arapichat > /tmp/arapichat.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapicloud/arapicloud > /tmp/arapicloud.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapistock/arapistock > /tmp/arapistock.log 2>&1 &
setsid /mnt/HD/ALRIGROUP/local/alrios/arcore/.staging/arapictrl/arapictrl > /tmp/arapictrl.log 2>&1 &

sleep 2
