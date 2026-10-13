/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#include "alrios/supervisor/context.h"
#include "alrios/supervisor/loader.h"
#include "alrios/deploy/drain.h"
#include "ar_ipc.h"
#include "ar_kernel.h"
#include "ar_path.h"
#include "aros_hal.h"
#include "zip.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define SEPARATOR '\\'
#define mkdir_p_(p) _mkdir(p)
#else
#include <unistd.h>
#include <dirent.h>
#define SEPARATOR '/'
#define mkdir_p_(p) mkdir(p, 0755)
#endif

static void mkdir_p(const char *path) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            mkdir_p_(tmp);
            *p = save;
        }
    }
    mkdir_p_(tmp);
}

static hotreload_app_context_t *find_or_create_hotapp(ar_supervisor_context_t *ctx, const char *app_name) {
    if (!ctx || !app_name) return NULL;
    for (int i = 0; i < ctx->hotapp_count; i++) {
        if (strcmp(ctx->hotapps[i].app_name, app_name) == 0) {
            return &ctx->hotapps[i];
        }
    }
    if (ctx->hotapp_count >= AR_MAX_MANAGED_HOTAPPS) {
        return NULL;
    }
    hotreload_app_context_t *hctx = &ctx->hotapps[ctx->hotapp_count++];
    memset(hctx, 0, sizeof(*hctx));
    snprintf(hctx->app_name, sizeof(hctx->app_name), "%s", app_name);
    hctx->active_slot = 0;
    hctx->slots[0].slot_id = 0;
    hctx->slots[0].state = HOTRELOAD_IDLE;
    hctx->slots[1].slot_id = 1;
    hctx->slots[1].state = HOTRELOAD_IDLE;
    return hctx;
}

ar_supervisor_context_t *ar_supervisor_context_create(void) {
    ar_supervisor_context_t *ctx = (ar_supervisor_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return NULL;

    ctx->app_mutex = ar_mutex_create();
    ctx->hotreload_mutex = ar_mutex_create();
    ctx->proc_group = ar_process_group_create();

    ar_supervisor_get_base_dir(ctx, ctx->base_dir, sizeof(ctx->base_dir));
    ar_supervisor_get_apps_dir(ctx, ctx->apps_dir, sizeof(ctx->apps_dir));
    ar_supervisor_get_run_dir(ctx, ctx->run_dir, sizeof(ctx->run_dir));
    ar_supervisor_set_temp_dir(ctx, ctx->base_dir);

    return ctx;
}

int ar_supervisor_context_create_with_config(
    const ar_supervisor_config_t *config,
    ar_supervisor_context_t **out_ctx) {
    if (!out_ctx) return -1;
    *out_ctx = NULL;

    ar_supervisor_context_t *ctx = (ar_supervisor_context_t *)calloc(1, sizeof(*ctx));
    if (!ctx) return -1;

    ctx->app_mutex = ar_mutex_create();
    ctx->hotreload_mutex = ar_mutex_create();
    ctx->proc_group = ar_process_group_create();

    if (config) {
        if (config->base_dir[0]) {
            snprintf(ctx->base_dir, sizeof(ctx->base_dir), "%s", config->base_dir);
        } else {
            ar_supervisor_get_base_dir(ctx, ctx->base_dir, sizeof(ctx->base_dir));
        }

        if (config->apps_dir[0]) {
            snprintf(ctx->apps_dir, sizeof(ctx->apps_dir), "%s", config->apps_dir);
        } else {
            ar_supervisor_get_apps_dir(ctx, ctx->apps_dir, sizeof(ctx->apps_dir));
        }

        if (config->run_dir[0]) {
            snprintf(ctx->run_dir, sizeof(ctx->run_dir), "%s", config->run_dir);
        } else {
            ar_supervisor_get_run_dir(ctx, ctx->run_dir, sizeof(ctx->run_dir));
        }

        ar_supervisor_set_temp_dir(ctx, ctx->base_dir);

        if (config->trust_dir[0]) {
            (void)ar_supervisor_policy_initialize(ctx, config->profile, config->trust_dir);
        }

        if (config->enable_router) {
            (void)ar_supervisor_init_router(ctx, NULL);
        }
    } else {
        ar_supervisor_get_base_dir(ctx, ctx->base_dir, sizeof(ctx->base_dir));
        ar_supervisor_get_apps_dir(ctx, ctx->apps_dir, sizeof(ctx->apps_dir));
        ar_supervisor_get_run_dir(ctx, ctx->run_dir, sizeof(ctx->run_dir));
        ar_supervisor_set_temp_dir(ctx, ctx->base_dir);
    }

    *out_ctx = ctx;
    return 0;
}

void ar_supervisor_context_destroy(ar_supervisor_context_t *ctx) {
    if (!ctx) return;

    /* 1. Teardown active hot-reload deployment slots safely */
    if (ctx->hotreload_mutex) {
        ar_mutex_lock(ctx->hotreload_mutex);
        for (int i = 0; i < ctx->hotapp_count; i++) {
            for (int s = 0; s < AR_MAX_DEPLOY_SLOTS; s++) {
                hotreload_slot_t *slot = &ctx->hotapps[i].slots[s];
                if (slot->pid > 0) {
                    (void)ar_process_kill(slot->pid);
                    (void)ar_process_wait(slot->pid);
                    slot->pid = 0;
                    slot->state = HOTRELOAD_TERMINATED;
                }
            }
        }
        ctx->hotapp_count = 0;
        ar_mutex_unlock(ctx->hotreload_mutex);
    }

    /* 2. Stop and reap all registered supervisor applications */
    ar_supervisor_stop_all(ctx);

    /* 3. Teardown IPC router and free associated routing channels */
    if (ctx->router) {
        ar_guest_router_destroy(ctx->router);
        ctx->router = NULL;
    }

    /* 4. Teardown supervisor execution policy */
    ar_supervisor_policy_shutdown(ctx);

    /* 5. Clean up registered temporary and sandbox directories */
    ar_supervisor_cleanup_temp(ctx);

    /* 6. Destroy process group abstraction */
    if (ctx->proc_group) {
        ar_process_group_destroy(ctx->proc_group);
        ctx->proc_group = NULL;
    }

    /* 7. Teardown synchronization primitives */
    if (ctx->app_mutex) {
        ar_mutex_destroy(ctx->app_mutex);
        ctx->app_mutex = NULL;
    }
    if (ctx->hotreload_mutex) {
        ar_mutex_destroy(ctx->hotreload_mutex);
        ctx->hotreload_mutex = NULL;
    }

    free(ctx);
}

ar_guest_router_t *ar_supervisor_get_router(ar_supervisor_context_t *ctx) {
    if (!ctx) return NULL;
    return ctx->router;
}

int ar_supervisor_init_router(
    ar_supervisor_context_t *ctx,
    const ar_guest_router_config_t *config) {
    if (!ctx) return -1;
    if (ctx->router) {
        ar_guest_router_destroy(ctx->router);
        ctx->router = NULL;
    }
    return ar_guest_router_create(config, &ctx->router);
}

int ar_supervisor_policy_initialize(
    ar_supervisor_context_t *ctx,
    alrios_profile_t profile,
    const char *trust_directory) {
    if (!ctx) return ALRIOS_POLICY_ERR_STATE;
    if (ctx->policy) return ALRIOS_POLICY_ERR_IMMUTABLE;
    return alrios_supervisor_policy_startup_with_trust(
        &ctx->policy, profile, trust_directory);
}

void ar_supervisor_policy_shutdown(ar_supervisor_context_t *ctx) {
    if (!ctx || !ctx->policy) return;
    alrios_supervisor_policy_shutdown(ctx->policy);
    ctx->policy = NULL;
}

int ar_supervisor_policy_get_profile(
    const ar_supervisor_context_t *ctx,
    alrios_profile_t *out_profile) {
    if (!ctx || !ctx->policy) return ALRIOS_POLICY_ERR_STATE;
    return alrios_supervisor_policy_get_profile(ctx->policy, out_profile);
}

int ar_supervisor_policy_set_profile(
    ar_supervisor_context_t *ctx,
    alrios_profile_t profile) {
    if (!ctx || !ctx->policy) return ALRIOS_POLICY_ERR_STATE;
    return alrios_supervisor_policy_set_profile(ctx->policy, profile);
}

int ar_supervisor_policy_evaluate_package(
    const ar_supervisor_context_t *ctx,
    const char *package_path,
    char *reason_buf,
    size_t reason_len) {
    if (!ctx || !ctx->policy) return ALRIOS_POLICY_ERR_STATE;
    return alrios_supervisor_policy_evaluate_package(
        ctx->policy, package_path, (uint64_t)time(NULL),
        reason_buf, reason_len);
}

int ar_supervisor_hotreload_init(ar_supervisor_context_t *ctx) {
    if (!ctx) return -1;
    if (ctx->hotreload_mutex) ar_mutex_lock(ctx->hotreload_mutex);
    memset(ctx->hotapps, 0, sizeof(ctx->hotapps));
    ctx->hotapp_count = 0;
    if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
    return 0;
}

int ar_supervisor_hotreload_deploy(
    ar_supervisor_context_t *ctx,
    const char *app_name,
    const char *arapp_path,
    char *out_log,
    size_t out_log_size) {
    if (!ctx || !app_name || !arapp_path) {
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Invalid parameters");
        return -1;
    }

    char resolved_pkg[1024];
    snprintf(resolved_pkg, sizeof(resolved_pkg), "%s", arapp_path);

    if (!ar_fs_exists(resolved_pkg)) {
        char base_dir[1024];
        ar_supervisor_get_base_dir(ctx, base_dir, sizeof(base_dir));
        snprintf(resolved_pkg, sizeof(resolved_pkg), "%s%c%s", base_dir, SEPARATOR, arapp_path);
    }
    if (!ar_fs_exists(resolved_pkg)) {
        char apps_dir[1024];
        ar_supervisor_get_apps_dir(ctx, apps_dir, sizeof(apps_dir));
        snprintf(resolved_pkg, sizeof(resolved_pkg), "%s%c%s", apps_dir, SEPARATOR, arapp_path);
    }
    if (!ar_fs_exists(resolved_pkg)) {
        char apps_dir[1024];
        ar_supervisor_get_apps_dir(ctx, apps_dir, sizeof(apps_dir));
        snprintf(resolved_pkg, sizeof(resolved_pkg), "%s%c%s.arapp", apps_dir, SEPARATOR, app_name);
    }

    if (!ar_fs_exists(resolved_pkg)) {
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Package file not found: %s", arapp_path);
        return -2;
    }

    {
        char policy_reason[160];
        int policy_result = ar_supervisor_policy_evaluate_package(
            ctx, resolved_pkg, policy_reason, sizeof(policy_reason));
        if (policy_result != ALRIOS_POLICY_OK) {
            if (out_log && out_log_size > 0) {
                snprintf(out_log, out_log_size,
                         "ERR: Package rejected by execution profile (rc=%d, %s)",
                         policy_result,
                         policy_reason[0] ? policy_reason : "fail-closed");
            }
            return -8;
        }
    }

    if (ctx->hotreload_mutex) ar_mutex_lock(ctx->hotreload_mutex);
    hotreload_app_context_t *actx = find_or_create_hotapp(ctx, app_name);
    if (!actx) {
        if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Maximum managed hot-reload applications reached");
        return -3;
    }

    int current_active = actx->active_slot;
    int candidate_idx = (actx->slots[current_active].state == HOTRELOAD_ACTIVE) ? (1 - current_active) : current_active;
    hotreload_slot_t *cand = &actx->slots[candidate_idx];

    if (cand->pid > 0) {
        ar_process_kill(cand->pid);
        cand->pid = 0;
    }

    cand->slot_id = candidate_idx;
    cand->state = HOTRELOAD_STAGING;

    char base_dir[1024];
    ar_supervisor_get_base_dir(ctx, base_dir, sizeof(base_dir));
    snprintf(cand->staging_dir, sizeof(cand->staging_dir), "%s%cprogramfiles%c%s_slot_%d",
             base_dir, SEPARATOR, SEPARATOR, app_name, candidate_idx);
    mkdir_p(cand->staging_dir);

    zip_reader_t *z = zip_reader_open_arapp(resolved_pkg);
    if (!z) {
        z = zip_reader_open(resolved_pkg);
    }
    if (!z) {
        cand->state = HOTRELOAD_ABORTED;
        if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Falha ao abrir pacote .arapp: %s", resolved_pkg);
        return -4;
    }

    int count = zip_reader_count(z);
    for (int i = 0; i < count; i++) {
        zip_reader_extract(z, i, cand->staging_dir);
    }
    zip_reader_close(z);

    char manifest_path[1024];
    snprintf(manifest_path, sizeof(manifest_path), "%s%c%s.arappmake", cand->staging_dir, SEPARATOR, app_name);

    char entry_name[256] = {0};
    FILE *mf = fopen(manifest_path, "r");
    if (mf) {
        char line[512];
        while (fgets(line, sizeof(line), mf)) {
            char *pentry = strstr(line, "\"entry\"");
            if (pentry) {
                char *colon = strchr(pentry, ':');
                if (colon) {
                    char *q1 = strchr(colon, '\"');
                    if (q1) {
                        char *q2 = strchr(q1 + 1, '\"');
                        if (q2) {
                            size_t elen = (size_t)(q2 - q1 - 1);
                            if (elen < sizeof(entry_name)) {
                                memcpy(entry_name, q1 + 1, elen);
                                entry_name[elen] = '\0';
                            }
                        }
                    }
                }
            }
        }
        fclose(mf);
    }

    if (!entry_name[0]) {
        snprintf(entry_name, sizeof(entry_name), "%s_bin", app_name);
    }

    char resolved_bin[1024];
    snprintf(resolved_bin, sizeof(resolved_bin), "%s%c%s", cand->staging_dir, SEPARATOR, entry_name);
    snprintf(cand->bin_path, sizeof(cand->bin_path), "%s", resolved_bin);
#ifndef _WIN32
    chmod(cand->bin_path, 0755);
#endif

    if (!ar_fs_exists(cand->bin_path)) {
        cand->state = HOTRELOAD_ABORTED;
        if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Executavel nao encontrado no slot: %s", cand->bin_path);
        return -5;
    }

    cand->state = HOTRELOAD_BOOTING;
    cand->port = (uint16_t)(13000 + (candidate_idx * 100) + (rand() % 50));

    char *argv[2];
    argv[0] = cand->bin_path;
    argv[1] = NULL;
    int pid = ar_process_create(cand->bin_path, argv);
    if (pid <= 0) {
        cand->state = HOTRELOAD_ABORTED;
        if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Falha ao iniciar processo no slot candidato");
        return -6;
    }

    cand->pid = pid;
    void *proc_group = ar_supervisor_get_proc_group(ctx);
    if (proc_group && ar_process_group_add(proc_group, pid) != 0) {
        (void)ar_process_signal(pid, AR_PROCESS_SIGNAL_KILL, 0);
        (void)ar_process_wait(pid);
        cand->pid = 0;
        cand->state = HOTRELOAD_ABORTED;
        if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Falha ao rastrear processo no slot candidato");
        return -6;
    }

    cand->state = HOTRELOAD_PROBING;

    int probe_ok = 1;
    for (int p = 0; p < 3; p++) {
        ar_sleep_ms(50);
        if (ar_process_wait_nohang(cand->pid) != 0) {
            probe_ok = 0;
            break;
        }
    }

    if (!probe_ok) {
        cand->state = HOTRELOAD_ABORTED;
        ar_process_kill(cand->pid);
        cand->pid = 0;
        if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "FAIL: Processo no Slot %d abortou durante os health probes sinteticos", candidate_idx);
        return -7;
    }

    int old_slot_idx = actx->active_slot;
    hotreload_slot_t *old_slot = &actx->slots[old_slot_idx];

    cand->state = HOTRELOAD_ACTIVE;
    actx->active_slot = candidate_idx;

    ar_supervisor_update_app_process(ctx, app_name, cand->pid, cand->staging_dir);

    if (old_slot_idx != candidate_idx && old_slot->pid > 0 && old_slot->state == HOTRELOAD_ACTIVE) {
        old_slot->state = HOTRELOAD_DRAINING;
        int exit_status = 0;
        (void)alrios_drain_process_await((pid_t)old_slot->pid,
                                         AR_DEFAULT_DRAIN_TIMEOUT_MS,
                                         500U,
                                         &exit_status);
        old_slot->state = HOTRELOAD_TERMINATED;
        old_slot->pid = 0;
    }

    if (out_log && out_log_size > 0) {
        snprintf(out_log, out_log_size,
                 "OK: [ZERO-DOWNTIME] Slot %s ativado com sucesso (PID %d na porta %u). Slot %s drenado sem perda de conexoes.",
                 candidate_idx == 0 ? "Alpha" : "Beta", cand->pid, cand->port,
                 old_slot_idx == 0 ? "Alpha" : "Beta");
    }

    if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
    return 0;
}

int ar_supervisor_hotreload_status(
    ar_supervisor_context_t *ctx,
    const char *app_name,
    char *out_buf,
    size_t out_buf_size) {
    if (!ctx || !app_name || !out_buf || out_buf_size == 0) return -1;
    if (ctx->hotreload_mutex) ar_mutex_lock(ctx->hotreload_mutex);
    for (int i = 0; i < ctx->hotapp_count; i++) {
        if (strcmp(ctx->hotapps[i].app_name, app_name) == 0) {
            hotreload_app_context_t *actx = &ctx->hotapps[i];
            int active = actx->active_slot;
            snprintf(out_buf, out_buf_size,
                     "App: %s | Active Slot: %s | Slot Alpha: [PID %d, State %d] | Slot Beta: [PID %d, State %d]",
                     actx->app_name, active == 0 ? "Alpha" : "Beta",
                     actx->slots[0].pid, actx->slots[0].state,
                     actx->slots[1].pid, actx->slots[1].state);
            if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
            return 0;
        }
    }
    if (ctx->hotreload_mutex) ar_mutex_unlock(ctx->hotreload_mutex);
    snprintf(out_buf, out_buf_size, "App: %s | Status: No active hot-reload session", app_name);
    return 0;
}
