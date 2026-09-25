/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/hooks.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <errno.h>

#define ALRIOS_HOOK_MAX_PATH 1024
#define ALRIOS_HOOK_DEFAULT_TIMEOUT_MS 5000

static const char *hook_stage_to_string(alrios_hook_stage_t stage) {
    switch (stage) {
        case HOOK_PRE_BUILD:  return "pre-build";
        case HOOK_POST_BUILD: return "post-build";
        case HOOK_PRE_SWAP:   return "pre-swap";
        case HOOK_POST_SWAP:  return "post-swap";
        case HOOK_PRE_DRAIN:  return "pre-drain";
        case HOOK_POST_DRAIN: return "post-drain";
        default:              return NULL;
    }
}

static int hook_stage_aborts_on_fail(alrios_hook_stage_t stage) {
    switch (stage) {
        case HOOK_PRE_BUILD:  return 1;
        case HOOK_POST_BUILD: return 1;
        case HOOK_PRE_SWAP:   return 1;
        case HOOK_POST_SWAP:  return 0;
        case HOOK_PRE_DRAIN:  return 1;
        case HOOK_POST_DRAIN: return 0;
        default:              return 1;
    }
}

static int is_valid_app_id(const char *app_id) {
    if (!app_id || app_id[0] == '\0') {
        return 0;
    }
    size_t len = strlen(app_id);
    if (len > 255) {
        return 0;
    }
    for (size_t i = 0; i < len; i++) {
        char c = app_id[i];
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '.' || c == '-' || c == '_')) {
            return 0;
        }
    }
    return 1;
}

static int is_valid_slot_path(const char *slot_path) {
    if (!slot_path) {
        return 1;
    }
    size_t len = strlen(slot_path);
    if (len == 0 || len >= 512) {
        return 0;
    }
    if (strstr(slot_path, "..") != NULL) {
        return 0;
    }
    return 1;
}

static void free_namelist(struct dirent **namelist, int count) {
    if (!namelist) {
        return;
    }
    for (int i = 0; i < count; i++) {
        if (namelist[i]) {
            free(namelist[i]);
            namelist[i] = NULL;
        }
    }
    free(namelist);
}

static int execute_single_hook(const char *hook_path, const alrios_hook_inv_t *inv, int abort_on_fail) {
    uint32_t timeout_ms = (inv->timeout_ms > 0) ? inv->timeout_ms : ALRIOS_HOOK_DEFAULT_TIMEOUT_MS;

    pid_t pid = fork();
    if (pid < 0) {
        return abort_on_fail ? ALRIOS_HOOK_ERR_FAIL : ALRIOS_HOOK_OK;
    }

    if (pid == 0) {
        char env_stage[128];
        char env_app[512];
        char env_slot[ALRIOS_HOOK_MAX_PATH];

        const char *stage_name = hook_stage_to_string(inv->stage);
        if (!stage_name) {
            stage_name = "unknown";
        }

        (void)snprintf(env_stage, sizeof(env_stage), "ALRIOS_HOOK_STAGE=%s", stage_name);
        (void)snprintf(env_app, sizeof(env_app), "ALRIOS_APP_ID=%s", inv->app_id ? inv->app_id : "");
        (void)snprintf(env_slot, sizeof(env_slot), "ALRIOS_SLOT_PATH=%s", inv->slot_path ? inv->slot_path : "");

        char *child_envp[] = {
            env_stage,
            env_app,
            env_slot,
            "PATH=/usr/bin:/bin:/usr/sbin:/sbin",
            NULL
        };

        char *child_argv[] = {
            (char *)(uintptr_t)hook_path,
            NULL
        };

        execve(hook_path, child_argv, child_envp);
        _exit(127);
    }

    struct timespec start_time;
    if (clock_gettime(CLOCK_MONOTONIC, &start_time) != 0) {
        start_time.tv_sec = 0;
        start_time.tv_nsec = 0;
    }

    int status = 0;
    int child_done = 0;
    int timed_out = 0;

    while (!child_done) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            child_done = 1;
            break;
        }
        if (r < 0) {
            if (errno == EINTR) {
                continue;
            }
            break;
        }

        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == 0) {
            uint64_t elapsed_ms = (uint64_t)(now.tv_sec - start_time.tv_sec) * 1000ULL +
                                  (uint64_t)(now.tv_nsec - start_time.tv_nsec) / 1000000ULL;
            if (elapsed_ms >= timeout_ms) {
                timed_out = 1;
                break;
            }
        }

        struct timespec sleep_interval = { .tv_sec = 0, .tv_nsec = 5000000L };
        nanosleep(&sleep_interval, NULL);
    }

    if (timed_out) {
        kill(pid, SIGKILL);
        (void)waitpid(pid, &status, 0);
        return ALRIOS_HOOK_ERR_TIMEOUT;
    }

    if (child_done) {
        int failed = 0;
        if (WIFEXITED(status)) {
            if (WEXITSTATUS(status) != 0) {
                failed = 1;
            }
        } else if (WIFSIGNALED(status)) {
            failed = 1;
        }

        if (failed && abort_on_fail) {
            return ALRIOS_HOOK_ERR_FAIL;
        }
    }

    return ALRIOS_HOOK_OK;
}

static int execute_hooks_in_dir(const char *dir_path, const alrios_hook_inv_t *inv, int abort_on_fail) {
    struct stat st;
    if (stat(dir_path, &st) != 0 || !S_ISDIR(st.st_mode)) {
        return ALRIOS_HOOK_OK;
    }

    struct dirent **namelist = NULL;
    int count = scandir(dir_path, &namelist, NULL, alphasort);
    if (count < 0) {
        return ALRIOS_HOOK_OK;
    }

    int result = ALRIOS_HOOK_OK;
    for (int i = 0; i < count; i++) {
        if (!namelist[i]) {
            continue;
        }

        const char *name = namelist[i]->d_name;
        if (name[0] == '.') {
            continue;
        }

        char hook_path[ALRIOS_HOOK_MAX_PATH];
        int w = snprintf(hook_path, sizeof(hook_path), "%s/%s", dir_path, name);
        if (w < 0 || (size_t)w >= sizeof(hook_path)) {
            continue;
        }

        struct stat fst;
        if (stat(hook_path, &fst) != 0 || !S_ISREG(fst.st_mode)) {
            continue;
        }

        if (access(hook_path, X_OK) != 0) {
            continue;
        }

        int hook_res = execute_single_hook(hook_path, inv, abort_on_fail);
        if (hook_res != ALRIOS_HOOK_OK) {
            result = hook_res;
            break;
        }
    }

    free_namelist(namelist, count);
    return result;
}

int alrios_hook_execute(const alrios_hook_inv_t *inv) {
    if (!inv || !is_valid_app_id(inv->app_id)) {
        return ALRIOS_HOOK_ERR_FAIL;
    }

    const char *stage_name = hook_stage_to_string(inv->stage);
    if (!stage_name) {
        return ALRIOS_HOOK_ERR_FAIL;
    }

    if (!is_valid_slot_path(inv->slot_path)) {
        return ALRIOS_HOOK_ERR_FAIL;
    }

    int abort_on_fail = hook_stage_aborts_on_fail(inv->stage);

    if (inv->slot_path && inv->slot_path[0] != '\0') {
        char slot_hooks_dir[ALRIOS_HOOK_MAX_PATH];
        int w = snprintf(slot_hooks_dir, sizeof(slot_hooks_dir), "%s/hooks.d/%s", inv->slot_path, stage_name);
        if (w > 0 && (size_t)w < sizeof(slot_hooks_dir)) {
            int ret = execute_hooks_in_dir(slot_hooks_dir, inv, abort_on_fail);
            if (ret != ALRIOS_HOOK_OK) {
                return ret;
            }
        }
    }

    char sys_hooks_dir[ALRIOS_HOOK_MAX_PATH];
    int sw = snprintf(sys_hooks_dir, sizeof(sys_hooks_dir), "/etc/alrios/hooks.d/%s", stage_name);
    if (sw > 0 && (size_t)sw < sizeof(sys_hooks_dir)) {
        int ret = execute_hooks_in_dir(sys_hooks_dir, inv, abort_on_fail);
        if (ret != ALRIOS_HOOK_OK) {
            return ret;
        }
    }

    return ALRIOS_HOOK_OK;
}

