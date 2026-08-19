/*
 * Suíte de Testes de Segurança Zero-Trust do ALRI DB
 * Conforme especificado em docs/ALRI_DB_SECURITY_TESTS.md
 */

#include "ardb_pgwire.h"
#include "ardb_auth.h"
#include "ardb_firewall.h"
#include "ardb_audit.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>

int main(void) {
    printf("=========================================================\n");
    printf("  ALRI DB - SUÍTE DE TESTES DE SEGURANÇA (ZERO-TRUST)   \n");
    printf("=========================================================\n\n");

    /* 1. DOMÍNIO 3: AUTENTICAÇÃO, TOKENS EFÊMEROS & TIMING ATTACKS */
    printf("[TEST-3.1 & 3.2] Testando Autenticação e Tokens Efêmeros...\n");
    ardb_auth_init();
    ardb_auth_add_user("test_user", "sec_pass_123", "empresa_holding", "operator");

    char token[128];
    int r = ardb_auth_generate_token("test_user", "sec_pass_123", NULL, 2, token, sizeof(token));
    assert(r == 0);
    printf("  Token gerado com sucesso: %s\n", token);

    char val_user[64], val_tenant[64], val_role[32];
    r = ardb_auth_verify_token(token, val_user, val_tenant, val_role);
    assert(r == 0);
    assert(strcmp(val_user, "test_user") == 0);
    assert(strcmp(val_tenant, "empresa_holding") == 0);
    printf("  [PASS] Token verificado com identidade de tenant e role.\n");

    /* Testar revogação de token */
    ardb_auth_revoke_token(token);
    r = ardb_auth_verify_token(token, val_user, val_tenant, val_role);
    assert(r != 0);
    printf("  [PASS] Revogacao imediata de token validada com sucesso.\n\n");

    /* 2. DOMÍNIO 4: FIREWALL SQL & RLS MULTI-TENANT */
    printf("[TEST-4.1 & 4.2] Testando Firewall SQL e Bloqueio de RLS Bypass...\n");
    char rewritten[2048];
    char reason[512];

    /* Tentativa de comando destrutivo por operador */
    ArdbFwAction act = ardb_firewall_inspect("DROP TABLE faturamento;", "empresa_holding", "operator",
                                            rewritten, sizeof(rewritten), reason, sizeof(reason));
    assert(act == ARDB_FW_BLOCK_DESTRUCTIVE);
    printf("  [PASS] Comando DROP TABLE bloqueado pelo Firewall: %s\n", reason);

    /* Tentativa de bypass de RLS */
    act = ardb_firewall_inspect("SELECT * FROM clientes WHERE tenant_id = 'empresa_alheia' /*",
                                "empresa_holding", "operator",
                                rewritten, sizeof(rewritten), reason, sizeof(reason));
    assert(act == ARDB_FW_BLOCK_RLS_BYPASS);
    printf("  [PASS] Tentativa de RLS Bypass bloqueada com sucesso: %s\n", reason);

    /* Query válida com injeção automática de contexto */
    act = ardb_firewall_inspect("SELECT id, nome FROM clientes;", "empresa_holding", "operator",
                                rewritten, sizeof(rewritten), reason, sizeof(reason));
    assert(act == ARDB_FW_OK);
    assert(strstr(rewritten, "SET LOCAL alri.tenant_id = 'empresa_holding';") != NULL);
    printf("  [PASS] Query reescrita com seguranca: %s\n\n", rewritten);

    /* 3. DOMÍNIO 5: AUDITORIA IMUTÁVEL (HASH EM CADEIA BLOCKCHAIN-LIKE) */
    printf("[TEST-5.1 & 5.2] Testando Auditoria Forense com Hash em Cadeia...\n");
    const char *test_log = "build/test_audit.log";
    remove(test_log);
    ardb_audit_init(test_log);

    ardb_audit_log_query("alex", "holding", "127.0.0.1", "SELECT 1;", 200, 150);
    ardb_audit_log_query("alex", "holding", "127.0.0.1", "SELECT 2;", 200, 120);
    ardb_audit_log_query("alex", "holding", "127.0.0.1", "SELECT 3;", 200, 180);

    char err_msg[256];
    int verify_ret = ardb_audit_verify_integrity(test_log, err_msg, sizeof(err_msg));
    assert(verify_ret == 0);
    printf("  [PASS] Cadeia de 3 hashes validada com 100%% de integridade.\n");

    /* Simular corrupção maliciosa do arquivo por invasor */
    FILE *f = fopen(test_log, "r+");
    if (f) {
        fseek(f, 0, SEEK_SET);
        fputs("ts=999999999", f);
        fclose(f);
    } else {
        printf("  [ERROR] Nao foi possivel abrir %s para corromper\n", test_log);
    }

    err_msg[0] = '\0';
    verify_ret = ardb_audit_verify_integrity(test_log, err_msg, sizeof(err_msg));
    assert(verify_ret == -1);
    printf("  [PASS] Detecao de adulteracao maliciosa funcionou perfeitamente: %s\n\n", err_msg);

    ardb_audit_cleanup();
    ardb_auth_cleanup();
    remove(test_log);

    printf("=========================================================\n");
    printf("  TODOS OS TESTES DE SEGURANCA DO ALRI DB PASSARAM (100%%) \n");
    printf("=========================================================\n");
    return 0;
}
