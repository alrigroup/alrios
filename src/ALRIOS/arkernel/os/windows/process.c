/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

static int os_process_create(const char *path, char *const argv[]) {
    /* skip argv[0] (program name) since 'path' serves that role */
    int start = (argv && argv[0]) ? 1 : 0;
    size_t cmdlen = strlen(path) + 4;
    if (argv) {
        for (int i = start; argv[i]; i++)
            cmdlen += strlen(argv[i]) + 4;
    }

    char *cmd = malloc(cmdlen);
    if (!cmd) return -ENOMEM;

    char *p = cmd;
    p += sprintf(p, "\"%s\"", path);
    if (argv) {
        for (int i = start; argv[i]; i++)
            p += sprintf(p, " \"%s\"", argv[i]);
    }

    STARTUPINFOA si = {0};
    PROCESS_INFORMATION pi = {0};
    si.cb = sizeof(si);

    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
    free(cmd);
    if (!ok) return -1;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)pi.dwProcessId;
}

static int os_process_wait(int pid) {
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return -1;
    WaitForSingleObject(h, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(h, &code);
    CloseHandle(h);
    return (int)code;
}

static int os_process_wait_nohang(int pid) {
    HANDLE h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
    if (!h) return 1; /* cannot open -> gone */
    DWORD r = WaitForSingleObject(h, 0);
    CloseHandle(h);
    return (r == WAIT_OBJECT_0) ? 1 : 0;
}

static int os_process_wait_status(int pid, int *exit_code) {
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 1; /* gone, exit code unknown */
    DWORD r = WaitForSingleObject(h, 0);
    if (r == WAIT_OBJECT_0) {
        DWORD c = 0;
        GetExitCodeProcess(h, &c);
        CloseHandle(h);
        if (exit_code) *exit_code = (int)c;
        return 1;
    }
    CloseHandle(h);
    return 0;
}

static int os_process_kill(int pid) {
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!h) return -1;
    BOOL ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok ? 0 : -1;
}

static int os_process_self(void) {
    return (int)GetCurrentProcessId();
}

static void *os_process_group_create(void) {
    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (!job) return NULL;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {0};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return NULL;
    }

    return (void *)job;
}

static int os_process_group_add(void *group, int pid) {
    HANDLE job = (HANDLE)group;
    HANDLE proc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!proc) return -1;
    BOOL ok = AssignProcessToJobObject(job, proc);
    CloseHandle(proc);
    return ok ? 0 : -1;
}

static int os_process_group_destroy(void *group) {
    if (!group) return -EINVAL;
    HANDLE job = (HANDLE)group;
    CloseHandle(job);
    return 0;
}
