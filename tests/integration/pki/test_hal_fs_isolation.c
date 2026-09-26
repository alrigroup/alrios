/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "aros_hal.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

#ifndef _WIN32
static void write_or_die(const char *path, const char *data) {
    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0600);
    CHECK(fd >= 0);
    CHECK(write(fd, data, strlen(data)) == (ssize_t)strlen(data));
    CHECK(close(fd) == 0);
}

static int try_read_file(const char *path) {
    char buf[16];
    ssize_t n;
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    n = read(fd, buf, sizeof(buf));
    if (n < 0) {
        (void)close(fd);
        return -1;
    }
    CHECK(close(fd) == 0);
    return 0;
}

static void join_path_or_die(char *out, size_t out_cap, const char *dir, const char *name) {
    size_t dir_len = strlen(dir);
    size_t name_len = strlen(name);
    CHECK(dir_len + 1U + name_len + 1U <= out_cap);
    memcpy(out, dir, dir_len);
    out[dir_len] = '/';
    memcpy(out + dir_len + 1U, name, name_len);
    out[dir_len + 1U + name_len] = '\0';
}
#endif

int main(void) {
#ifdef _WIN32
    const char *roots[] = { "." };
    int rc = ar_fs_restrict_to_paths(roots, 1U, roots, 1U);
    CHECK(rc != 0);
    (void)printf("[PASS] HAL fs isolation fails closed on unsupported OS (rc=%d)\n", rc);
    return 0;
#else
    char tmpl[] = "/tmp/alrios-hal-iso-XXXXXX";
    char *base = mkdtemp(tmpl);
    char allowed[4096];
    char denied[4096];
    pid_t child;
    int status = 0;

    CHECK(base != NULL);
    join_path_or_die(allowed, sizeof(allowed), base, "allowed");
    join_path_or_die(denied, sizeof(denied), base, "denied");
    CHECK(mkdir(allowed, 0700) == 0);
    CHECK(mkdir(denied, 0700) == 0);

    {
        char allowed_file[4096];
        char denied_file[4096];
        join_path_or_die(allowed_file, sizeof(allowed_file), allowed, "public.txt");
        join_path_or_die(denied_file, sizeof(denied_file), denied, "private.key");
        write_or_die(allowed_file, "public");
        write_or_die(denied_file, "secret");
    }

    child = fork();
    CHECK(child >= 0);
    if (child == 0) {
        const char *read_roots[] = { allowed };
        char allowed_file[4096];
        char denied_file[4096];
        int rc;

        join_path_or_die(allowed_file, sizeof(allowed_file), allowed, "public.txt");
        join_path_or_die(denied_file, sizeof(denied_file), denied, "private.key");

        rc = ar_fs_restrict_to_paths(read_roots, 1U, NULL, 0U);
        if (rc != 0) {
            _exit(100);
        }
        if (try_read_file(allowed_file) != 0) {
            _exit(101);
        }
        if (try_read_file(denied_file) == 0) {
            _exit(102);
        }
        _exit(0);
    }

    CHECK(waitpid(child, &status, 0) == child);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0 || WEXITSTATUS(status) == 100);
    if (WEXITSTATUS(status) == 100) {
        (void)printf("[SKIP] Landlock unavailable; HAL isolation failed closed\n");
        return 0;
    }

    (void)printf("[PASS] HAL filesystem isolation permits declared root and denies external key path\n");
    return 0;
#endif
}
