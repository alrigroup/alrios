/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "home_os.h"
#include <stddef.h>
#include <stdint.h>

/* ---------- forward declarations ---------- */

static int  os_get_exe_dir(char *buf, int size);
static int  os_connect_timeout(int fd, const char *addr, uint16_t port, int timeout_ms);
static int  os_set_recv_timeout(int fd, int timeout_ms);

/* ---------- public API ---------- */

int home_os_get_exe_dir(char *buf, int size) {
    if (!buf || size <= 0) return -1;
    return os_get_exe_dir(buf, size);
}

int home_os_connect_timeout(int fd, const char *addr, uint16_t port, int timeout_ms) {
    if (fd < 0 || !addr) return -1;
    return os_connect_timeout(fd, addr, port, timeout_ms);
}

int home_os_set_recv_timeout(int fd, int timeout_ms) {
    if (fd < 0) return -1;
    return os_set_recv_timeout(fd, timeout_ms);
}

/* ---------- OS dispatch ---------- */

#ifdef __linux__
    #include "linux/path.c"
    #include "linux/net.c"
#elif defined(_WIN32)
    #include "windows/path.c"
    #include "windows/net.c"
#else
    #error "alrios: no home.web OS implementation for this OS"
#endif
