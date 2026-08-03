/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef MODES_H
#define MODES_H

#include "arws_config.h"

void modes_init(void);
const char* modes_get_effective(const char *host, const char *path);
int modes_set_global(const char *mode);
const char* modes_get_global(void);

#endif
