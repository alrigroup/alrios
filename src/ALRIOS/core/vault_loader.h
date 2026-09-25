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
 * Squad 1 — TASK-006: vault_loader.h
 *
 * CONTRACT (SPECS.md §8 / Tripartite State Separation Invariant 2):
 *   - Configuration NEVER touches disk in plaintext.
 *   - Secrets are consumed from RAM, materialized as envp[] passed to fexecve().
 *   - Every allocation containing secret material is zeroized before free.
 *   - Callers in SOVEREIGN-MAX profile MUST wrap vault_matrix_t in guard pages
 *     via alrios_alloc_guarded_pages() before populating and releasing via
 *     alrios_vault_free_envp() before alrios_free_guarded_pages().
 */

#ifndef ALRIOS_VAULT_LOADER_H
#define ALRIOS_VAULT_LOADER_H

#include <stddef.h>

/* -----------------------------------------------------------------------
 * Capacity constants — tunable at compile time without ABI breakage
 * ----------------------------------------------------------------------- */
#ifndef VAULT_MAX_ENTRIES
#  define VAULT_MAX_ENTRIES   128U
#endif

/*
 * Key constraint: SPECS §4.4 manifest schema — required_keys pattern
 * ^[A-Z0-9_]+$, minimum 1 char, maximum KEY_MAX-1 usable chars.
 */
#ifndef VAULT_KEY_MAX
#  define VAULT_KEY_MAX       65U   /* 64 usable chars + NUL */
#endif

/*
 * Value constraint: generous upper bound; prevents single-entry
 * allocation from overflowing size_t when combined with key + "=" + NUL.
 */
#ifndef VAULT_VAL_MAX
#  define VAULT_VAL_MAX       1025U /* 1024 usable chars + NUL */
#endif

/* -----------------------------------------------------------------------
 * Return codes
 * ----------------------------------------------------------------------- */
#define VAULT_OK                    0
#define VAULT_ERR_NULL_PARAM       -1  /* required pointer was NULL */
#define VAULT_ERR_EMPTY             -2  /* vault has zero entries */
#define VAULT_ERR_COUNT_OVERFLOW   -3  /* count > VAULT_MAX_ENTRIES */
#define VAULT_ERR_INVALID_KEY      -4  /* key fails ^[A-Z0-9_]+$ validation */
#define VAULT_ERR_INVALID_VALUE    -5  /* value contains embedded NUL or '=' */
#define VAULT_ERR_KEY_TOO_LONG     -6  /* key >= VAULT_KEY_MAX */
#define VAULT_ERR_VAL_TOO_LONG     -7  /* value >= VAULT_VAL_MAX */
#define VAULT_ERR_SIZE_OVERFLOW    -8  /* key_len + val_len + 2 overflows size_t */
#define VAULT_ERR_OOM              -9  /* malloc/calloc returned NULL */

/* -----------------------------------------------------------------------
 * Data structures
 * ----------------------------------------------------------------------- */

/*
 * vault_entry_t — a single key=value configuration pair.
 *
 * SECURITY: both fields are fixed-size arrays; NUL termination is
 * enforced unconditionally by alrios_vault_inject_envp().
 * Callers MUST NOT rely on implicit NUL termination from their fill path.
 */
typedef struct vault_entry {
    char key[VAULT_KEY_MAX];
    char val[VAULT_VAL_MAX];
} vault_entry_t;

/*
 * vault_matrix_t — the complete in-memory configuration store.
 *
 * LIFECYCLE:
 *   1. Allocate (optionally inside guard pages).
 *   2. Populate entries from the encrypted vault channel.
 *   3. Call alrios_vault_inject_envp() to materialize envp[].
 *   4. Zeroize this struct immediately after — envp[] now owns the data.
 *   5. Pass envp[] to fexecve(); do NOT log or persist it.
 *   6. After launch call alrios_vault_free_envp() to wipe and release.
 */
typedef struct vault_matrix {
    size_t       count;                          /* number of valid entries */
    vault_entry_t entries[VAULT_MAX_ENTRIES];    /* the entries array       */
} vault_matrix_t;

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * alrios_vault_inject_envp()
 *
 * Materializes the vault configuration matrix into a NULL-terminated
 * envp[] array suitable for direct use with fexecve(2).
 *
 * Each slot i of *out_envp will hold a heap-allocated string of the form
 * "KEY=VALUE\0". (*out_envp)[count] == NULL (POSIX terminator).
 *
 * SECURITY INVARIANTS:
 *   - Key is validated against ^[A-Z0-9_]+$ (no injection via key).
 *   - Value is checked for embedded NUL bytes (no truncation attack).
 *   - Value is checked for leading '=' characters (no key smuggling).
 *   - Integer overflow in (key_len + val_len + 2) is explicitly detected.
 *   - On ANY allocation failure, all previously allocated slots are
 *     zeroized with explicit_bzero() before free, preventing secret
 *     material from resting in freed heap pages.
 *   - The function never calls strlen() on fixed-size fields past their
 *     declared bounds; it uses strnlen() with explicit capacity limits.
 *
 * @param vault     Pointer to a populated vault_matrix_t. Must be non-NULL.
 * @param out_envp  Output pointer; receives the allocated envp[]. Must be
 *                  non-NULL. Set to NULL on any error return.
 *
 * @return VAULT_OK (0) on success, or a negative VAULT_ERR_* code.
 *         On error, *out_envp is set to NULL and no memory is leaked.
 */
int alrios_vault_inject_envp(const vault_matrix_t *vault, char ***out_envp);

/*
 * alrios_vault_free_envp()
 *
 * Securely releases an envp[] array produced by alrios_vault_inject_envp().
 *
 * SECURITY: each slot is overwritten with explicit_bzero() BEFORE free(),
 * preventing secret residue from lingering on the freed heap.
 *
 * @param envp       The envp[] pointer to release. If NULL, a no-op.
 * @param entry_count The number of valid (non-NULL) slots that were
 *                   allocated. Callers MUST pass the exact count returned
 *                   by the vault struct (vault->count). Passing a wrong
 *                   count is undefined behaviour.
 */
void alrios_vault_free_envp(char **envp, size_t entry_count);

/*
 * alrios_vault_validate()
 *
 * Full pre-flight validation of a vault_matrix_t without allocating envp[].
 * Useful for unit tests and audit daemons that wish to verify a vault
 * snapshot before committing to the injection path.
 *
 * @return VAULT_OK if all entries pass, or the first VAULT_ERR_* encountered.
 */
int alrios_vault_validate(const vault_matrix_t *vault);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_VAULT_LOADER_H */
