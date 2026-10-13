/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_PLATFORM_TYPES_H
#define ALRIOS_PLATFORM_TYPES_H

#ifdef _WIN32
typedef int pid_t;
typedef unsigned int uid_t;
typedef unsigned int gid_t;
#else
#include <sys/types.h>
#endif

#endif /* ALRIOS_PLATFORM_TYPES_H */
