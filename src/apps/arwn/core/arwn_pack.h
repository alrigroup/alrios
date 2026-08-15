/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_PACK_H
#define ARWN_PACK_H

#include <stddef.h>
#include <stdint.h>
#include "arwn.h"

#define ARWN_PACK_MAX_PAYLOAD (256 * 1024 * 1024)

typedef struct {
    char name[ARWN_ARWEB_NAME_MAX + 1];
    const void *data;
    uint32_t size;
} arwn_pack_section_t;

/* Constrói um .arweb em memória (binário, seções + CRC32 por seção).
   Retorna tamanho total ou <0 em erro. O buffer deve ter
   ARWN_PACK_MAX_PAYLOAD cap implicitamente validado. */
int arwn_pack_build(const arwn_pack_section_t *sections, int count,
                    uint8_t *out, size_t out_cap, size_t *out_len);

/* Valida um .arweb em memória: magic, versão, tabela de seções com
   offsets clampados ao tamanho real e CRC32 por seção. */
int arwn_pack_validate(const uint8_t *data, size_t len);

/* Indexa um .arweb validado em memória (zero-copy): preenche `views`
   com ponteiros diretos para cada seção. Retorna o nº de seções, ou <0
   se inválido. `views_cap` é o tamanho de `views`. */
int arwn_pack_index(const uint8_t *data, size_t len,
                    arwn_pack_section_t *views, int views_cap);

uint32_t arwn_crc32(const void *data, size_t len);

#endif /* ARWN_PACK_H */