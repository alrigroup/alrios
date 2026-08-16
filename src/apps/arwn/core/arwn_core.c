/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#ifndef _WIN32
#include <unistd.h>
#endif
#include "arwn_config.h"
#include "arwn_builder.h"
#include "arwn_gateway.h"
#include "arwn_pack.h"
#include "arwn_server.h"
#include "arwn_internal.h"

#define ARWN_APP_MAX_NAME 63

static arwn_server_t *g_active_server = NULL;

static void on_sigterm(int sig) {
    (void)sig;
    if (g_active_server) {
        arwn_server_stop(g_active_server);
    }
    _exit(0);
}

static int file_exists(const char *path) {
    if (!path) return 0;
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

arwn_app_t *arwn_app_new(const char *name) {
    if (!name || name[0] == '\0') return NULL;
    arwn_app_t *app = (arwn_app_t *)malloc(sizeof(*app));
    if (!app) return NULL;
    memset(app, 0, sizeof(*app));

    size_t n = strlen(name);
    if (n > ARWN_APP_MAX_NAME) n = ARWN_APP_MAX_NAME;
    memcpy(app->name, name, n);
    app->name[n] = '\0';
    return app;
}

void arwn_app_free(arwn_app_t *app) {
    if (app) free(app);
}

const char *arwn_app_name(const arwn_app_t *app) {
    return app ? app->name : "";
}

const char *arwn_app_root(const arwn_app_t *app) {
    return app ? app->root : "";
}

void arwn_app_set_root(arwn_app_t *app, const char *root) {
    if (!app || !root) return;
    size_t n = strlen(root);
    if (n >= sizeof(app->root)) n = sizeof(app->root) - 1;
    memcpy(app->root, root, n);
    app->root[n] = '\0';
}

const char *arwn_config_last_error(const arwn_app_t *app) {
    return app ? app->cfg.error : "";
}

int arwn_unit_fill(arwn_unit_t *u, arwn_cfg_t *cfg) {
    if (!u || !cfg) return -1;

    char sect[sizeof("unit:") + ARWN_UNIT_MAX_NAME + 1];
    snprintf(sect, sizeof(sect), "unit:%s", u->name);

    const char *src = arwn_cfg_find_def(cfg, sect, "source", "");
    const char *entry = arwn_cfg_find_def(cfg, sect, "entry", "main.arhtml");
    const char *route = arwn_cfg_find_def(cfg, sect, "route", "");
    const char *compile = arwn_cfg_find_def(cfg, sect, "compile", "");
    const char *langs = arwn_cfg_find_def(cfg, sect, "compile.lang", "");
    const char *obf = arwn_cfg_find_def(cfg, sect, "obfuscate", "no");
    const char *cpy = arwn_cfg_find_def(cfg, sect, "copyright", "");
    if (!cpy || cpy[0] == '\0') {
        cpy = arwn_cfg_find_def(cfg, "app", "copyright", "");
    }

    if (src[0] == '\0') {
        snprintf(cfg->error, sizeof(cfg->error), "unit:%s missing 'source'", u->name);
        return -1;
    }

    size_t nl = strlen(src);
    if (nl > ARWN_UNIT_MAX_SOURCE) nl = ARWN_UNIT_MAX_SOURCE;
    memcpy(u->source, src, nl);
    u->source[nl] = '\0';

    nl = strlen(entry);
    if (nl > ARWN_UNIT_MAX_ENTRY) nl = ARWN_UNIT_MAX_ENTRY;
    memcpy(u->entry, entry, nl);
    u->entry[nl] = '\0';

    /* route: path limpo da página (sem extensão). Default: "/<nome>" */
    nl = strlen(route);
    if (nl > ARWN_UNIT_MAX_ENTRY) nl = ARWN_UNIT_MAX_ENTRY;
    memcpy(u->route, route, nl);
    u->route[nl] = '\0';
    if (u->route[0] == '\0') {
        snprintf(u->route, sizeof(u->route), "/%s", u->name);
    } else if (u->route[0] != '/') {
        /* garante "/" à frente (ex: route=regras -> /regras) */
        char tmp[ARWN_UNIT_MAX_ENTRY + 1];
        snprintf(tmp, sizeof(tmp), "/%s", u->route);
        snprintf(u->route, sizeof(u->route), "%s", tmp);
    }

    u->obfuscate = (strcmp(obf, "yes") == 0 || strcmp(obf, "true") == 0);

    if (cpy && cpy[0] != '\0') {
        size_t cl = strlen(cpy);
        if (cl > sizeof(u->copyright) - 1) cl = sizeof(u->copyright) - 1;
        memcpy(u->copyright, cpy, cl);
        u->copyright[cl] = '\0';
    } else {
        u->copyright[0] = '\0';
    }

    u->file_count = 0;
    const char *p = compile;
    while (p && *p && u->file_count < ARWN_UNIT_MAX_FILES) {
        while (*p == ' ' || *p == '\t') p++;
        const char *comma = strchr(p, ',');
        size_t fl = comma ? (size_t)(comma - p) : strlen(p);
        while (fl > 0 && (p[fl - 1] == ' ' || p[fl - 1] == '\t')) fl--;
        if (fl > 0 && fl <= ARWN_UNIT_MAX_ENTRY) {
            memcpy(u->files[u->file_count], p, fl);
            u->files[u->file_count][fl] = '\0';
            u->file_count++;
        }
        p = comma ? comma + 1 : NULL;
    }

    u->lang_count = 0;
    p = langs;
    while (p && *p && u->lang_count < ARWN_UNIT_MAX_LANGS) {
        while (*p == ' ' || *p == '\t') p++;
        const char *comma = strchr(p, ',');
        size_t fl = comma ? (size_t)(comma - p) : strlen(p);
        while (fl > 0 && (p[fl - 1] == ' ' || p[fl - 1] == '\t')) fl--;
        if (fl > 0 && fl < 16) {
            memcpy(u->langs[u->lang_count], p, fl);
            u->langs[u->lang_count][fl] = '\0';
            u->lang_count++;
        }
        p = comma ? comma + 1 : NULL;
    }

    if (u->lang_count == 0) {
        snprintf(cfg->error, sizeof(cfg->error), "unit:%s missing 'compile.lang'", u->name);
        return -1;
    }

    return 0;
}

int arwn_config_parse_buffer(arwn_app_t *app, const char *buf, size_t len) {
    if (!app) return -1;
    int rc = arwn_cfg_parse(&app->cfg, buf, len);
    if (rc != 0) return rc;

    /* deriva as units [unit:<nome>] e preenche cada uma */
    int unit_count = 0;
    for (int i = 0; i < app->cfg.count && unit_count < ARWN_CFG_MAX_SECTIONS; i++) {
        const arwn_cfg_entry_t *e = &app->cfg.entries[i];
        if (strncmp(e->section, "unit:", 5) == 0 && e->section[5] != '\0') {
            /* só entra na primeira chave de cada seção */
            int seen = 0;
            for (int j = 0; j < unit_count; j++) {
                if (strcmp(app->units[j].name, e->section + 5) == 0) { seen = 1; break; }
            }
            if (!seen) {
                arwn_unit_t *u = &app->units[unit_count++];
                memset(u, 0, sizeof(*u));
                size_t nl = strlen(e->section + 5);
                if (nl > ARWN_UNIT_MAX_NAME) nl = ARWN_UNIT_MAX_NAME;
                memcpy(u->name, e->section + 5, nl);
                u->name[nl] = '\0';
            }
        }
    }
    app->unit_count = unit_count;

    /* preenche source/entry/files/langs de cada unit */
    for (int i = 0; i < app->unit_count; i++) {
        if (arwn_unit_fill(&app->units[i], &app->cfg) != 0) return -1;
    }
    return 0;
}

int arwn_config_load(arwn_app_t *app, const char *path) {
    if (!app || !path) return -1;

    FILE *f = fopen(path, "rb");
    if (!f) {
        snprintf(app->cfg.error, sizeof(app->cfg.error),
                 "cannot open config: %s", path);
        return -1;
    }
    char buf[ARWN_CFG_MAX_FILE];
    size_t got = fread(buf, 1, sizeof(buf), f);
    int closed = fclose(f);
    if (closed != 0) return -1;
    if (got >= sizeof(buf)) {
        snprintf(app->cfg.error, sizeof(app->cfg.error),
                 "config too large (max %d bytes)", ARWN_CFG_MAX_FILE);
        return -1;
    }

    /* root = diretório do config (para resolver web/ e assets/ relativos) */
    const char *slash = strrchr(path, '/');
    const char *bslash = strrchr(path, '\\');
    const char *last = slash && bslash ? (slash > bslash ? slash : bslash)
                     : (slash ? slash : bslash);
    if (last) {
        size_t rlen = (size_t)(last - path);
        if (rlen > 0 && rlen < sizeof(app->root)) {
            memcpy(app->root, path, rlen);
            app->root[rlen] = '\0';
        } else if (rlen == 0) {
            strncpy(app->root, "/", sizeof(app->root) - 1);
        }
    } else {
        strncpy(app->root, ".", sizeof(app->root) - 1);
    }

    int rc = arwn_config_parse_buffer(app, buf, got);
    if (rc == 0) {
        const char *app_name = arwn_config_get(app, "app", "name", "");
        if (app_name && app_name[0] != '\0') {
            size_t n = strlen(app_name);
            if (n > ARWN_APP_MAX_NAME) n = ARWN_APP_MAX_NAME;
            memcpy(app->name, app_name, n);
            app->name[n] = '\0';
        }
    }
    return rc;
}

const char *arwn_config_get(const arwn_app_t *app, const char *section, const char *key,
                            const char *def) {
    if (!app) return def ? def : "";
    return arwn_cfg_find_def(&app->cfg, section, key, def);
}

int arwn_config_get_int(const arwn_app_t *app, const char *section, const char *key,
                        int def) {
    if (!app) return def;
    return arwn_cfg_find_int(&app->cfg, section, key, def);
}

int arwn_config_get_bool(const arwn_app_t *app, const char *section, const char *key,
                         int def) {
    if (!app) return def;
    return arwn_cfg_find_bool(&app->cfg, section, key, def);
}

int arwn_config_unit_count(const arwn_app_t *app) {
    return app ? app->unit_count : 0;
}

const arwn_unit_t *arwn_config_unit(const arwn_app_t *app, int idx) {
    if (!app || idx < 0 || idx >= app->unit_count) return NULL;
    return &app->units[idx];
}

int arwn_mount(arwn_app_t *app) {
    if (!app) return -1;

    char build_dir[1300];
    snprintf(build_dir, sizeof(build_dir), "%s/%s", arwn_app_root(app), "build");

    /* Verifica se todos os .arweb já estão presentes */
    int all_exist = 1;
    for (int i = 0; i < arwn_config_unit_count(app); i++) {
        const arwn_unit_t *u = arwn_config_unit(app, i);
        char p1[1300], p2[1300];
        arwn_path_join_suffix(p1, sizeof(p1), build_dir, u->name, ".arweb");
        arwn_path_join_suffix(p2, sizeof(p2), arwn_app_root(app), u->name, ".arweb");
        if (!file_exists(p1) && !file_exists(p2)) {
            all_exist = 0;
            break;
        }
    }

    /* 1. build no start somente se faltar algum .arweb */
    if (!all_exist) {
        if (arwn_builder_execute(app) != 0) {
            fprintf(stderr, "[arwn] mount: build failed\n");
            return -1;
        }
    }

    /* 2. carrega cada unit .arweb em memória (validado) e monta o server */
    arwn_server_t *server = arwn_server_create();
    if (!server) return -1;

    int loaded = 0;
    for (int i = 0; i < arwn_config_unit_count(app); i++) {
        const arwn_unit_t *u = arwn_config_unit(app, i);
        char path[1300];
        if (arwn_path_join_suffix(path, (int)sizeof(path), build_dir, u->name,
                                  ".arweb") != 0) {
            fprintf(stderr, "[arwn] mount: path too long (%s.arweb)\n", u->name);
            continue;
        }

        FILE *f = fopen(path, "rb");
        if (!f) {
            /* Fallback: procura direto na raiz do app (caso descompactado sem subpasta build/) */
            char root_path[1300];
            if (arwn_path_join_suffix(root_path, (int)sizeof(root_path), arwn_app_root(app), u->name, ".arweb") == 0) {
                f = fopen(root_path, "rb");
            }
        }
        if (!f) continue;
        if (fseek(f, 0, SEEK_END) != 0) { fclose(f); continue; }
        long sz = ftell(f);
        if (sz <= 0) { fclose(f); continue; }
        fseek(f, 0, SEEK_SET);
        unsigned char *data = (unsigned char *)malloc((size_t)sz);
        if (!data) { fclose(f); continue; }
        size_t got = fread(data, 1, (size_t)sz, f);
        fclose(f);

        if (arwn_pack_validate(data, got) == 0) {
            arwn_server_load_arweb(server, u, data, got);
            printf("[arwn] mounted %s.arweb (%zu bytes)\n", u->name, got);
            loaded++;
        } else {
            fprintf(stderr, "[arwn] mount: %s.arweb invalid (CRC/format)\n",
                    u->name);
            free(data);
        }
    }
    if (loaded == 0) {
        fprintf(stderr, "[arwn] mount: no .arweb loaded\n");
        arwn_server_free(server);
        return -1;
    }

    /* 3. serve + registra rotas no arws (gateway em thread separada) */
    const char *bind = arwn_config_get(app, "app", "bind", "127.0.0.1");
    int port = arwn_config_get_int(app, "app", "port", 3001);

    g_active_server = server;
#ifndef _WIN32
    signal(SIGTERM, on_sigterm);
    signal(SIGINT, on_sigterm);
#endif

    arwn_gateway_start(app, server);

    int rc = arwn_server_run(server, bind, (uint16_t)port);
    g_active_server = NULL;
    arwn_server_free(server);
    return rc;
}