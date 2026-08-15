/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_CONFIG_H
#define ARWN_CONFIG_H

#include <stddef.h>
#include "arwn.h"

typedef struct {
    char section[ARWN_CFG_SECT_MAX + 1];
    char key[ARWN_CFG_KEY_MAX + 1];
    char value[ARWN_CFG_VAL_MAX + 1];
} arwn_cfg_entry_t;

typedef struct {
    arwn_cfg_entry_t entries[ARWN_CFG_MAX_KEYS];
    int count;
    char error[256];
} arwn_cfg_t;

/* Parse de um buffer config.arwn (formato proprietário estilo arws.cfg).
   Estrito: sem alocação, sem strtol, com limites rígidos. */
int arwn_cfg_parse(arwn_cfg_t *cfg, const char *buf, size_t len);

const char *arwn_cfg_find(const arwn_cfg_t *cfg, const char *section, const char *key);
const char *arwn_cfg_find_def(const arwn_cfg_t *cfg, const char *section,
                              const char *key, const char *def);
int arwn_cfg_find_int(const arwn_cfg_t *cfg, const char *section, const char *key,
                      int def);
int arwn_cfg_find_bool(const arwn_cfg_t *cfg, const char *section, const char *key,
                       int def);

#endif /* ARWN_CONFIG_H */