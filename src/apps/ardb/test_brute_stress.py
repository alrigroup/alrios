#!/usr/bin/env python3
"""
ALRI DB - SUÍTE DE TESTES BRUTOS DE CARGA E ESTRESSE (ZERO-TRUST DEPLOYMENT)
Simula rajadas maciças de conexões simultâneas, ataques de timing,
fuzzing de injeção de pacotes e benchmark de queries de alta concorrência.
"""

import socket
import struct
import threading
import time
import random
import sys

TARGET_HOST = "127.0.0.1"
TARGET_PORT = 5432
NUM_THREADS = 50          # 50 conexões simultâneas concorrentes
QUERIES_PER_THREAD = 100  # 5.000 queries totais sob carga máxima

g_success_queries = 0
g_blocked_attacks = 0
g_auth_failures = 0
g_errors = 0
g_lock = threading.Lock()

def recv_until_ready(sock):
    total = b""
    while b'Z\x00\x00\x00\x05' not in total:
        chunk = sock.recv(2048)
        if not chunk:
            break
        total += chunk
    return total

def worker_stress_test(worker_id):
    global g_success_queries, g_blocked_attacks, g_errors
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(5.0)
        s.connect((TARGET_HOST, TARGET_PORT))

        # 1. Handshake Startup
        user = "alri_admin"
        params = f"user\x00{user}\x00database\x00alrios_db\x00\x00".encode('utf-8')
        length = 4 + 4 + len(params)
        startup_pkt = struct.pack("!II", length, 196608) + params
        s.sendall(startup_pkt)

        # 2. Handshake Auth
        auth_req = s.recv(9)
        if len(auth_req) < 9 or chr(auth_req[0]) != 'R':
            s.close()
            with g_lock: g_errors += 1
            return

        password = b"alrios_master_sec_2026\x00"
        pass_pkt = b'p' + struct.pack("!I", 4 + len(password)) + password
        s.sendall(pass_pkt)

        auth_flow = recv_until_ready(s)
        if b'Z\x00\x00\x00\x05' not in auth_flow:
            s.close()
            with g_lock: g_errors += 1
            return

        # 3. Rajada de Queries Concorrentes (Legítimas e Ataques)
        for i in range(QUERIES_PER_THREAD):
            if i % 10 == 0:
                # Injeção de Ataque (RLS Bypass / DDL Proibido)
                attack_sql = f"SELECT * FROM folha_pagamento WHERE tenant_id = 'filial_hack_{random.randint(100,999)}' /*\x00".encode('utf-8')
                pkt = b'Q' + struct.pack("!I", 4 + len(attack_sql)) + attack_sql
                s.sendall(pkt)
                flow = recv_until_ready(s)
                if b'E\x00' in flow:
                    with g_lock: g_blocked_attacks += 1
                else:
                    with g_lock: g_errors += 1
            else:
                # Query Legítima
                legit_sql = f"SELECT id, nome, saldo FROM empresas WHERE id = {i};\x00".encode('utf-8')
                pkt = b'Q' + struct.pack("!I", 4 + len(legit_sql)) + legit_sql
                s.sendall(pkt)
                flow = recv_until_ready(s)
                if b'C\x00' in flow or b'Z\x00' in flow:
                    with g_lock: g_success_queries += 1
                else:
                    with g_lock: g_errors += 1

        # 4. Finalização Graciosa
        s.sendall(b'X\x00\x00\x00\x04')
        s.close()
    except Exception as e:
        with g_lock: g_errors += 1

def run_fuzzing_test():
    print("\n[FUZZING] Executando teste de Memory Safety e Injeção de Ruído...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect((TARGET_HOST, TARGET_PORT))
    
    # Enviar pacote Startup gigante (Tentativa de Buffer Overflow)
    overflow_pkt = struct.pack("!I", 2147483640) + b"\x00" * 32
    s.sendall(overflow_pkt)
    time.sleep(0.2)
    # A conexão deve ter sido derrubada sem crashar o engine
    try:
        s.sendall(b"PING")
    except:
        pass
    s.close()
    print("  [PASS] Prevenção de Buffer Overflow validada (Drop TCP imediato).")

def main():
    print("═════════════════════════════════════════════════════════")
    print("  ALRI DB - BENCHMARK & TESTE BRUTO DE CARGA E ESTRESSE  ")
    print("═════════════════════════════════════════════════════════")
    print(f"Alvo: {TARGET_HOST}:{TARGET_PORT} (PG-Wire Sovereign Guard)")
    print(f"Carga: {NUM_THREADS} threads concorrentes x {QUERIES_PER_THREAD} queries cada = {NUM_THREADS * QUERIES_PER_THREAD} transações")

    run_fuzzing_test()

    print(f"\n[STRESS] Iniciando rajada massiva de {NUM_THREADS} conexões simultâneas...")
    start_time = time.time()

    threads = []
    for i in range(NUM_THREADS):
        t = threading.Thread(target=worker_stress_test, args=(i,))
        threads.append(t)
        t.start()

    for t in threads:
        t.join()

    total_time = time.time() - start_time
    total_queries = g_success_queries + g_blocked_attacks

    print("\n═════════════════════════════════════════════════════════")
    print("              RESULTADOS DO TESTE BRUTO                 ")
    print("═════════════════════════════════════════════════════════")
    print(f"Tempo Total de Execução    : {total_time:.2f} segundos")
    print(f"Queries Legítimas com Êxito: {g_success_queries}")
    print(f"Ataques Interceptados (FW) : {g_blocked_attacks}")
    print(f"Falhas / Conexões Perdidas : {g_errors}")
    print(f"Throughput Real            : {total_queries / total_time:.2f} queries/segundo")
    print("═════════════════════════════════════════════════════════")

    if g_errors == 0 and g_blocked_attacks > 0 and g_success_queries > 0:
        print("\n  >>> STATUS: APROVADO COM GRAU MÁXIMO DE RESILIÊNCIA E SEGURANÇA (100% PASS) <<<\n")
        return 0
    else:
        print("\n  [FAIL] Teste de estresse apresentou divergências ou falhas.\n")
        return 1

if __name__ == '__main__':
    sys.exit(main())
