/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * Proprietary and confidential. Unauthorized copying is prohibited.
 * ==================================================================== */

#ifndef ALRIOS_SANDBOX_DEFS_H
#define ALRIOS_SANDBOX_DEFS_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>

#define ALRIOS_SANDBOX_OK                 0
#define ALRIOS_SANDBOX_ERR_PRCTL_FAIL    -2001
#define ALRIOS_SANDBOX_ERR_SECCOMP_FAIL  -2002
#define ALRIOS_SANDBOX_ERR_UNSHARE_FAIL  -2003
#define ALRIOS_SANDBOX_ERR_CGROUP_FAIL   -2004

#define ALRIOS_CGROUP_RAM_CAP_BYTES      (256 * 1024 * 1024) /* 256MB */
#define ALRIOS_CGROUP_SWAP_MAX_BYTES     0                   /* 0 Swap */
#define ALRIOS_CGROUP_PIDS_MAX           32
#define ALRIOS_CGROUP_SLOT_MAX           64
#define ALRIOS_CGROUP_DEFAULT_ROOT       "/sys/fs/cgroup/alrios"

int alrios_sandbox_apply_seccomp(void);
int alrios_sandbox_setup_cgroups(const char *slot_id, pid_t pid);
int alrios_sandbox_set_cgroup_root(const char *root_path);
const char *alrios_sandbox_get_cgroup_root(void);
int alrios_sandbox_cleanup_cgroups(const char *slot_id);
int alrios_sandbox_enter_jail(const char *slot_id, pid_t pid);

#endif /* ALRIOS_SANDBOX_DEFS_H */
