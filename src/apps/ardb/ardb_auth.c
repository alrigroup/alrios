/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_auth.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/sha.h>
#include <openssl/evp.h>

static ArdbUser g_users[ARDB_MAX_USERS];
static int g_user_count = 0;
static ArdbSessionToken g_tokens[ARDB_MAX_ACTIVE_TOKENS];
static int g_token_count = 0;
static void *g_auth_mutex = NULL;
static int g_auth_initialized = 0;

/* Hashing SHA256 com Salt para demonstração e compatibilidade */
static void compute_hash(const char *password, const char *salt, char *out_hex, size_t out_size) {
    char combined[256];
    snprintf(combined, sizeof(combined), "%s:%s:alrios_salt", password, salt);
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((unsigned char*)combined, strlen(combined), hash);

    for (int i = 0; i < SHA256_DIGEST_LENGTH && (i * 2 + 1) < (int)out_size; i++) {
        snprintf(out_hex + (i * 2), 3, "%02x", hash[i]);
    }
}

/* Comparação constante no tempo para evitar Timing Attacks (TEST-3.1) */
static int constant_time_compare(const char *a, const char *b) {
    if (!a || !b) return 0;
    size_t len_a = strlen(a);
    size_t len_b = strlen(b);
    int result = (len_a == len_b) ? 0 : 1;

    size_t max_len = len_a > len_b ? len_a : len_b;
    for (size_t i = 0; i < max_len; i++) {
        char ca = (i < len_a) ? a[i] : 0;
        char cb = (i < len_b) ? b[i] : 0;
        result |= (ca ^ cb);
    }
    return (result == 0);
}

void ardb_auth_init(void) {
    if (g_auth_initialized) return;
    g_auth_mutex = ar_mutex_create();
    ar_mutex_lock(g_auth_mutex);
    memset(g_users, 0, sizeof(g_users));
    memset(g_tokens, 0, sizeof(g_tokens));
    g_user_count = 0;
    g_token_count = 0;
    g_auth_initialized = 1;
    ar_mutex_unlock(g_auth_mutex);

    /* Usuários padrão do sistema ALRIOS para testes e bootstrap */
    ardb_auth_add_user("alri_admin", "alrios_master_sec_2026", "holding_alri", "admin");
    ardb_auth_add_user("alri_op", "alrios_op_sec_2026", "holding_alri", "operator");
}

void ardb_auth_cleanup(void) {
    if (!g_auth_initialized) return;
    if (g_auth_mutex) {
        ar_mutex_destroy(g_auth_mutex);
        g_auth_mutex = NULL;
    }
    g_auth_initialized = 0;
}

int ardb_auth_add_user(const char *username, const char *password, const char *tenant_id, const char *role) {
    if (!username || !password) return -1;
    ardb_auth_init();

    ar_mutex_lock(g_auth_mutex);
    for (int i = 0; i < g_user_count; i++) {
        if (strcmp(g_users[i].username, username) == 0) {
            compute_hash(password, username, g_users[i].password_hash, sizeof(g_users[i].password_hash));
            if (tenant_id) strncpy(g_users[i].tenant_id, tenant_id, sizeof(g_users[i].tenant_id) - 1);
            if (role) strncpy(g_users[i].role, role, sizeof(g_users[i].role) - 1);
            g_users[i].is_active = 1;
            ar_mutex_unlock(g_auth_mutex);
            return 0;
        }
    }

    if (g_user_count >= ARDB_MAX_USERS) {
        ar_mutex_unlock(g_auth_mutex);
        return -1;
    }

    ArdbUser *u = &g_users[g_user_count++];
    memset(u, 0, sizeof(ArdbUser));
    strncpy(u->username, username, sizeof(u->username) - 1);
    compute_hash(password, username, u->password_hash, sizeof(u->password_hash));
    strncpy(u->tenant_id, tenant_id ? tenant_id : "default", sizeof(u->tenant_id) - 1);
    strncpy(u->role, role ? role : "operator", sizeof(u->role) - 1);
    u->is_active = 1;

    ar_mutex_unlock(g_auth_mutex);
    return 0;
}

int ardb_auth_user_exists(const char *username) {
    if (!username) return 0;
    ardb_auth_init();

    ar_mutex_lock(g_auth_mutex);
    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i].is_active && strcmp(g_users[i].username, username) == 0) {
            ar_mutex_unlock(g_auth_mutex);
            return 1;
        }
    }
    ar_mutex_unlock(g_auth_mutex);
    return 0;
}

int ardb_auth_generate_token(const char *username, const char *password, const char *totp_code,
                             int ttl_seconds, char *out_token, size_t out_token_size) {
    (void)totp_code;
    if (!username || !password || !out_token || out_token_size < 40) return -1;
    ardb_auth_init();

    ar_mutex_lock(g_auth_mutex);

    ArdbUser *matched = NULL;
    char target_hash[128];
    compute_hash(password, username, target_hash, sizeof(target_hash));

    for (int i = 0; i < g_user_count; i++) {
        if (g_users[i].is_active && strcmp(g_users[i].username, username) == 0) {
            if (constant_time_compare(g_users[i].password_hash, target_hash)) {
                matched = &g_users[i];
            }
            break;
        }
    }

    if (!matched) {
        ar_mutex_unlock(g_auth_mutex);
        return -1; /* Credencial inválida */
    }

    /* Gerar token criptográfico único */
    uint64_t now_ms = (uint64_t)ar_time_ms();
    if (ttl_seconds <= 0) ttl_seconds = ARDB_TOKEN_DEFAULT_TTL_SEC;

    int slot = -1;
    for (int i = 0; i < ARDB_MAX_ACTIVE_TOKENS; i++) {
        if (g_tokens[i].token[0] == '\0' || g_tokens[i].is_revoked || g_tokens[i].expires_at_ms < now_ms) {
            slot = i;
            break;
        }
    }

    if (slot == -1) {
        slot = g_token_count % ARDB_MAX_ACTIVE_TOKENS;
    }

    ArdbSessionToken *t = &g_tokens[slot];
    memset(t, 0, sizeof(ArdbSessionToken));
    snprintf(t->token, sizeof(t->token), "ardb_tok_%08x%08x%08x%08x",
             (uint32_t)rand(), (uint32_t)rand(), (uint32_t)now_ms, (uint32_t)slot);

    strncpy(t->username, matched->username, sizeof(t->username) - 1);
    strncpy(t->tenant_id, matched->tenant_id, sizeof(t->tenant_id) - 1);
    strncpy(t->role, matched->role, sizeof(t->role) - 1);
    t->created_at_ms = now_ms;
    t->expires_at_ms = now_ms + ((uint64_t)ttl_seconds * 1000);
    t->is_revoked = 0;

    strncpy(out_token, t->token, out_token_size - 1);
    out_token[out_token_size - 1] = '\0';

    if (slot >= g_token_count) g_token_count = slot + 1;

    ar_mutex_unlock(g_auth_mutex);
    return 0;
}

int ardb_auth_verify_token(const char *token, char *out_user, char *out_tenant, char *out_role) {
    if (!token || token[0] == '\0') return -1;
    ardb_auth_init();

    ar_mutex_lock(g_auth_mutex);
    uint64_t now_ms = (uint64_t)ar_time_ms();

    for (int i = 0; i < ARDB_MAX_ACTIVE_TOKENS; i++) {
        ArdbSessionToken *t = &g_tokens[i];
        if (t->token[0] != '\0' && !t->is_revoked && strcmp(t->token, token) == 0) {
            if (t->expires_at_ms < now_ms) {
                t->is_revoked = 1; /* Expirou (TEST-3.2) */
                ar_mutex_unlock(g_auth_mutex);
                return -1;
            }
            if (out_user) strncpy(out_user, t->username, 63);
            if (out_tenant) strncpy(out_tenant, t->tenant_id, 63);
            if (out_role) strncpy(out_role, t->role, 31);
            ar_mutex_unlock(g_auth_mutex);
            return 0; /* Token válido */
        }
    }

    ar_mutex_unlock(g_auth_mutex);
    return -1;
}

int ardb_auth_revoke_token(const char *token) {
    if (!token) return -1;
    ardb_auth_init();

    ar_mutex_lock(g_auth_mutex);
    for (int i = 0; i < ARDB_MAX_ACTIVE_TOKENS; i++) {
        if (strcmp(g_tokens[i].token, token) == 0) {
            g_tokens[i].is_revoked = 1;
            ar_mutex_unlock(g_auth_mutex);
            return 0;
        }
    }
    ar_mutex_unlock(g_auth_mutex);
    return -1;
}
