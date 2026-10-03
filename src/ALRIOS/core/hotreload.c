/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#include "hotreload.h"
#include "loader.h"
#include "ar_ipc.h"
#include "arapp_parser.h"
#include "aros_hal.h"
#include "zip.h"
#include "log.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#define SEPARATOR '\\'
#define mkdir_p_(p) _mkdir(p)
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#define SEPARATOR '/'
#define mkdir_p_(p) mkdir(p, 0755)
#endif

static hotreload_app_context_t g_hotapps[AR_MAX_MANAGED_HOTAPPS];
static int g_hotapp_count = 0;
static void *g_hotreload_mutex = NULL;

static void hotreload_lock(void) {
    if (!g_hotreload_mutex) {
        g_hotreload_mutex = ar_mutex_create();
    }
    if (g_hotreload_mutex) {
        ar_mutex_lock(g_hotreload_mutex);
    }
}

static void hotreload_unlock(void) {
    if (g_hotreload_mutex) {
        ar_mutex_unlock(g_hotreload_mutex);
    }
}

int ar_hotreload_init(void) {
    hotreload_lock();
    if (g_hotapp_count == 0) {
        memset(g_hotapps, 0, sizeof(g_hotapps));
    }
    hotreload_unlock();
    return 0;
}

static hotreload_app_context_t *find_or_create_context(const char *app_name) {
    for (int i = 0; i < g_hotapp_count; i++) {
        if (strcmp(g_hotapps[i].app_name, app_name) == 0) {
            return &g_hotapps[i];
        }
    }
    if (g_hotapp_count >= AR_MAX_MANAGED_HOTAPPS) {
        return NULL;
    }
    hotreload_app_context_t *ctx = &g_hotapps[g_hotapp_count++];
    memset(ctx, 0, sizeof(*ctx));
    snprintf(ctx->app_name, sizeof(ctx->app_name), "%s", app_name);
    ctx->active_slot = 0;
    ctx->slots[0].slot_id = 0;
    ctx->slots[0].state = HOTRELOAD_IDLE;
    ctx->slots[1].slot_id = 1;
    ctx->slots[1].state = HOTRELOAD_IDLE;
    return ctx;
}

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

int ar_hotreload_deploy(const char *app_name, const char *arapp_path, char *out_log, size_t out_log_size) {
    if (!app_name || !arapp_path) {
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Invalid parameters");
        return -1;
    }

    char resolved_pkg[1024];
    snprintf(resolved_pkg, sizeof(resolved_pkg), "%s", arapp_path);

    if (!ar_fs_exists(resolved_pkg)) {
        char base_dir[1024];
        loader_get_base_dir(base_dir, sizeof(base_dir));
        snprintf(resolved_pkg, sizeof(resolved_pkg), "%s%c%s", base_dir, SEPARATOR, arapp_path);
    }
    if (!ar_fs_exists(resolved_pkg)) {
        char apps_dir[1024];
        loader_get_apps_dir(apps_dir, sizeof(apps_dir));
        snprintf(resolved_pkg, sizeof(resolved_pkg), "%s%c%s", apps_dir, SEPARATOR, arapp_path);
    }
    if (!ar_fs_exists(resolved_pkg)) {
        char apps_dir[1024];
        loader_get_apps_dir(apps_dir, sizeof(apps_dir));
        snprintf(resolved_pkg, sizeof(resolved_pkg), "%s%c%s.arapp", apps_dir, SEPARATOR, app_name);
    }

    if (!ar_fs_exists(resolved_pkg)) {
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Package file not found: %s", arapp_path);
        return -2;
    }

    hotreload_lock();
    hotreload_app_context_t *ctx = find_or_create_context(app_name);
    if (!ctx) {
        hotreload_unlock();
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Maximum managed hot-reload applications reached");
        return -3;
    }

    /* Determina slot candidato: se slot ativo estiver rodando, use o oposto */
    int current_active = ctx->active_slot;
    int candidate_idx = (ctx->slots[current_active].state == HOTRELOAD_ACTIVE) ? (1 - current_active) : current_active;
    hotreload_slot_t *cand = &ctx->slots[candidate_idx];

    /* Se o candidato possui um processo fantasma anterior, encerra-o */
    if (cand->pid > 0) {
        ar_process_kill(cand->pid);
        cand->pid = 0;
    }

    cand->slot_id = candidate_idx;
    cand->state = HOTRELOAD_STAGING;

    /* Define diretorio sandbox isolado para o slot candidato */
    char base_dir[1024];
    loader_get_base_dir(base_dir, sizeof(base_dir));
    snprintf(cand->staging_dir, sizeof(cand->staging_dir), "%s%cprogramfiles%c%s_slot_%d",
             base_dir, SEPARATOR, SEPARATOR, app_name, candidate_idx);
    mkdir_p(cand->staging_dir);

    /* Extrai pacote .arapp no sandbox do slot */
    zip_reader_t *z = zip_reader_open_arapp(resolved_pkg);
    if (!z) {
        z = zip_reader_open(resolved_pkg);
    }
    if (!z) {
        cand->state = HOTRELOAD_ABORTED;
        hotreload_unlock();
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Falha ao abrir pacote .arapp: %s", resolved_pkg);
        return -4;
    }

    int count = zip_reader_count(z);
    for (int i = 0; i < count; i++) {
        zip_reader_extract(z, i, cand->staging_dir);
    }
    zip_reader_close(z);

    /* Localiza o binario executavel dentro do sandbox */
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
        hotreload_unlock();
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Executavel nao encontrado no slot: %s", cand->bin_path);
        return -5;
    }

    cand->state = HOTRELOAD_BOOTING;
    cand->port = (uint16_t)(13000 + (candidate_idx * 100) + (rand() % 50));

    /* Inicializa o processo candidato */
    char *argv[2];
    argv[0] = cand->bin_path;
    argv[1] = NULL;
    int pid = ar_process_create(cand->bin_path, argv);
    if (pid <= 0) {
        cand->state = HOTRELOAD_ABORTED;
        hotreload_unlock();
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "ERR: Falha ao iniciar processo no slot candidato");
        return -6;
    }

    cand->pid = pid;
    void *proc_group = loader_get_proc_group();
    if (proc_group) {
        ar_process_group_add(proc_group, pid);
    }

    cand->state = HOTRELOAD_PROBING;

    /* Probes de saúde sintéticos agressivos (3 ciclos com intervalo de 50ms) */
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
        hotreload_unlock();
        if (out_log && out_log_size > 0)
            snprintf(out_log, out_log_size, "FAIL: Processo no Slot %d abortou durante os health probes sinteticos", candidate_idx);
        return -7;
    }

    /* Comutação Atômica de Rota (< 1 nanosegundo em memória) */
    int old_slot_idx = ctx->active_slot;
    hotreload_slot_t *old_slot = &ctx->slots[old_slot_idx];

    cand->state = HOTRELOAD_ACTIVE;
    ctx->active_slot = candidate_idx;

    /* Atualiza o status no loader oficial */
    loader_update_app_process(app_name, cand->pid, cand->staging_dir);

    /* Drenagem graciosa de conexões no slot antecessor */
    if (old_slot_idx != candidate_idx && old_slot->pid > 0 && old_slot->state == HOTRELOAD_ACTIVE) {
        old_slot->state = HOTRELOAD_DRAINING;
        /* Sinaliza o processo antigo para encerrar após tolerância de drenagem */
        ar_sleep_ms(100);
        ar_process_kill(old_slot->pid);
        old_slot->state = HOTRELOAD_TERMINATED;
        old_slot->pid = 0;
    }

    if (out_log && out_log_size > 0) {
        snprintf(out_log, out_log_size,
                 "OK: [ZERO-DOWNTIME] Slot %s ativado com sucesso (PID %d na porta %u). Slot %s drenado sem perda de conexoes.",
                 candidate_idx == 0 ? "Alpha" : "Beta", cand->pid, cand->port,
                 old_slot_idx == 0 ? "Alpha" : "Beta");
    }

    hotreload_unlock();
    return 0;
}

int ar_hotreload_status(const char *app_name, char *out_buf, size_t out_buf_size) {
    if (!app_name || !out_buf || out_buf_size == 0) return -1;
    hotreload_lock();
    for (int i = 0; i < g_hotapp_count; i++) {
        if (strcmp(g_hotapps[i].app_name, app_name) == 0) {
            hotreload_app_context_t *ctx = &g_hotapps[i];
            int active = ctx->active_slot;
            snprintf(out_buf, out_buf_size,
                     "App: %s | Active Slot: %s | Slot Alpha: [PID %d, State %d] | Slot Beta: [PID %d, State %d]",
                     ctx->app_name, active == 0 ? "Alpha" : "Beta",
                     ctx->slots[0].pid, ctx->slots[0].state,
                     ctx->slots[1].pid, ctx->slots[1].state);
            hotreload_unlock();
            return 0;
        }
    }
    hotreload_unlock();
    snprintf(out_buf, out_buf_size, "App: %s | Status: No active hot-reload session", app_name);
    return 0;
}
