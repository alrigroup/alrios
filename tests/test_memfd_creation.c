/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/memloader.h"
#include <assert.h>
#include <unistd.h>
#include <stdio.h>

int main(void) {
    const char *payload = "ELF_MOCK_PAYLOAD";
    int fd = alrios_memfd_create_sealed("test_fd", (const uint8_t *)payload, 16);
    assert(fd >= 0);
    close(fd);
    printf("TASK-004 (memfd creation): PASS\n");
    return 0;
}
