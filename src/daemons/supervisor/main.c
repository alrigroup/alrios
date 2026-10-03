/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#include "alrios/profiles.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[]) {
    if (argc < 2) {
        (void)fprintf(stderr, "Usage: supervisor_policy <profile>\n");
        return 1;
    }
    alrios_profile_t prof;
    if (alrios_profile_parse(argv[1], &prof) != 0) {
        (void)fprintf(stderr, "Error: unknown profile %s\n", argv[1]);
        return 1;
    }
    alrios_node_policy_t node;
    if (alrios_profile_init(&node, prof) != 0) {
        (void)fprintf(stderr, "Error: failed to initialize profile\n");
        return 1;
    }
    (void)printf("Supervisor initialized with profile: %s\n", alrios_profile_name(prof));
    return 0;
}
