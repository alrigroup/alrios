/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */
/*
 * ALRIOS In-Memory Vault — Configuration Matrix Injection Engine
 * Squad 1 — TASK-006: vault_loader.c
 *
 * Implements alrios_vault_inject_envp() and alrios_vault_free_envp().
 * Pure C11, Zero Trust, Pit of Success, ASan/UBSan clean.
 */

#include "vault_loader.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wstringop-truncation"
#endif

/* -----------------------------------------------------------------------
 * Portable secure zeroization
 * ----------------------------------------------------------------------- */
static void secure_zeroize(void *ptr, size_t len) {
    if (!ptr || len == 0) return;
    volatile unsigned char *p = (volatile unsigned char *)ptr;
    while (len--) {
        *p++ = 0x00;
    }
#if defined(__GNUC__) || defined(__clang__)
    __asm__ __volatile__("" : : "r"(ptr) : "memory");
#endif
}

/* -----------------------------------------------------------------------
 * Safe string length calculation with explicit bounds
 * ----------------------------------------------------------------------- */
static size_t safe_strnlen(const char *s, size_t maxlen) {
    if (!s) return 0;
    size_t i = 0;
    while (i < maxlen && s[i] != '\0') {
        i++;
    }
    return i;
}

/* -----------------------------------------------------------------------
 * Key validation: must match ^[A-Z0-9_]+$ per SPECS.md §4.4
 * Keys MUST NOT be empty, MUST NOT begin with a digit (POSIX standard),
 * and MUST contain ONLY uppercase ASCII letters, digits, or underscores.
 * ----------------------------------------------------------------------- */
static int validate_key(const char *key, size_t len) {
    if (!key || len == 0) {
        return VAULT_ERR_INVALID_KEY;
    }

    /* POSIX requirement: key cannot start with a digit */
    if (isdigit((unsigned char)key[0])) {
        return VAULT_ERR_INVALID_KEY;
    }

    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)key[i];
        if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')) {
            return VAULT_ERR_INVALID_KEY;
        }
    }
    return VAULT_OK;
}

/* -----------------------------------------------------------------------
 * Value validation:
 * Values cannot contain embedded NUL bytes (would cause truncation)
 * and cannot start with '=' (would produce corrupt KEY==VAL or ambiguous env).
 * ----------------------------------------------------------------------- */
static int validate_val(const char *val, size_t len) {
    if (!val) {
        return VAULT_ERR_INVALID_VALUE;
    }

    /* An empty value ("KEY=") is valid in POSIX environments */
    if (len == 0) {
        return VAULT_OK;
    }

    /* Avoid '=' smuggling at start of value */
    if (val[0] == '=') {
        return VAULT_ERR_INVALID_VALUE;
    }

    return VAULT_OK;
}

/* -----------------------------------------------------------------------
 * Pre-flight validation
 * ----------------------------------------------------------------------- */
int alrios_vault_validate(const vault_matrix_t *vault) {
    if (!vault) {
        return VAULT_ERR_NULL_PARAM;
    }

    if (vault->count == 0) {
        return VAULT_ERR_EMPTY;
    }

    if (vault->count > VAULT_MAX_ENTRIES) {
        return VAULT_ERR_COUNT_OVERFLOW;
    }

    for (size_t i = 0; i < vault->count; i++) {
        const vault_entry_t *e = &vault->entries[i];

        size_t klen = safe_strnlen(e->key, VAULT_KEY_MAX);
        if (klen >= VAULT_KEY_MAX) {
            return VAULT_ERR_KEY_TOO_LONG;
        }

        int k_rc = validate_key(e->key, klen);
        if (k_rc != VAULT_OK) {
            return k_rc;
        }

        size_t vlen = safe_strnlen(e->val, VAULT_VAL_MAX);
        if (vlen >= VAULT_VAL_MAX) {
            return VAULT_ERR_VAL_TOO_LONG;
        }

        int v_rc = validate_val(e->val, vlen);
        if (v_rc != VAULT_OK) {
            return v_rc;
        }

        /* Check for integer overflow on klen + vlen + 2 */
        if (klen > SIZE_MAX - vlen - 2) {
            return VAULT_ERR_SIZE_OVERFLOW;
        }
    }

    return VAULT_OK;
}

/* -----------------------------------------------------------------------
 * Clean up partially constructed envp array on error
 * ----------------------------------------------------------------------- */
static void cleanup_partial(char **envp, size_t allocated_count) {
    if (!envp) return;
    for (size_t j = 0; j < allocated_count; j++) {
        if (envp[j]) {
            size_t len = safe_strnlen(envp[j], VAULT_KEY_MAX + VAULT_VAL_MAX + 2);
            secure_zeroize(envp[j], len);
            free(envp[j]);
            envp[j] = NULL;
        }
    }
    free(envp);
}

/* -----------------------------------------------------------------------
 * Materialize vault matrix to envp[]
 * ----------------------------------------------------------------------- */
int alrios_vault_inject_envp(const vault_matrix_t *vault, char ***out_envp) {
    if (!out_envp) {
        return VAULT_ERR_NULL_PARAM;
    }
    *out_envp = NULL;

    if (!vault) {
        return VAULT_ERR_NULL_PARAM;
    }

    /* Run full pre-flight validation first */
    int val_rc = alrios_vault_validate(vault);
    if (val_rc != VAULT_OK) {
        return val_rc;
    }

    /*
     * Allocate pointer vector: vault->count entries + 1 for NULL terminator.
     * Check for arithmetic overflow in (count + 1).
     */
    if (vault->count > (SIZE_MAX / sizeof(char *)) - 1) {
        return VAULT_ERR_SIZE_OVERFLOW;
    }

    size_t envp_len = vault->count + 1;
    char **envp = (char **)calloc(envp_len, sizeof(char *));
    if (!envp) {
        return VAULT_ERR_OOM;
    }

    /* Populate each entry safely */
    for (size_t i = 0; i < vault->count; i++) {
        const vault_entry_t *e = &vault->entries[i];

        size_t klen = safe_strnlen(e->key, VAULT_KEY_MAX);
        size_t vlen = safe_strnlen(e->val, VAULT_VAL_MAX);

        /* klen + vlen + 1 (for '=') + 1 (for '\0') */
        size_t line_len = klen + vlen + 2;

        envp[i] = (char *)malloc(line_len);
        if (!envp[i]) {
            cleanup_partial(envp, i);
            return VAULT_ERR_OOM;
        }

        /*
         * Format: "KEY=VALUE"
         * We use snprintf with exact bounded line_len.
         * The result is guaranteed to fit because line_len = klen + vlen + 2.
         */
        int written = snprintf(envp[i], line_len, "%.*s=%.*s",
                               (int)klen, e->key,
                               (int)vlen, e->val);
        if (written < 0 || (size_t)written >= line_len) {
            cleanup_partial(envp, i + 1);
            return VAULT_ERR_SIZE_OVERFLOW;
        }
    }

    /* Terminate the vector as per POSIX execve standard */
    envp[vault->count] = NULL;
    *out_envp = envp;
    return VAULT_OK;
}

/* -----------------------------------------------------------------------
 * Securely release envp[]
 * ----------------------------------------------------------------------- */
void alrios_vault_free_envp(char **envp, size_t entry_count) {
    if (!envp) return;

    for (size_t i = 0; i < entry_count; i++) {
        if (envp[i]) {
            size_t len = safe_strnlen(envp[i], VAULT_KEY_MAX + VAULT_VAL_MAX + 2);
            secure_zeroize(envp[i], len);
            free(envp[i]);
            envp[i] = NULL;
        }
    }

    free(envp);
}

#if defined(__GNUC__) || defined(__clang__)
#  pragma GCC diagnostic pop
#endif
