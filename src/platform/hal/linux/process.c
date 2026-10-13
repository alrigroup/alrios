/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "hal_private.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    pid_t *pids;
    int count;
    int capacity;
} process_group_t;

static void child_fail(int error_fd, int error_code) {
    int saved_errno = error_code;
    ssize_t written;

    do {
        written = write(error_fd, &saved_errno, sizeof(saved_errno));
    } while (written < 0 && errno == EINTR);
    (void)written;
    _exit(127);
}

static int await_exec_result(pid_t pid, int error_fd) {
    int child_errno = 0;
    ssize_t count;

    do {
        count = read(error_fd, &child_errno, sizeof(child_errno));
    } while (count < 0 && errno == EINTR);
    (void)close(error_fd);

    if (count == 0) {
        return (int)pid;
    }
    if (count == (ssize_t)sizeof(child_errno)) {
        int status;
        while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        }
        return -child_errno;
    }
    return (count < 0) ? -errno : -EIO;
}

static int spawn_common(const char *path,
                        int executable_fd,
                        char *const argv[],
                        char *const envp[],
                        const char *working_directory,
                        unsigned int flags,
                        unsigned long uid,
                        unsigned long gid) {
    int error_pipe[2];
    pid_t pid;

    if ((path == NULL && executable_fd < 0) ||
        (path != NULL && executable_fd >= 0)) {
        return -EINVAL;
    }
    if (pipe2(error_pipe, O_CLOEXEC) != 0) {
        return -errno;
    }

    pid = fork();
    if (pid < 0) {
        int saved_errno = errno;
        (void)close(error_pipe[0]);
        (void)close(error_pipe[1]);
        return -saved_errno;
    }

    if (pid == 0) {
        (void)close(error_pipe[0]);
        if ((flags & AR_PROCESS_FLAG_NEW_GROUP) != 0U && setpgid(0, 0) != 0) {
            child_fail(error_pipe[1], errno);
        }
        if ((flags & AR_PROCESS_FLAG_DETACHED) != 0U && setsid() < 0) {
            child_fail(error_pipe[1], errno);
        }
        if ((flags & AR_PROCESS_FLAG_NO_NEW_PRIVS) != 0U &&
            prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) != 0) {
            child_fail(error_pipe[1], errno);
        }
        if (working_directory != NULL && chdir(working_directory) != 0) {
            child_fail(error_pipe[1], errno);
        }
        if (gid > 0UL && setgid((gid_t)gid) != 0) {
            child_fail(error_pipe[1], errno);
        }
        if (uid > 0UL && setuid((uid_t)uid) != 0) {
            child_fail(error_pipe[1], errno);
        }
        if ((flags & AR_PROCESS_FLAG_NULL_STDIO) != 0U) {
            int null_fd = open("/dev/null", O_RDWR);
            if (null_fd < 0 || dup2(null_fd, STDIN_FILENO) < 0 ||
                dup2(null_fd, STDOUT_FILENO) < 0 ||
                dup2(null_fd, STDERR_FILENO) < 0) {
                child_fail(error_pipe[1], errno);
            }
            if (null_fd > STDERR_FILENO) {
                (void)close(null_fd);
            }
        }
        if ((flags & AR_PROCESS_FLAG_CLOSE_EXTRA_FDS) != 0U) {
            long max_fd = sysconf(_SC_OPEN_MAX);
            if (max_fd < 0 || max_fd > 65536) {
                max_fd = 4096;
            }
            for (int fd = STDERR_FILENO + 1; fd < (int)max_fd; ++fd) {
                if (fd != error_pipe[1] && fd != executable_fd) {
                    (void)close(fd);
                }
            }
        }

        if ((flags & AR_PROCESS_FLAG_RAW_FD) != 0U) {
            char *default_argv[] = {(char *)"alrios_process", NULL};
            char *default_envp[] = {NULL};
            fexecve(executable_fd, argv ? argv : default_argv,
                    envp ? envp : default_envp);
        } else if (envp != NULL) {
            execve(path, argv, envp);
        } else if ((flags & AR_PROCESS_FLAG_SEARCH_PATH) != 0U) {
            execvp(path, argv);
        } else {
            execv(path, argv);
        }
        child_fail(error_pipe[1], errno);
    }

    (void)close(error_pipe[1]);
    return await_exec_result(pid, error_pipe[0]);
}

int os_process_create(const char *path, char *const argv[]) {
    return os_process_spawn(path, argv, NULL, NULL, AR_PROCESS_FLAG_SEARCH_PATH);
}

int os_process_spawn(const char *path,
                     char *const argv[],
                     char *const envp[],
                     const char *working_directory,
                     unsigned int flags) {
    return spawn_common(path, -1, argv, envp, working_directory,
                        flags & ~AR_PROCESS_FLAG_RAW_FD, 0UL, 0UL);
}

int os_process_spawn_as(const char *path,
                        char *const argv[],
                        char *const envp[],
                        const char *working_directory,
                        unsigned int flags,
                        unsigned long uid,
                        unsigned long gid) {
    return spawn_common(path, -1, argv, envp, working_directory,
                        flags & ~AR_PROCESS_FLAG_RAW_FD, uid, gid);
}

int os_process_spawn_fd(int executable_fd,
                        char *const argv[],
                        char *const envp[],
                        unsigned int flags) {
    return spawn_common(NULL, executable_fd, argv, envp, NULL,
                        flags | AR_PROCESS_FLAG_RAW_FD, 0UL, 0UL);
}

int os_process_run_callback(int (*callback)(void *), void *arg) {
    int error_pipe[2];
    pid_t pid;

    if (!callback) return -EINVAL;
    if (pipe2(error_pipe, O_CLOEXEC) != 0) return -errno;

    pid = fork();
    if (pid < 0) {
        int saved = errno;
        (void)close(error_pipe[0]);
        (void)close(error_pipe[1]);
        return -saved;
    }
    if (pid == 0) {
        (void)close(error_pipe[0]);
        int rc = callback(arg);
        (void)close(error_pipe[1]);
        _exit(rc == 0 ? 0 : 1);
    }
    (void)close(error_pipe[1]);
    (void)close(error_pipe[0]);

    int status = 0;
    pid_t wait_result;
    do {
        wait_result = waitpid(pid, &status, 0);
    } while (wait_result < 0 && errno == EINTR);
    if (wait_result != pid) return -errno;
    return (WIFEXITED(status) && WEXITSTATUS(status) == 0) ? 0 : -1;
}

void *os_process_group_create(void) {
    process_group_t *pg = (process_group_t *)malloc(sizeof(process_group_t));
    if (!pg) return NULL;
    pg->pids = NULL;
    pg->count = 0;
    pg->capacity = 0;
    return (void *)pg;
}

int os_process_group_add(void *group, int pid) {
    if (!group || pid <= 0) return -EINVAL;
    process_group_t *pg = (process_group_t *)group;
    if (pg->count >= pg->capacity) {
        int new_cap = pg->capacity ? pg->capacity * 2 : 8;
        pid_t *new_pids = (pid_t *)realloc(pg->pids, (size_t)new_cap * sizeof(pid_t));
        if (!new_pids) return -ENOMEM;
        pg->pids = new_pids;
        pg->capacity = new_cap;
    }
    pg->pids[pg->count++] = (pid_t)pid;
    return 0;
}

int os_process_group_destroy(void *group) {
    if (!group) return -EINVAL;
    process_group_t *pg = (process_group_t *)group;
    for (int i = 0; i < pg->count; i++) {
        (void)kill(pg->pids[i], SIGTERM);
    }
    free(pg->pids);
    free(pg);
    return 0;
}

int os_process_wait_raw(int pid, int *status) {
    pid_t result;
    do {
        result = waitpid((pid_t)pid, status, 0);
    } while (result < 0 && errno == EINTR);
    return (result == (pid_t)pid) ? 0 : -errno;
}

int os_process_wait(int pid) {
    int status;
    int result = os_process_wait_raw(pid, &status);
    if (result != 0) return result;
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return -WTERMSIG(status);
    return -1;
}

int os_process_wait_raw_nohang(int pid, int *status) {
    pid_t result;
    do {
        result = waitpid((pid_t)pid, status, WNOHANG);
    } while (result < 0 && errno == EINTR);
    if (result == 0) return 0;
    if (result == (pid_t)pid) return 1;
    if (errno == ECHILD && kill((pid_t)pid, 0) == 0) return 0;
    return -errno;
}

int os_process_wait_nohang(int pid) {
    int status;
    return os_process_wait_raw_nohang(pid, &status) == 0 ? 0 : 1;
}

int os_process_wait_status(int pid, int *exit_code) {
    int status;
    pid_t r = waitpid((pid_t)pid, &status, WNOHANG);
    if (r == 0) return 0;
    if (r == -1) {
        if (errno == ECHILD && kill((pid_t)pid, 0) == 0) return 0;
        if (exit_code) *exit_code = 0;
        return 1;
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

int os_process_signal(int pid, int signal_kind, int process_group) {
    int os_signal;
    pid_t target = process_group ? -(pid_t)pid : (pid_t)pid;

    switch (signal_kind) {
        case AR_PROCESS_SIGNAL_TERM: os_signal = SIGTERM; break;
        case AR_PROCESS_SIGNAL_KILL: os_signal = SIGKILL; break;
        case AR_PROCESS_SIGNAL_HUP: os_signal = SIGHUP; break;
        case AR_PROCESS_SIGNAL_NONE: os_signal = 0; break;
        default: return -EINVAL;
    }
    if (kill(target, os_signal) != 0) return -errno;
    return 0;
}

int os_process_kill(int pid) {
    return os_process_signal(pid, AR_PROCESS_SIGNAL_TERM, 0);
}

int os_process_self(void) {
    return (int)getpid();
}
