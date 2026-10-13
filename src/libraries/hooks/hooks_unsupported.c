/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/hooks.h"
#undef ALRIOS_HOOK_ERR_TIMEOUT
#include "alrios/hooks/lifecycle.h"

#include <string.h>

int alrios_hook_execute(const alrios_hook_inv_t *inv) {
    (void)inv;
    return ALRIOS_HOOK_ERR_FAIL;
}

const char *alrios_hook_phase_name(alrios_hook_phase_t phase) {
    switch (phase) {
        case ALRIOS_HOOK_PRE_BUILD: return "pre-build";
        case ALRIOS_HOOK_POST_BUILD: return "post-build";
        case ALRIOS_HOOK_PRE_SWAP: return "pre-swap";
        case ALRIOS_HOOK_POST_SWAP: return "post-swap";
        case ALRIOS_HOOK_PRE_DRAIN: return "pre-drain";
        case ALRIOS_HOOK_POST_DRAIN: return "post-drain";
        default: return "unknown";
    }
}

int alrios_hook_phase_from_name(const char *name, alrios_hook_phase_t *out_phase) {
    static const char *const names[] = {
        "", "pre-build", "post-build", "pre-swap", "post-swap", "pre-drain", "post-drain"
    };
    if (!name || !out_phase) return ALRIOS_HOOK_ERR_INVALID;
    for (int phase = ALRIOS_HOOK_PRE_BUILD; phase <= ALRIOS_HOOK_POST_DRAIN; ++phase) {
        if (strcmp(name, names[phase]) == 0) {
            *out_phase = (alrios_hook_phase_t)phase;
            return ALRIOS_HOOK_OK;
        }
    }
    return ALRIOS_HOOK_ERR_INVALID;
}

bool alrios_hook_phase_aborts_on_fail(alrios_hook_phase_t phase) {
    return phase != ALRIOS_HOOK_POST_SWAP && phase != ALRIOS_HOOK_POST_DRAIN;
}

const char *alrios_hook_strerror(int code) {
    return code == ALRIOS_HOOK_OK ? "Success" : "Hook execution is unsupported on this platform";
}

static int unsupported_result(alrios_hook_result_t *out_result) {
    if (out_result) {
        memset(out_result, 0, sizeof(*out_result));
        out_result->error_code = ALRIOS_HOOK_ERR_EXEC;
    }
    return ALRIOS_HOOK_ERR_EXEC;
}

int alrios_hook_run_single(const alrios_hook_context_t *ctx,
                           const char *hook_path,
                           uint32_t timeout_ms,
                           alrios_hook_result_t *out_result) {
    (void)ctx;
    (void)hook_path;
    (void)timeout_ms;
    return unsupported_result(out_result);
}

int alrios_hook_run_list(const alrios_hook_context_t *ctx,
                         const alrios_hook_entry_t *hooks,
                         size_t hook_count,
                         alrios_hook_result_t *out_result) {
    (void)ctx;
    (void)hooks;
    (void)hook_count;
    return unsupported_result(out_result);
}

int alrios_hook_run_directory(const alrios_hook_context_t *ctx,
                              const char *dir_path,
                              alrios_hook_result_t *out_result) {
    (void)ctx;
    (void)dir_path;
    return unsupported_result(out_result);
}

int alrios_hook_run_phase(const alrios_hook_context_t *ctx,
                          alrios_hook_result_t *out_result) {
    (void)ctx;
    return unsupported_result(out_result);
}

int alrios_lifecycle_atomic_swap(const alrios_hook_context_t *ctx,
                                 const char *target_dir,
                                 const char *symlink_current,
                                 pid_t master_pid,
                                 alrios_hook_result_t *out_result) {
    (void)ctx;
    (void)target_dir;
    (void)symlink_current;
    (void)master_pid;
    return unsupported_result(out_result);
}
