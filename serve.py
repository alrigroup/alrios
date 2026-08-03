#!/usr/bin/env python3
# Copyright (c) ALRIGROUP and its affiliates.
#
# This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
# found in the LICENSE file in the root directory of this source tree
# and at: https://github.com/alrigroup/licenses/tree/main
# -*- coding: utf-8 -*-
"""Start arcore in TEST mode and LEAVE IT RUNNING so http://localhost:8080 works.

Unlike run_test.py this script does NOT stop the services or restore the
production config afterwards. To stop and go back to production later:

    python stop_serve.py
"""
import http.client
import os
import shutil
import socket
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.abspath(__file__))
ARWS = os.path.join(ROOT, "arcore")
CFG = os.path.join(ARWS, "storage", "arws", "arws.cfg")
BAKCFG = os.path.join(ARWS, "storage", "arws", "arws.cfg.production")
LOG = os.path.join(ARWS, "persist_serve.log")

TEST_CFG_CONTENT = (
    "# ALRI Web Services Config - TEST mode (persistent, for local browsing)\n"
    "mode=test\n"
    "port=8080\n"
    "bind=127.0.0.1\n"
    "global_mode=production\n"
    "cache_ttl=0\n"
)


def run(cmd):
    return subprocess.run(cmd, shell=True, capture_output=True, text=True)


def port_open(port, host="127.0.0.1"):
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(0.5)
    try:
        s.connect((host, port))
        return True
    except OSError:
        return False
    finally:
        s.close()


def wait_port(port, timeout):
    t0 = time.time()
    while time.time() - t0 < timeout:
        if port_open(port):
            return True
        time.sleep(0.3)
    return False


def stop_all():
    for name in ("arcore.exe", "node.exe", "home.web.exe", "detroit.web.exe",
                 "projetoliteratura.web.exe"):
        run("taskkill /f /im %s >nul 2>nul" % name)


def http_get(port, path, host):
    conn = http.client.HTTPConnection("127.0.0.1", port, timeout=20)
    conn.request("GET", path, headers={"Host": host})
    r = conn.getresponse()
    body = r.read()
    conn.close()
    return r.status, body


def check(label, port, path, host, marker="ECOSYSTEM_MONITOR"):
    try:
        st, body = http_get(port, path, host)
        text = body.decode("utf-8", "ignore")
        ok = st == 200 and marker in text
        mark = "OK" if ok else "FAIL"
        print("      [%s] %s -> HTTP %d len=%d marker=%s" % (
            mark, label, st, len(body), marker in text))
        return ok
    except Exception as e:
        print("      [FAIL] %s -> %s" % (label, e))
        return False


def main():
    print("== ALRI home.web persistent server (arcore TEST mode) ==")
    try:
        print("[1/5] stopping old processes...")
        stop_all()
        time.sleep(1)

        print("[2/5] ensuring arws.cfg is in TEST mode...")
        with open(CFG, "r", encoding="utf-8") as f:
            current = f.read()
        if "mode=test" in current:
            print("      arws.cfg already TEST (leaving as-is)")
        else:
            shutil.copy(CFG, BAKCFG)
            with open(CFG, "w", encoding="utf-8") as f:
                f.write(TEST_CFG_CONTENT)
            print("      switched to TEST; production saved to arws.cfg.production")

        print("[3/5] starting arcore (background, no console)...")
        flags = subprocess.CREATE_NO_WINDOW
        logf = open(LOG, "wb")
        proc = subprocess.Popen(
            [os.path.join(ARWS, "arcore.exe")],
            cwd=ARWS, stdout=logf, stderr=subprocess.STDOUT,
            creationflags=flags)
        print("      arcore pid=%d" % proc.pid)

        print("[4/5] waiting for gateway/admin/apps...")
        ok_gw = wait_port(8080, 30)
        ok_adm = wait_port(9500, 30)
        ok_home = wait_port(3001, 30)
        ok_detroit = wait_port(3004, 30)
        ok_projeto = wait_port(3003, 30)
        print("      gateway 8080 up=%s  admin 9500 up=%s  home.web 3001 up=%s"
              % (ok_gw, ok_adm, ok_home))
        print("      detroit.web 3004 up=%s  projetoliteratura.web 3003 up=%s"
              % (ok_detroit, ok_projeto))
        if not (ok_gw and ok_home):
            print("[5/5] FAILED to start services; log tail:")
            try:
                with open(LOG, "r", encoding="utf-8", errors="replace") as f:
                    for ln in f.readlines()[-40:]:
                        print("      " + ln.rstrip())
            except Exception as e:
                print("      (log read error: %s)" % e)
            return 1

        print("[5/5] verifying through the gateway (browser-like Host headers)...")
        passed = failed = 0
        for host in ("localhost", "localhost:8080", "alrigroup.com"):
            if check("gateway 8080 / (Host %s)" % host, 8080, "/", host,
                     "ALRI Group"):
                passed += 1
            else:
                failed += 1

        detroit_hosts = ("detroitgg.alrigroup.com", "detroit.localhost")
        for host in detroit_hosts:
            for path, marker in (("/", "Detroit Roleplay"),
                                 ("/regras", "Detroit Roleplay"),
                                 ("/regimentostaff", "Detroit Roleplay"),
                                 ("/logo.png", "")):
                ok = check("detroit %s (Host %s)" % (path, host), 8080, path,
                           host, marker)
                if ok:
                    passed += 1
                else:
                    failed += 1
            try:
                st, body = http_get(8080, "/nao-existe", host)
                ok = st == 404 and "Detroit Roleplay" in body.decode("utf-8", "ignore")
                mark = "OK" if ok else "FAIL"
                print("      [%s] detroit 404 /nao-existe (Host %s) -> HTTP %d" % (
                    mark, host, st))
                if ok:
                    passed += 1
                else:
                    failed += 1
            except Exception as e:
                print("      [FAIL] detroit 404 (Host %s) -> %s" % (host, e))
                failed += 1

        for host in ("alexsander.alrigroup.com", "alexsander.localhost"):
            ok = check("projetoliteratura /projetoliteratura (Host %s)" % host,
                       8080, "/projetoliteratura", host, "Machado de Assis")
            if ok:
                passed += 1
            else:
                failed += 1

        print("      passed=%d failed=%d" % (passed, failed))

        print()
        print("== RODANDO! Abra no navegador: http://localhost:8080 ==")
        print("   arcore ficou em background (pid=%d, sem console)." % proc.pid)
        print("   Para parar e restaurar a config de producao:  python stop_serve.py")
        if failed:
            print("   ATENCAO: alguns checks falharam - veja o log acima.")
            return 1
        return 0
    finally:
        logf = None


if __name__ == "__main__":
    sys.exit(main())
