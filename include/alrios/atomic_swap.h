/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_ATOMIC_SWAP_H
#define ALRIOS_ATOMIC_SWAP_H

#include <sys/types.h>

int alrios_atomic_symlink_swap(const char *target_dir, const char *symlink_current, pid_t master_pid);

#endif /* ALRIOS_ATOMIC_SWAP_H */
