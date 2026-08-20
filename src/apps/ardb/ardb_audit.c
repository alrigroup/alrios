/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_audit.h"
#include "aros_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <openssl/sha.h>

static char g_audit_log_path[256] = "storage/ardb/audit.log";
static char g_prev_hash[65] = "0000000000000000000000000000000000000000000000000000000000000000";
static void *g_audit_mutex = NULL;
static int g_audit_initialized = 0;

static void sha256_hex(const char *input, char *out_hex) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256((const unsigned char*)input, strlen(input), hash);
    for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
        snprintf(out_hex + (i * 2), 3, "%02x", hash[i]);
    }
}

void ardb_audit_init(const char *log_path) {
    if (!g_audit_initialized) {
        g_audit_mutex = ar_mutex_create();
        g_audit_initialized = 1;
    }
    if (log_path && log_path[0]) {
        strncpy(g_audit_log_path, log_path, sizeof(g_audit_log_path) - 1);
    }

#ifdef _WIN32
    CreateDirectoryA("storage", NULL);
    CreateDirectoryA("storage\\ardb", NULL);
#else
    system("mkdir -p storage/ardb 2>/dev/null");
#endif

    /* Load last hash from the chain if file already exists */
    strncpy(g_prev_hash, "0000000000000000000000000000000000000000000000000000000000000000", 64);
    FILE *f = fopen(g_audit_log_path, "r");
    if (f) {
        char line[8192];
        while (fgets(line, sizeof(line), f)) {
            char *hash_pos = strstr(line, " hash=");
            if (hash_pos) {
                char h[65] = {0};
                strncpy(h, hash_pos + 6, 64);
                if (strlen(h) == 64) {
                    strncpy(g_prev_hash, h, 64);
                }
            }
        }
        fclose(f);
    }
}

void ardb_audit_cleanup(void) {
    if (!g_audit_initialized) return;
    if (g_audit_mutex) {
        ar_mutex_destroy(g_audit_mutex);
        g_audit_mutex = NULL;
    }
    g_audit_initialized = 0;
}

void ardb_audit_log_query(const char *user, const char *tenant_id, const char *client_ip,
                          const char *sql_query, int status_code, uint64_t duration_us) {
    if (!g_audit_initialized) {
        ardb_audit_init(NULL);
    }
    ar_mutex_lock(g_audit_mutex);

    /* Timestamp in microseconds directly from host OS clock */
    uint64_t ts_ms = (uint64_t)ar_time_ms();

    /* Build log entry */
    char entry_raw[4096];
    snprintf(entry_raw, sizeof(entry_raw),
             "ts=%llu user=%s tenant=%s ip=%s status=%d duration_us=%llu sql=[%s] prev_hash=%s",
             (unsigned long long)ts_ms,
             user ? user : "anonymous",
             tenant_id ? tenant_id : "none",
             client_ip ? client_ip : "127.0.0.1",
             status_code,
             (unsigned long long)duration_us,
             sql_query ? sql_query : "",
             g_prev_hash);

    /* Current_Hash = SHA256(Current_Log + Prev_Hash) */
    char current_hash[65];
    sha256_hex(entry_raw, current_hash);

    /* Append-only write */
    FILE *f = fopen(g_audit_log_path, "a");
    if (f) {
        fprintf(f, "%s hash=%s\n", entry_raw, current_hash);
        fflush(f);
        fclose(f);
    }

    /* Advance chain state */
    strncpy(g_prev_hash, current_hash, sizeof(g_prev_hash) - 1);

    ar_mutex_unlock(g_audit_mutex);
}

int ardb_audit_verify_integrity(const char *log_path, char *out_error, size_t out_error_size) {
    const char *path = (log_path && log_path[0]) ? log_path : g_audit_log_path;
    FILE *f = fopen(path, "r");
    if (!f) {
        if (out_error) snprintf(out_error, out_error_size, "Log file not found: %s", path);
        return 0; /* Empty or non-existent */
    }

    char line[8192];
    char expected_prev_hash[65] = "0000000000000000000000000000000000000000000000000000000000000000";
    int line_num = 0;

    while (fgets(line, sizeof(line), f)) {
        line_num++;
        /* Strip newlines */
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
            line[--len] = '\0';
        }

        char *hash_pos = strstr(line, " hash=");
        if (!hash_pos) {
            if (out_error) snprintf(out_error, out_error_size, "Line %d missing cryptographic hash", line_num);
            fclose(f);
            return -1;
        }

        char recorded_hash[65] = {0};
        strncpy(recorded_hash, hash_pos + 6, 64);
        *hash_pos = '\0'; /* entry_raw */

        char calculated_hash[65];
        sha256_hex(line, calculated_hash);

        if (strcmp(recorded_hash, calculated_hash) != 0) {
            if (out_error) {
                snprintf(out_error, out_error_size,
                         "CRITICAL: Forensic Integrity Failure at line %d. Hash chain corrupted.", line_num);
            }
            fclose(f);
            return -1;
        }

        /* Verify previous hash match */
        char *prev_pos = strstr(line, "prev_hash=");
        if (prev_pos) {
            char line_prev_hash[65] = {0};
            strncpy(line_prev_hash, prev_pos + 10, 64);
            if (strcmp(line_prev_hash, expected_prev_hash) != 0) {
                if (out_error) {
                    snprintf(out_error, out_error_size,
                             "CRITICAL: Link Failure at line %d (prev_hash mismatch).", line_num);
                }
                fclose(f);
                return -1;
            }
        }

        strncpy(expected_prev_hash, calculated_hash, 64);
    }

    fclose(f);
    return 0; /* 100% Verified */
}
