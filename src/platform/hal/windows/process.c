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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static size_t quoted_arg_size(const char *arg) {
    size_t size = 3U;
    size_t backslashes = 0U;
    for (const char *p = arg; *p != '\0'; ++p) {
        if (*p == '\\') {
            ++backslashes;
        } else {
            if (*p == '"') size += backslashes + 1U;
            backslashes = 0U;
        }
        ++size;
    }
    return size + backslashes;
}

static int append_quoted_arg(char **cursor, size_t *remaining, const char *arg) {
    char *out = *cursor;
    size_t rem = *remaining;
    size_t backslashes = 0U;

    if (rem < 2U) return -EINVAL;
    *out++ = '"';
    --rem;
    for (const char *p = arg; ; ++p) {
        if (*p == '\\') {
            ++backslashes;
            continue;
        }
        if (*p == '"' || *p == '\0') {
            size_t required = backslashes * ((*p == '"') ? 2U : 2U);
            if (*p == '"') ++required;
            if (required >= rem) return -EINVAL;
            for (size_t i = 0U; i < required; ++i) *out++ = '\\';
            rem -= required;
            backslashes = 0U;
            if (*p == '\0') break;
            *out++ = '"';
            --rem;
            continue;
        }
        if (backslashes >= rem) return -EINVAL;
        while (backslashes > 0U) {
            *out++ = '\\';
            --rem;
            --backslashes;
        }
        if (rem < 2U) return -EINVAL;
        *out++ = *p;
        --rem;
    }
    if (rem < 2U) return -EINVAL;
    *out++ = '"';
    *out = '\0';
    --rem;
    *cursor = out;
    *remaining = rem;
    return 0;
}

static char *build_command_line(const char *path, char *const argv[]) {
    size_t length = quoted_arg_size(path);
    int start = (argv != NULL && argv[0] != NULL) ? 1 : 0;

    if (argv != NULL) {
        for (int i = start; argv[i] != NULL; ++i) {
            length += 1U + quoted_arg_size(argv[i]);
        }
    }

    char *command_line = (char *)malloc(length);
    if (command_line == NULL) return NULL;
    char *cursor = command_line;
    size_t remaining = length;
    if (append_quoted_arg(&cursor, &remaining, path) != 0) {
        free(command_line);
        return NULL;
    }
    if (argv != NULL) {
        for (int i = start; argv[i] != NULL; ++i) {
            if (remaining < 2U) {
                free(command_line);
                return NULL;
            }
            *cursor++ = ' ';
            --remaining;
            if (append_quoted_arg(&cursor, &remaining, argv[i]) != 0) {
                free(command_line);
                return NULL;
            }
        }
    }
    return command_line;
}

int os_process_create(const char *path, char *const argv[]) {
    return os_process_spawn(path, argv, NULL, NULL, AR_PROCESS_FLAG_SEARCH_PATH);
}

int os_process_spawn(const char *path,
                     char *const argv[],
                     char *const envp[],
                     const char *working_directory,
                     unsigned int flags) {
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD creation_flags = 0U;
    char *command_line;
    HANDLE null_handle = INVALID_HANDLE_VALUE;
    BOOL ok;

    if (envp != NULL || (flags & (AR_PROCESS_FLAG_NO_NEW_PRIVS |
                                  AR_PROCESS_FLAG_RAW_FD)) != 0U) {
        return -ENOTSUP;
    }
    if ((flags & AR_PROCESS_FLAG_CLOSE_EXTRA_FDS) != 0U &&
        (flags & AR_PROCESS_FLAG_NULL_STDIO) == 0U) {
        return -ENOTSUP;
    }
    command_line = build_command_line(path, argv);
    if (command_line == NULL) return -ENOMEM;

    memset(&si, 0, sizeof(si));
    memset(&pi, 0, sizeof(pi));
    si.cb = sizeof(si);
    if ((flags & AR_PROCESS_FLAG_NEW_GROUP) != 0U) {
        creation_flags |= CREATE_NEW_PROCESS_GROUP;
    }
    if ((flags & AR_PROCESS_FLAG_DETACHED) != 0U) {
        creation_flags |= DETACHED_PROCESS;
    }
    if ((flags & AR_PROCESS_FLAG_NULL_STDIO) != 0U) {
        SECURITY_ATTRIBUTES attributes;
        memset(&attributes, 0, sizeof(attributes));
        attributes.nLength = sizeof(attributes);
        attributes.bInheritHandle = TRUE;
        null_handle = CreateFileA("NUL", GENERIC_READ | GENERIC_WRITE,
                                  FILE_SHARE_READ | FILE_SHARE_WRITE,
                                  &attributes, OPEN_EXISTING, 0, NULL);
        if (null_handle == INVALID_HANDLE_VALUE) {
            free(command_line);
            return -EIO;
        }
        si.dwFlags |= STARTF_USESTDHANDLES;
        si.hStdInput = null_handle;
        si.hStdOutput = null_handle;
        si.hStdError = null_handle;
    }

    ok = CreateProcessA(NULL, command_line, NULL, NULL,
                        null_handle != INVALID_HANDLE_VALUE, creation_flags,
                        NULL, working_directory, &si, &pi);
    if (null_handle != INVALID_HANDLE_VALUE) CloseHandle(null_handle);
    free(command_line);
    if (!ok) return -EIO;

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)pi.dwProcessId;
}

int os_process_spawn_as(const char *path,
                        char *const argv[],
                        char *const envp[],
                        const char *working_directory,
                        unsigned int flags,
                        unsigned long uid,
                        unsigned long gid) {
    if (uid != 0UL || gid != 0UL) return -ENOTSUP;
    return os_process_spawn(path, argv, envp, working_directory, flags);
}

int os_process_spawn_fd(int executable_fd,
                        char *const argv[],
                        char *const envp[],
                        unsigned int flags) {
    (void)executable_fd;
    (void)argv;
    (void)envp;
    (void)flags;
    return -ENOTSUP;
}

int os_process_run_callback(int (*callback)(void *), void *arg) {
    (void)callback;
    (void)arg;
    return -ENOTSUP;
}

int os_process_wait(int pid) {
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    DWORD code = 0;
    if (!h) return -1;
    if (WaitForSingleObject(h, INFINITE) != WAIT_OBJECT_0 ||
        !GetExitCodeProcess(h, &code)) {
        CloseHandle(h);
        return -1;
    }
    CloseHandle(h);
    return (int)code;
}

int os_process_wait_raw(int pid, int *status) {
    int code = os_process_wait(pid);
    if (code < 0) return code;
    *status = code;
    return 0;
}

int os_process_wait_raw_nohang(int pid, int *status) {
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    DWORD code = 0;
    DWORD result;
    if (!h) return -1;
    result = WaitForSingleObject(h, 0);
    if (result == WAIT_TIMEOUT) {
        CloseHandle(h);
        return 0;
    }
    if (result != WAIT_OBJECT_0 || !GetExitCodeProcess(h, &code)) {
        CloseHandle(h);
        return -1;
    }
    CloseHandle(h);
    *status = (int)code;
    return 1;
}

int os_process_wait_nohang(int pid) {
    int status;
    return os_process_wait_raw_nohang(pid, &status) == 0 ? 0 : 1;
}

int os_process_wait_status(int pid, int *exit_code) {
    HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, (DWORD)pid);
    if (!h) return 1;
    DWORD r = WaitForSingleObject(h, 0);
    if (r == WAIT_OBJECT_0) {
        DWORD c = 0;
        BOOL ok = GetExitCodeProcess(h, &c);
        CloseHandle(h);
        if (!ok) return -1;
        if (exit_code) *exit_code = (int)c;
        return 1;
    }
    CloseHandle(h);
    return 0;
}

int os_process_signal(int pid, int signal_kind, int process_group) {
    HANDLE h;
    BOOL ok;
    (void)process_group;

    if (signal_kind == AR_PROCESS_SIGNAL_NONE) {
        h = OpenProcess(SYNCHRONIZE, FALSE, (DWORD)pid);
        if (!h) return -1;
        DWORD state = WaitForSingleObject(h, 0);
        CloseHandle(h);
        return (state == WAIT_TIMEOUT) ? 0 : -1;
    }
    if (signal_kind == AR_PROCESS_SIGNAL_HUP) return -ENOTSUP;
    if (signal_kind != AR_PROCESS_SIGNAL_TERM &&
        signal_kind != AR_PROCESS_SIGNAL_KILL) return -EINVAL;

    h = OpenProcess(PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!h) return -1;
    ok = TerminateProcess(h, 1);
    CloseHandle(h);
    return ok ? 0 : -1;
}

int os_process_kill(int pid) {
    return os_process_signal(pid, AR_PROCESS_SIGNAL_TERM, 0);
}

int os_process_self(void) {
    return (int)GetCurrentProcessId();
}

void *os_process_group_create(void) {
    HANDLE job = CreateJobObjectA(NULL, NULL);
    if (!job) return NULL;

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info;
    memset(&info, 0, sizeof(info));
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(job, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(job);
        return NULL;
    }
    return (void *)job;
}

int os_process_group_add(void *group, int pid) {
    HANDLE job = (HANDLE)group;
    HANDLE proc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, (DWORD)pid);
    if (!proc) return -1;
    BOOL ok = AssignProcessToJobObject(job, proc);
    CloseHandle(proc);
    return ok ? 0 : -1;
}

int os_process_group_destroy(void *group) {
    if (!group) return -EINVAL;
    CloseHandle((HANDLE)group);
    return 0;
}
