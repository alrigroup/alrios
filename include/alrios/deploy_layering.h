/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#ifndef ALRIOS_DEPLOY_LAYERING_H
#define ALRIOS_DEPLOY_LAYERING_H

#include "alrios/arapp_index.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum alrios_layer_result {
    ALRIOS_LAYER_OK = 0,
    ALRIOS_LAYER_HARDLINKED = 1,
    ALRIOS_LAYER_REFLINKED  = 2,
    ALRIOS_LAYER_COPIED     = 3,
    ALRIOS_LAYER_ERR        = -1
} alrios_layer_result_t;

int alrios_deploy_provision_delta(const char *old_slot,
                                 const char *new_slot,
                                 const arapp_index_entry_t *entries,
                                 size_t count);

int alrios_deploy_verify_slot(const char *slot_path,
                             const arapp_index_entry_t *entries,
                             size_t count);

int alrios_deploy_rollback_slot(const char *new_slot);

#ifdef __cplusplus
}
#endif

#endif
