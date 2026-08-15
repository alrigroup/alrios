/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include <unistd.h>
#include <string.h>

static int os_get_exe_dir(char *buf, int size) {
    ssize_t n = readlink("/proc/self/exe", buf, (size_t)size - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';
    char *p = strrchr(buf, '/');
    if (p) { *p = '\0'; return 0; }
    strcpy(buf, ".");
    return 0;
}
