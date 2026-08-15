/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_INTERNAL_H
#define ARWN_INTERNAL_H

#include "arwn.h"
#include "arwn_config.h"

#define ARWN_APP_MAX_NAME 63

struct arwn_app {
    char name[ARWN_APP_MAX_NAME + 1];
    char root[1024];
    arwn_cfg_t cfg;
    arwn_unit_t units[ARWN_CFG_MAX_SECTIONS];
    int unit_count;
};

/* Preenche uma unit (source/entry/files/langs) a partir do config.
   Compartilhado entre core e builder. Retorna 0 ou -1 (erro no cfg). */
int arwn_unit_fill(arwn_unit_t *u, arwn_cfg_t *cfg);

#endif /* ARWN_INTERNAL_H */