/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "alrios/memloader.h"
#include <sys/mman.h>
#include <sys/syscall.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

int alrios_memfd_create_sealed(const char *name, const uint8_t *payload, size_t size) {
    if (!name || (!payload && size > 0)) {
        return ALRIOS_MEMLOADER_ERR_INVALID_PARAM;
    }

    int fd = (int)syscall(SYS_memfd_create, name, MFD_CLOEXEC | MFD_ALLOW_SEALING);
    if (fd < 0) {
        return ALRIOS_MEMLOADER_ERR_MEMFD_FAIL;
    }

    if (size > 0) {
        size_t total = 0;
        while (total < size) {
            ssize_t w = write(fd, payload + total, size - total);
            if (w < 0) {
                if (errno == EINTR) {
                    continue;
                }
                close(fd);
                return ALRIOS_MEMLOADER_ERR_IO_FAIL;
            }
            if (w == 0) {
                close(fd);
                return ALRIOS_MEMLOADER_ERR_IO_FAIL;
            }
            total += (size_t)w;
        }
    }

    if (fcntl(fd, F_ADD_SEALS, F_SEAL_SEAL | F_SEAL_SHRINK | F_SEAL_GROW | F_SEAL_WRITE) < 0) {
        close(fd);
        return ALRIOS_MEMLOADER_ERR_SEALING_FAIL;
    }

    return fd;
}
