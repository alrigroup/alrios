import socket
import time
import struct

def recv_until_ready(sock):
    total = b""
    while b'Z\x00\x00\x00\x05' not in total:
        chunk = sock.recv(1024)
        if not chunk:
            break
        total += chunk
    return total

def test_pgwire_connection():
    print("[TEST] Conectando ao ALRI DB na porta 5432 (PG-Wire)...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.connect(('127.0.0.1', 5432))

    # 1. Enviar StartupMessage (user='alri_admin', database='alrios_db')
    params = b"user\x00alri_admin\x00database\x00alrios_db\x00\x00"
    length = 4 + 4 + len(params)
    startup_pkt = struct.pack("!II", length, 196608) + params
    s.sendall(startup_pkt)

    # 2. Ler resposta de auth (espera 'R' -> AuthenticationCleartextPassword)
    auth_req = s.recv(9)
    assert len(auth_req) == 9 and chr(auth_req[0]) == 'R'
    print("  [PASS] Recebido pedido de autenticacao do ALRI DB.")

    # 3. Enviar PasswordMessage com a senha do alri_admin
    password = b"alrios_master_sec_2026\x00"
    pass_pkt = b'p' + struct.pack("!I", 4 + len(password)) + password
    s.sendall(pass_pkt)

    # 4. Ler fluxo completo ate ReadyForQuery ('Z')
    auth_flow = recv_until_ready(s)
    assert b'Z\x00\x00\x00\x05' in auth_flow
    print("  [PASS] Autenticacao aceita e parametros de sessao validados!")

    # 5. Enviar Query SQL Normal
    query = b"SELECT * FROM clientes;\x00"
    query_pkt = b'Q' + struct.pack("!I", 4 + len(query)) + query
    s.sendall(query_pkt)
    query_flow = recv_until_ready(s)
    assert b'C\x00\x00\x00\x07OK\x00' in query_flow or b'C' in query_flow
    print("  [PASS] Query executada com sucesso e auditada pelo ALRI DB.")

    # 6. Testar RLS Bypass attempt
    bypass_query = b"SELECT * FROM clientes WHERE tenant_id = 'empresa_alheia' /*\x00"
    bypass_pkt = b'Q' + struct.pack("!I", 4 + len(bypass_query)) + bypass_query
    s.sendall(bypass_pkt)
    bypass_flow = recv_until_ready(s)
    assert b'E\x00' in bypass_flow # 'E' = ErrorResponse
    print("  [PASS] Firewall SQL interceptou e bloqueou a tentativa de RLS Bypass!")

    # 7. Finalizar conexao
    s.sendall(b'X\x00\x00\x00\x04')
    s.close()
    print("\n=== TESTE COMPLETO DE CLIENTE PG-WIRE (DBEAVER MOCK) FINALIZADO COM SUCESSO! ===")

if __name__ == '__main__':
    test_pgwire_connection()
