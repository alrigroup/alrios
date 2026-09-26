/* ====================================================================
 * Copyright (c) 2026 ALRI Development. All rights reserved.
 * ==================================================================== */
#include "alrios/vault.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main(void) {
    alrios_vault_t v;
    memset(&v, 0, sizeof(v));
    assert(alrios_vault_init(&v) == 0);
    assert(alrios_vault_set(&v, "DB_HOST", "127.0.0.1") == 0);
    assert(strcmp(alrios_vault_get(&v, "DB_HOST"), "127.0.0.1") == 0);

    char **envp = NULL;
    assert(alrios_vault_build_envp(&v, &envp) == 0);
    assert(envp != NULL);
    assert(strcmp(envp[0], "DB_HOST=127.0.0.1") == 0);
    alrios_vault_free_envp(envp, v.count);

    printf("TASK-006 (In-Memory Vault): PASS\n");
    return 0;
}
