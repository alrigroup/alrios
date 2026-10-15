/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef ALRIOS_ATOMIC_SWAP_H
#define ALRIOS_ATOMIC_SWAP_H

#include "alrios/platform/types.h"

int alrios_atomic_symlink_swap(const char *target_dir, const char *symlink_current, pid_t master_pid);

#endif /* ALRIOS_ATOMIC_SWAP_H */
