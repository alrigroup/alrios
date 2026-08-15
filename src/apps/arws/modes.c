/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "modes.h"
#include "log.h"
#include <string.h>
#include <stddef.h>

void modes_init(void) {
    alri_print(CYN "[MODES]" RST " (compat) Redirecting to arws_config\n");
}

const char* modes_get_effective(const char *host, const char *path) {
    return arws_config_get_effective_mode(host, path, NULL);
}

int modes_set_global(const char *mode) {
    return arws_config_set_global(mode);
}

const char* modes_get_global(void) {
    return arws_config_get_global_mode();
}
