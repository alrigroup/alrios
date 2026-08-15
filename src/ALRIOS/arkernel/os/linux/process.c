/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>
#include <stdlib.h>

typedef struct {
    pid_t *pids;
    int count;
    int capacity;
} process_group_t;

static int os_process_create(const char *path, char *const argv[]) {
    pid_t pid = fork();
    if (pid == -1) return -errno;

    if (pid == 0) {
        execvp(path, argv);
        _exit(127);
    }

    return (int)pid;
}

static void *os_process_group_create(void) {
    process_group_t *pg = (process_group_t *)malloc(sizeof(process_group_t));
    if (!pg) return NULL;
    pg->pids = NULL;
    pg->count = 0;
    pg->capacity = 0;
    return (void *)pg;
}

static int os_process_group_add(void *group, int pid) {
    if (!group || pid <= 0) return -EINVAL;
    process_group_t *pg = (process_group_t *)group;
    if (pg->count >= pg->capacity) {
        int new_cap = pg->capacity ? pg->capacity * 2 : 8;
        pid_t *new_pids = (pid_t *)realloc(pg->pids, new_cap * sizeof(pid_t));
        if (!new_pids) return -ENOMEM;
        pg->pids = new_pids;
        pg->capacity = new_cap;
    }
    pg->pids[pg->count++] = (pid_t)pid;
    return 0;
}

static int os_process_group_destroy(void *group) {
    if (!group) return -EINVAL;
    process_group_t *pg = (process_group_t *)group;
    for (int i = 0; i < pg->count; i++)
        kill(pg->pids[i], SIGTERM);
    free(pg->pids);
    free(pg);
    return 0;
}

static int os_process_wait(int pid) {
    int status;
    if (waitpid((pid_t)pid, &status, 0) == -1)
        return -errno;
    if (WIFEXITED(status))
        return WEXITSTATUS(status);
    if (WIFSIGNALED(status))
        return -WTERMSIG(status);
    return -1;
}

static int os_process_wait_nohang(int pid) {
    int status;
    pid_t r = waitpid((pid_t)pid, &status, WNOHANG);
    if (r == 0) return 0; /* still running */
    if (r == -1) {
        /* Forks come from short-lived worker threads; until that thread
           exits, this thread cannot wait on the child (ECHILD). Treat a
           still-alive process as running rather than falsely CRASHED. */
        if (errno == ECHILD && kill((pid_t)pid, 0) == 0) return 0;
        return 1; /* gone (no such child) */
    }
    if (WIFEXITED(status)) return 1;
    if (WIFSIGNALED(status)) return 1;
    return 1;
}

static int os_process_wait_status(int pid, int *exit_code) {
    int status;
    pid_t r = waitpid((pid_t)pid, &status, WNOHANG);
    if (r == 0) return 0; /* still running */
    if (r == -1) {
        if (errno == ECHILD && kill((pid_t)pid, 0) == 0) return 0;
        if (exit_code) *exit_code = 0;
        return 1; /* gone, exit code unknown */
    }
    if (WIFEXITED(status)) {
        if (exit_code) *exit_code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
        if (exit_code) *exit_code = -WTERMSIG(status);
    } else if (exit_code) {
        *exit_code = 0;
    }
    return 1;
}

static int os_process_kill(int pid) {
    if (kill((pid_t)pid, SIGTERM) == -1)
        return -errno;
    return 0;
}

static int os_process_self(void) {
    return (int)getpid();
}
