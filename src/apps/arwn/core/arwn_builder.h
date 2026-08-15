/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_BUILDER_H
#define ARWN_BUILDER_H

#include "arwn.h"

/* Executa o pipeline de build no start: lê as units do config, dispara as
   toolchains e monta um .arweb por unit. */
int arwn_builder_execute(arwn_app_t *app);

/* Junta dois segmentos de path com limite explícito. Retorna 0 ou -1. */
int arwn_path_join(char *out, int cap, const char *a, const char *b);

/* Monta "<a>/<b><suffix>" com bounds. Retorna 0 ou -1. */
int arwn_path_join_suffix(char *out, int cap, const char *a, const char *b,
                          const char *suffix);

#endif /* ARWN_BUILDER_H */