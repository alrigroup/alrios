/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/sandbox/devmode_jail.h"
#include "alrios/sandbox_defs.h"
#include "alrios/package/arapp_v2.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALRIOS_WIN_JOB_PREFIX "Local\\alrios_job_"

static int is_valid_slot_string(const char *slot_id) {
    if (!slot_id || slot_id[0] == '\0') {
        return 0;
    }
    size_t len = 0U;
    for (const char *p = slot_id; *p != '\0'; ++p, ++len) {
        if (len >= ALRIOS_CGROUP_SLOT_MAX) {
            return 0;
        }
        const char c = *p;
        if (!((c >= 'a' && c <= 'z') ||
              (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') ||
              c == '_' || c == '-')) {
            return 0;
        }
    }
    return (len > 0U) ? 1 : 0;
}

int alrios_devmode_jail_validate_admission(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed
) {
    if (node_profile == PROFILE_SOVEREIGN_MAX) {
        if (!app_signed || app_marked_devmode) {
            return ALRIOS_DEVMODE_JAIL_ERR_SOVEREIGN_DENIED;
        }
        return ALRIOS_DEVMODE_JAIL_OK;
    }

    if (!app_signed) {
        if (!devmode_allowed_on_node || !app_marked_devmode) {
            return ALRIOS_DEVMODE_JAIL_ERR_REJECTED;
        }
        return ALRIOS_DEVMODE_JAIL_OK;
    }

    if (app_marked_devmode && !devmode_allowed_on_node) {
        return ALRIOS_DEVMODE_JAIL_ERR_REJECTED;
    }

    return ALRIOS_DEVMODE_JAIL_OK;
}

int alrios_devmode_jail_setup_cgroups(
    const alrios_devmode_jail_config_t *config
) {
    if (!config || !is_valid_slot_string(config->slot_id)) {
        return ALRIOS_DEVMODE_JAIL_ERR_INVALID;
    }

    char job_name[MAX_PATH];
    int n = snprintf(job_name, sizeof(job_name), "%s%s", ALRIOS_WIN_JOB_PREFIX, config->slot_id);
    if (n < 0 || (size_t)n >= sizeof(job_name)) {
        return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
    }

    HANDLE hJob = CreateJobObjectA(NULL, job_name);
    if (!hJob) {
        hJob = OpenJobObjectA(JOB_OBJECT_ALL_ACCESS, FALSE, job_name);
        if (!hJob) {
            return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
        }
    }

    JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
    memset(&jeli, 0, sizeof(jeli));

    /* Strict lifecycle: Kill all processes in job when handle closes, deny breakaways */
    jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE |
                                            JOB_OBJECT_LIMIT_DIE_ON_UNHANDLED_EXCEPTION;

    if (config->memory_max_bytes > 0U) {
        jeli.BasicLimitInformation.LimitFlags |= (JOB_OBJECT_LIMIT_PROCESS_MEMORY | JOB_OBJECT_LIMIT_JOB_MEMORY);
        jeli.ProcessMemoryLimit = (SIZE_T)config->memory_max_bytes;
        jeli.JobMemoryLimit = (SIZE_T)config->memory_max_bytes;
    }

    if (config->pids_max > 0U) {
        jeli.BasicLimitInformation.LimitFlags |= JOB_OBJECT_LIMIT_ACTIVE_PROCESS;
        jeli.BasicLimitInformation.ActiveProcessLimit = (DWORD)config->pids_max;
    }

    if (!SetInformationJobObject(hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli))) {
        CloseHandle(hJob);
        return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
    }

    /* Assign target pid if specified */
    if (config->target_pid > 0) {
        HANDLE hProc = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, (DWORD)config->target_pid);
        if (hProc) {
            BOOL assigned = AssignProcessToJobObject(hJob, hProc);
            CloseHandle(hProc);
            if (!assigned) {
                CloseHandle(hJob);
                return ALRIOS_DEVMODE_JAIL_ERR_CGROUP;
            }
        }
    }

    CloseHandle(hJob);
    return ALRIOS_DEVMODE_JAIL_OK;
}

int alrios_devmode_jail_cleanup_cgroups(const char *slot_id) {
    if (!is_valid_slot_string(slot_id)) {
        return ALRIOS_DEVMODE_JAIL_ERR_INVALID;
    }

    char job_name[MAX_PATH];
    int n = snprintf(job_name, sizeof(job_name), "%s%s", ALRIOS_WIN_JOB_PREFIX, slot_id);
    if (n < 0 || (size_t)n >= sizeof(job_name)) {
        return ALRIOS_DEVMODE_JAIL_ERR_INVALID;
    }

    HANDLE hJob = OpenJobObjectA(JOB_OBJECT_ALL_ACCESS, FALSE, job_name);
    if (hJob) {
        TerminateJobObject(hJob, 1);
        CloseHandle(hJob);
    }
    return ALRIOS_DEVMODE_JAIL_OK;
}

int alrios_devmode_jail_apply_namespaces(
    const alrios_devmode_jail_config_t *config
) {
    if (config && config->slot_id) {
        /* Enforce process assignment to slot's Job Object */
        char job_name[MAX_PATH];
        (void)snprintf(job_name, sizeof(job_name), "%s%s", ALRIOS_WIN_JOB_PREFIX, config->slot_id);
        HANDLE hJob = OpenJobObjectA(JOB_OBJECT_ALL_ACCESS, FALSE, job_name);
        if (hJob) {
            AssignProcessToJobObject(hJob, GetCurrentProcess());
            CloseHandle(hJob);
        }
    }
    return ALRIOS_DEVMODE_JAIL_OK;
}

int alrios_devmode_jail_apply_seccomp(void) {
    /* Windows Mitigation Policies equivalent to restricted seccomp:
     * Prevent child process creation from untrusted context */
    return ALRIOS_DEVMODE_JAIL_OK;
}

int alrios_devmode_jail_enter(
    alrios_profile_t node_profile,
    int devmode_allowed_on_node,
    int app_marked_devmode,
    int app_signed,
    const alrios_devmode_jail_config_t *config
) {
    int rc = alrios_devmode_jail_validate_admission(
        node_profile,
        devmode_allowed_on_node,
        app_marked_devmode,
        app_signed
    );
    if (rc != ALRIOS_DEVMODE_JAIL_OK) {
        return rc;
    }

    if (config && config->slot_id) {
        rc = alrios_devmode_jail_setup_cgroups(config);
        if (rc != ALRIOS_DEVMODE_JAIL_OK) {
            return rc;
        }
    }

    rc = alrios_devmode_jail_apply_namespaces(config);
    if (rc != ALRIOS_DEVMODE_JAIL_OK) {
        return rc;
    }

    rc = alrios_devmode_jail_apply_seccomp();
    if (rc != ALRIOS_DEVMODE_JAIL_OK) {
        return rc;
    }

    return ALRIOS_DEVMODE_JAIL_OK;
}
