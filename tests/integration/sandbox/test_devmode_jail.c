/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "alrios/sandbox/devmode_jail.h"
#include "alrios/sandbox_defs.h"
#include "alrios/profiles.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(cond) do { \
    if (!(cond)) { \
        (void)fprintf(stderr, "CHECK failed at %s:%d: %s\n", \
                      __FILE__, __LINE__, #cond); \
        exit(EXIT_FAILURE); \
    } \
} while (0)

static void test_admission_control(void) {
    /* 1. Sovereign profile: fails closed for any unsigned app */
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_SOVEREIGN_MAX, 1, 1, 0) == ALRIOS_DEVMODE_JAIL_ERR_SOVEREIGN_DENIED);

    /* 2. Sovereign profile: rejects even signed app if marked DevMode */
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_SOVEREIGN_MAX, 0, 1, 1) == ALRIOS_DEVMODE_JAIL_ERR_SOVEREIGN_DENIED);
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_SOVEREIGN_MAX, 1, 1, 1) == ALRIOS_DEVMODE_JAIL_ERR_SOVEREIGN_DENIED);

    /* 3. Sovereign profile: allows signed app not marked devmode */
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_SOVEREIGN_MAX, 0, 0, 1) == ALRIOS_DEVMODE_JAIL_OK);

    /* 4. Enterprise profile: devmode disabled on node -> unsigned app rejected */
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_ENTERPRISE_BAL, 0, 1, 0) == ALRIOS_DEVMODE_JAIL_ERR_REJECTED);

    /* 5. Enterprise profile: devmode enabled on node, but app not marked devmode -> rejected */
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_ENTERPRISE_BAL, 1, 0, 0) == ALRIOS_DEVMODE_JAIL_ERR_REJECTED);

    /* 6. Enterprise profile: devmode allowed on node AND app marked devmode -> accepted */
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_ENTERPRISE_BAL, 1, 1, 0) == ALRIOS_DEVMODE_JAIL_OK);

    /* 7. Enterprise profile: signed app always allowed if not devmode */
    CHECK(alrios_devmode_jail_validate_admission(
        PROFILE_ENTERPRISE_BAL, 0, 0, 1) == ALRIOS_DEVMODE_JAIL_OK);

    /* 8. DevMode policy API struct evaluation */
    alrios_devmode_policy_t policy;
    alrios_devmode_request_t request;
    alrios_devmode_admission_t admission;

    (void)memset(&policy, 0, sizeof(policy));
    (void)memset(&request, 0, sizeof(request));
    (void)memset(&admission, 0, sizeof(admission));

    policy.abi_version = ALRIOS_DEVMODE_POLICY_ABI_VERSION;
    policy.deployment = ALRIOS_DEPLOYMENT_PRODUCTION;
    policy.devmode_enabled = 0;
    request.artifact_trust = ALRIOS_ARTIFACT_UNSIGNED;
    request.requested_ring = ALRIOS_EXECUTION_RING_DEVMODE;

    /* Production node: unsigned rejected */
    CHECK(alrios_devmode_policy_evaluate(&policy, &request, &admission) ==
          ALRIOS_DEVMODE_POLICY_ERR_DISABLED);

    /* Dev node: devmode disabled -> rejected */
    policy.deployment = ALRIOS_DEPLOYMENT_DEVELOPMENT;
    policy.devmode_enabled = 0;
    CHECK(alrios_devmode_policy_evaluate(NULL, &request, &admission) ==
          ALRIOS_DEVMODE_POLICY_ERR_INVALID);
    CHECK(alrios_devmode_policy_evaluate(&policy, &request, &admission) ==
          ALRIOS_DEVMODE_POLICY_ERR_DISABLED);

    /* Dev node: devmode enabled, requesting Sovereign ring for unsigned -> denied */
    policy.devmode_enabled = 1;
    request.requested_ring = ALRIOS_EXECUTION_RING_SOVEREIGN;
    CHECK(alrios_devmode_policy_evaluate(&policy, &request, &admission) ==
          ALRIOS_DEVMODE_POLICY_ERR_UNSIGNED_SOVEREIGN);

    /* Dev node: devmode enabled, requesting DevMode ring for unsigned -> OK */
    request.requested_ring = ALRIOS_EXECUTION_RING_DEVMODE;
    CHECK(alrios_devmode_policy_evaluate(&policy, &request, &admission) ==
          ALRIOS_DEVMODE_POLICY_OK);
    CHECK(admission.execution_ring == ALRIOS_EXECUTION_RING_DEVMODE);
}

static void test_cgroups_and_limits(void) {
    const char *slot = "devmode_slot_01";
    pid_t current_pid = getpid();

    alrios_devmode_jail_config_t config;
    (void)memset(&config, 0, sizeof(config));
    config.abi_version = ALRIOS_DEVMODE_JAIL_ABI_VERSION;
    config.slot_id = slot;
    config.target_pid = current_pid;
    config.memory_max_bytes = 128 * 1024 * 1024; /* 128MB */
    config.pids_max = 16;
    config.net_mode = ALRIOS_DEVMODE_NET_LOOPBACK_ONLY;

    CHECK(alrios_devmode_jail_setup_cgroups(&config) == ALRIOS_DEVMODE_JAIL_OK);

    const char *root = alrios_sandbox_get_cgroup_root();
    CHECK(root != NULL);

    char path[PATH_MAX];
    char buf[128];
    int n = snprintf(path, sizeof(path), "%s/%s/cpu.max", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    int fd = open(path, O_RDONLY);
    CHECK(fd >= 0);
    ssize_t rd = read(fd, buf, sizeof(buf) - 1);
    CHECK(rd > 0);
    buf[rd] = '\0';
    (void)close(fd);
    CHECK(strstr(buf, "50000 100000") != NULL);

    n = snprintf(path, sizeof(path), "%s/%s/memory.max", root, slot);
    CHECK(n > 0 && (size_t)n < sizeof(path));
    fd = open(path, O_RDONLY);
    CHECK(fd >= 0);
    rd = read(fd, buf, sizeof(buf) - 1);
    CHECK(rd > 0);
    buf[rd] = '\0';
    (void)close(fd);
    CHECK(strstr(buf, "134217728") != NULL);

    CHECK(alrios_devmode_jail_cleanup_cgroups(slot) == ALRIOS_DEVMODE_JAIL_OK);
}

static void test_seccomp_filter_enforcement(void) {
    /* 1. Test blacklisted syscalls trigger SIGSYS */
    pid_t pid = fork();
    CHECK(pid >= 0);

    if (pid == 0) {
        if (alrios_devmode_jail_apply_seccomp() != ALRIOS_DEVMODE_JAIL_OK) {
            _exit(1);
        }
        /* Call blocked syscall SYS_ptrace -> must receive SIGSYS */
        syscall(SYS_ptrace, 0, 0, NULL, NULL);
        _exit(99);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGSYS);

    /* 2. Test SYS_bpf triggers SIGSYS */
    pid = fork();
    CHECK(pid >= 0);

    if (pid == 0) {
        if (alrios_devmode_jail_apply_seccomp() != ALRIOS_DEVMODE_JAIL_OK) {
            _exit(1);
        }
        syscall(SYS_bpf, 0, NULL, 0);
        _exit(99);
    }

    waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGSYS);

    /* 3. Test SYS_mount triggers SIGSYS */
    pid = fork();
    CHECK(pid >= 0);

    if (pid == 0) {
        if (alrios_devmode_jail_apply_seccomp() != ALRIOS_DEVMODE_JAIL_OK) {
            _exit(1);
        }
        syscall(SYS_mount, NULL, NULL, NULL, 0, NULL);
        _exit(99);
    }

    waitpid(pid, &status, 0);
    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGSYS);

    /* 4. Test blocked socket types: RAW and AF_PACKET return EPERM */
    pid = fork();
    CHECK(pid >= 0);

    if (pid == 0) {
        if (alrios_devmode_jail_apply_seccomp() != ALRIOS_DEVMODE_JAIL_OK) {
            _exit(1);
        }
        errno = 0;
        int s = socket(AF_INET, SOCK_RAW, 0);
        if (s != -1 || errno != EPERM) {
            _exit(2);
        }

        errno = 0;
        s = socket(17, SOCK_RAW, 0); /* AF_PACKET */
        if (s != -1 || errno != EPERM) {
            _exit(3);
        }

        errno = 0;
        s = socket(16, SOCK_RAW, 0); /* AF_NETLINK */
        if (s != -1 || errno != EPERM) {
            _exit(4);
        }

        /* Allowed socket e.g. UNIX or UDP */
        s = socket(AF_UNIX, SOCK_STREAM, 0);
        if (s < 0) {
            _exit(5);
        }
        (void)close(s);

        _exit(0);
    }

    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

static void test_namespaces_and_network_containment(void) {
    pid_t pid = fork();
    CHECK(pid >= 0);

    if (pid == 0) {
        alrios_devmode_jail_config_t config;
        (void)memset(&config, 0, sizeof(config));
        config.abi_version = ALRIOS_DEVMODE_JAIL_ABI_VERSION;
        config.net_mode = ALRIOS_DEVMODE_NET_LOOPBACK_ONLY;

        if (alrios_devmode_jail_apply_namespaces(&config) != ALRIOS_DEVMODE_JAIL_OK) {
            _exit(1);
        }

        /* Loopback socket connect to 127.0.0.1 UDP should succeed */
        int s = socket(AF_INET, SOCK_DGRAM, 0);
        if (s < 0) {
            _exit(2);
        }

        struct sockaddr_in sin;
        (void)memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons(8080);
        sin.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

        if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) != 0) {
            (void)close(s);
            _exit(3);
        }
        (void)close(s);

        /* External connection to non-loopback must fail with ENETUNREACH in isolated netns */
        s = socket(AF_INET, SOCK_DGRAM, 0);
        sin.sin_addr.s_addr = htonl(0x08080808); /* 8.8.8.8 */
        errno = 0;
        if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) == 0) {
            (void)close(s);
            _exit(4);
        }
        if (errno != ENETUNREACH && errno != EPERM) {
            (void)close(s);
            _exit(5);
        }
        (void)close(s);

        _exit(0);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    CHECK(WIFEXITED(status));
    CHECK(WEXITSTATUS(status) == 0);
}

static void test_jail_enter_full_flow(void) {
    pid_t pid = fork();
    CHECK(pid >= 0);

    if (pid == 0) {
        alrios_devmode_jail_config_t config;
        (void)memset(&config, 0, sizeof(config));
        config.abi_version = ALRIOS_DEVMODE_JAIL_ABI_VERSION;
        config.slot_id = "test_slot_full";
        config.target_pid = getpid();
        config.memory_max_bytes = 64 * 1024 * 1024;
        config.pids_max = 16;
        config.net_mode = ALRIOS_DEVMODE_NET_LOOPBACK_ONLY;

        /* In Enterprise profile with DevMode allowed and app marked devmode */
        int rc = alrios_devmode_jail_enter(
            PROFILE_ENTERPRISE_BAL,
            1, /* devmode allowed on node */
            1, /* app marked devmode */
            0, /* app unsigned */
            &config
        );
        if (rc != ALRIOS_DEVMODE_JAIL_OK) {
            _exit(1);
        }

        /* Verify jail holds: ptrace blocked */
        syscall(SYS_ptrace, 0, 0, NULL, NULL);
        _exit(99);
    }

    int status = 0;
    waitpid(pid, &status, 0);
    /* Child must have been killed by SIGSYS */
    CHECK(WIFSIGNALED(status));
    CHECK(WTERMSIG(status) == SIGSYS);

    (void)alrios_devmode_jail_cleanup_cgroups("test_slot_full");
}

int main(void) {
    test_admission_control();
    test_cgroups_and_limits();
    test_seccomp_filter_enforcement();
    test_namespaces_and_network_containment();
    test_jail_enter_full_flow();

    (void)puts("test_devmode_jail: PASS");
    return EXIT_SUCCESS;
}
