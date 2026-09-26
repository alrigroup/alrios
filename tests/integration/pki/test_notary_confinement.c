/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/pki/notary.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef _WIN32
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#define CHECK(condition) do { \
    if (!(condition)) { \
        (void)fprintf(stderr, "FAIL: %s at %s:%d\n", #condition, __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

int main(void) {
#ifdef _WIN32
    int rc = alrios_notary_enable_noexec_confinement();
    CHECK(rc != 0);
    (void)printf("[PASS] noexec confinement helper fails closed on unsupported OS (rc=%d)\n", rc);
    return 0;
#else
    pid_t child = fork();
    int status = 0;

    CHECK(child >= 0);
    if (child == 0) {
        char *const argv[] = { (char *)"true", NULL };
        int rc = alrios_notary_enable_noexec_confinement();
        if (rc != 0) {
            _exit(100);
        }
        execv("/bin/true", argv);
        _exit(101);
    }

    CHECK(waitpid(child, &status, 0) == child);
    CHECK(!WIFEXITED(status) || WEXITSTATUS(status) != 0);
    CHECK(WIFSIGNALED(status) || (WIFEXITED(status) && WEXITSTATUS(status) == 101));

    (void)printf("[PASS] notary noexec confinement blocks compiler/process exec attempts\n");
    return 0;
#endif
}
