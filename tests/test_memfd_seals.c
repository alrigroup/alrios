/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "alrios/memloader.h"
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

int main(void) {
    int fd = alrios_memfd_create_sealed("test_sealed", (const uint8_t *)"DATA", 4);
    assert(fd >= 0);
    int seals = fcntl(fd, F_GET_SEALS);
    assert(seals & F_SEAL_WRITE);
    assert(seals & F_SEAL_SEAL);
    close(fd);
    printf("TASK-005 (memfd seals): PASS\n");
    return 0;
}
