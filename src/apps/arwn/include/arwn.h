/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWN_H
#define ARWN_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ARWN_VERSION "0.2.0"

/* Limites rígidos de segurança do parser/contêiner (anti-bomba). */
#define ARWN_CFG_MAX_FILE        (64 * 1024)
#define ARWN_CFG_MAX_KEYS        256
#define ARWN_CFG_MAX_SECTIONS    16
#define ARWN_CFG_KEY_MAX         63
#define ARWN_CFG_VAL_MAX         4095
#define ARWN_CFG_SECT_MAX        63

#define ARWN_UNIT_MAX_NAME       63
#define ARWN_UNIT_MAX_SOURCE     1023
#define ARWN_UNIT_MAX_ENTRY      255
#define ARWN_UNIT_MAX_FILES      16
#define ARWN_UNIT_MAX_LANGS      8

#define ARWN_ARWEB_MAGIC         "ALRIGROUP@ARWEB"
#define ARWN_ARWEB_VERSION       1
#define ARWN_ARWEB_MAX_SECTIONS  64
#define ARWN_ARWEB_NAME_MAX      31

typedef struct arwn_app arwn_app_t;

/* Unit lógica declarada em config.arwn ([unit:<nome>]). */
typedef struct {
    char name[ARWN_UNIT_MAX_NAME + 1];
    char source[ARWN_UNIT_MAX_SOURCE + 1];
    char entry[ARWN_UNIT_MAX_ENTRY + 1];
    char route[ARWN_UNIT_MAX_ENTRY + 1];
    char files[ARWN_UNIT_MAX_FILES][ARWN_UNIT_MAX_ENTRY + 1];
    int  file_count;
    char langs[ARWN_UNIT_MAX_LANGS][16];
    int  lang_count;
    int  obfuscate;
    char copyright[4096];
} arwn_unit_t;

/* App: detém config parseada + resultados do builder. */
arwn_app_t *arwn_app_new(const char *name);
void arwn_app_free(arwn_app_t *app);
const char *arwn_app_name(const arwn_app_t *app);
const char *arwn_app_root(const arwn_app_t *app);

/* config.arwn */
int arwn_config_load(arwn_app_t *app, const char *path);
int arwn_config_parse_buffer(arwn_app_t *app, const char *buf, size_t len);
const char *arwn_config_get(const arwn_app_t *app, const char *section, const char *key,
                            const char *def);
int arwn_config_get_int(const arwn_app_t *app, const char *section, const char *key,
                        int def);
int arwn_config_get_bool(const arwn_app_t *app, const char *section, const char *key,
                         int def);
int arwn_config_unit_count(const arwn_app_t *app);
const arwn_unit_t *arwn_config_unit(const arwn_app_t *app, int idx);
const char *arwn_config_last_error(const arwn_app_t *app);

/* Builder (Fase 1) */
int arwn_builder_execute(arwn_app_t *app);
int arwn_builder_execute_out(arwn_app_t *app, const char *out_dir);

/* Mount (Fase 2): build no start -> carrega .arweb em memória -> serve
   (event loop) + registra rotas no arws (gateway em thread separada).
   Bloqueia rodando o event loop do servidor até erro/parada. */
int arwn_mount(arwn_app_t *app);

#ifdef __cplusplus
}
#endif

#endif /* ARWN_H */