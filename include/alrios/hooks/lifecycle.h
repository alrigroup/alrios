/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_HOOKS_LIFECYCLE_H
#define ALRIOS_HOOKS_LIFECYCLE_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "alrios/platform/types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ALRIOS_HOOK_DEFAULT_TIMEOUT_MS 5000U
#define ALRIOS_HOOK_MAX_PATH_LEN       1024U
#define ALRIOS_HOOK_MAX_NAME_LEN       128U
#define ALRIOS_HOOK_MAX_ENV_COUNT      64U

/* Error codes matching ALRIOS platform specifications */
#define ALRIOS_HOOK_OK                  0
#define ALRIOS_HOOK_ERR_INVALID        -3001
#define ALRIOS_HOOK_ERR_TIMEOUT        -3002
#define ALRIOS_HOOK_ERR_SIGNATURE      -3003
#define ALRIOS_HOOK_ERR_EXEC           -3004
#define ALRIOS_HOOK_ERR_ABORT          -3005
#define ALRIOS_HOOK_ERR_NOMEM          -3006
#define ALRIOS_HOOK_ERR_PRIVILEGE      -3007
#define ALRIOS_HOOK_ERR_SWAP           -3008
#define ALRIOS_HOOK_ERR_ROLLBACK       -3009

/* Lifecycle hook phases */
typedef enum alrios_hook_phase {
    ALRIOS_HOOK_PRE_BUILD  = 1,
    ALRIOS_HOOK_POST_BUILD = 2,
    ALRIOS_HOOK_PRE_SWAP   = 3,
    ALRIOS_HOOK_POST_SWAP  = 4,
    ALRIOS_HOOK_PRE_DRAIN  = 5,
    ALRIOS_HOOK_POST_DRAIN = 6
} alrios_hook_phase_t;

/* Hook entry descriptor for explicit registered hook sequences */
typedef struct alrios_hook_entry {
    char name[ALRIOS_HOOK_MAX_NAME_LEN];
    char path[ALRIOS_HOOK_MAX_PATH_LEN];
    char **argv;              /* Optional NULL-terminated argument vector */
    uint32_t timeout_ms;      /* 0 to use context default timeout */
    bool optional;            /* If true, failure does not abort sequence */
} alrios_hook_entry_t;

/* Hook invocation context */
typedef struct alrios_hook_context {
    alrios_hook_phase_t phase;
    const char *app_id;
    const char *slot_path;           /* Target slot or staging path */
    const char *old_slot_path;       /* For swap: current/previous slot */
    const char *new_slot_path;       /* For swap: candidate/new slot */
    const char *working_directory;   /* Execution working directory */
    uint32_t timeout_ms;             /* Default timeout in ms */
    const char *const *custom_env;   /* Additional environment variables */
    size_t custom_env_count;
    uid_t run_as_uid;                /* Drop to UID if > 0 */
    gid_t run_as_gid;                /* Drop to GID if > 0 */
    bool drop_privileges;            /* Enforce PR_SET_NO_NEW_PRIVS */
} alrios_hook_context_t;

/* Hook execution result */
typedef struct alrios_hook_result {
    int exit_code;
    int error_code;
    uint64_t elapsed_ms;
    char failed_hook[ALRIOS_HOOK_MAX_PATH_LEN];
    bool timed_out;
    bool aborted;
} alrios_hook_result_t;

/* String representation of lifecycle phase ("pre-build", "pre-swap", etc.) */
const char *alrios_hook_phase_name(alrios_hook_phase_t phase);

/* Parse lifecycle phase from name */
int alrios_hook_phase_from_name(const char *name, alrios_hook_phase_t *out_phase);

/* Whether failure in given phase is fatal and aborts pipeline */
bool alrios_hook_phase_aborts_on_fail(alrios_hook_phase_t phase);

/* Human-readable error message */
const char *alrios_hook_strerror(int code);

/* Execute a single hook executable with timeout and confinement */
int alrios_hook_run_single(const alrios_hook_context_t *ctx,
                           const char *hook_path,
                           uint32_t timeout_ms,
                           alrios_hook_result_t *out_result);

/* Execute a list of hooks sequentially in strict declaration order */
int alrios_hook_run_list(const alrios_hook_context_t *ctx,
                         const alrios_hook_entry_t *hooks,
                         size_t hook_count,
                         alrios_hook_result_t *out_result);

/* Execute executable hooks inside a directory in lexicographic byte order */
int alrios_hook_run_directory(const alrios_hook_context_t *ctx,
                              const char *dir_path,
                              alrios_hook_result_t *out_result);

/* Discover and execute hooks for a phase across slot and system directories */
int alrios_hook_run_phase(const alrios_hook_context_t *ctx,
                          alrios_hook_result_t *out_result);

/*
 * High-level atomic swap with lifecycle hooks:
 * 1. Executes PRE_SWAP hooks for the candidate slot.
 * 2. If PRE_SWAP fails: aborts immediately, keeps active symlink untouched,
 *    and returns ALRIOS_HOOK_ERR_ABORT.
 * 3. If PRE_SWAP succeeds: performs atomic symlink swap.
 * 4. On swap failure: restores previous symlink target (rollback).
 * 5. Executes POST_SWAP hooks.
 */
int alrios_lifecycle_atomic_swap(const alrios_hook_context_t *ctx,
                                 const char *target_dir,
                                 const char *symlink_current,
                                 pid_t master_pid,
                                 alrios_hook_result_t *out_result);

#ifdef __cplusplus
}
#endif

#endif /* ALRIOS_HOOKS_LIFECYCLE_H */
