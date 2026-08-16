/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn_config.h"

#include <ctype.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* Helpers estritos (sem strtol/sscanf)                                */
/* ------------------------------------------------------------------ */

static int is_ws(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static const char *skip_ws(const char *p, const char *end) {
    while (p < end && is_ws(*p)) p++;
    return p;
}

static const char *trim_end(const char *p, const char *start) {
    while (p > start && is_ws(p[-1])) p--;
    return p;
}

/* Inteiro estrito: apenas dígitos, com guarda de overflow.
   Rejeita sinal/hex/octal (classe inteira de bugs de smuggling). */
static int parse_int(const char *s, size_t len, int def, int *ok) {
    if (len == 0) {
        if (ok) *ok = 0;
        return def;
    }
    long long v = 0;
    for (size_t i = 0; i < len; i++) {
        char c = s[i];
        if (c < '0' || c > '9') {
            if (ok) *ok = 0;
            return def;
        }
        if (v > (LLONG_MAX - (c - '0')) / 10) {
            if (ok) *ok = 0;
            return def;
        }
        v = v * 10 + (c - '0');
        if (v > INT_MAX) {
            if (ok) *ok = 0;
            return def;
        }
    }
    if (ok) *ok = 1;
    return (int)v;
}

static void set_error(arwn_cfg_t *cfg, const char *msg) {
    if (!cfg || !msg) return;
    size_t n = strlen(msg);
    if (n >= sizeof(cfg->error)) n = sizeof(cfg->error) - 1;
    memcpy(cfg->error, msg, n);
    cfg->error[n] = '\0';
}

static void set_error_fmt(arwn_cfg_t *cfg, const char *fmt, int line) {
    if (!cfg) return;
    snprintf(cfg->error, sizeof(cfg->error), fmt, line);
}

/* ------------------------------------------------------------------ */
/* Parse principal                                                      */
/* ------------------------------------------------------------------ */

int arwn_cfg_parse(arwn_cfg_t *cfg, const char *buf, size_t len) {
    if (!cfg || !buf) return -1;
    memset(cfg, 0, sizeof(*cfg));

    if (len > ARWN_CFG_MAX_FILE) {
        set_error(cfg, "config.arwn exceeds ARWN_CFG_MAX_FILE");
        return -1;
    }

    char section[ARWN_CFG_SECT_MAX + 1] = {0};
    const char *p = buf;
    const char *end = buf + len;
    int line_no = 0;

    while (p < end) {
        const char *eol = p;
        while (eol < end && *eol != '\n') eol++;
        line_no++;

        const char *l = skip_ws(p, eol);
        const char *le = trim_end(eol, l);

        if (l < le) {
            if (*l == '#') {
                /* comentário */
            } else if (*l == '[') {
                /* seção */
                const char *rs = l + 1;
                const char *re = rs;
                while (re < le && *re != ']') re++;
                if (re >= le || re == rs) {
                    set_error_fmt(cfg, "config line %d: invalid section", line_no);
                    return -1;
                }
                size_t slen = (size_t)(re - rs);
                if (slen > ARWN_CFG_SECT_MAX) {
                    set_error_fmt(cfg, "config line %d: section name too long", line_no);
                    return -1;
                }
                memcpy(section, rs, slen);
                section[slen] = '\0';
            } else {
                /* chave=valor */
                const char *eq = l;
                while (eq < le && *eq != '=') eq++;
                if (eq >= le) {
                    set_error_fmt(cfg, "config line %d: expected key=value", line_no);
                    return -1;
                }

                const char *ks = l;
                const char *ke = trim_end(eq, ks);
                const char *vs = skip_ws(eq + 1, eol);
                const char *ve = trim_end(eol, vs);

                /* Suporte a blocos literais multi-linhas delimitados por crase `...` ou aspas '...' / "..." */
                if (vs < end && (*vs == '`' || *vs == '\'' || *vs == '"')) {
                    char quote_char = *vs;
                    vs++; /* pula a aspas/crase inicial */
                    const char *closing_qt = vs;
                    while (closing_qt < end && *closing_qt != quote_char) {
                        if (*closing_qt == '\n') line_no++;
                        closing_qt++;
                    }
                    if (closing_qt >= end) {
                        set_error_fmt(cfg, "config line %d: unclosed multiline literal", line_no);
                        return -1;
                    }
                    ve = closing_qt;
                    eol = closing_qt;
                    while (eol < end && *eol != '\n') eol++;
                }

                if (ks >= ke) {
                    set_error_fmt(cfg, "config line %d: empty key", line_no);
                    return -1;
                }
                if ((size_t)(ke - ks) > ARWN_CFG_KEY_MAX) {
                    set_error_fmt(cfg, "config line %d: key too long", line_no);
                    return -1;
                }
                if ((size_t)(ve - vs) > ARWN_CFG_VAL_MAX) {
                    set_error_fmt(cfg, "config line %d: value too long", line_no);
                    return -1;
                }

                if (cfg->count >= ARWN_CFG_MAX_KEYS) {
                    set_error_fmt(cfg, "config line %d: too many keys", line_no);
                    return -1;
                }

                arwn_cfg_entry_t *e = &cfg->entries[cfg->count++];
                size_t slen = strlen(section);
                if (slen > 0) {
                    memcpy(e->section, section, slen);
                    e->section[slen] = '\0';
                }
                size_t klen = (size_t)(ke - ks);
                memcpy(e->key, ks, klen);
                e->key[klen] = '\0';
                size_t vlen = (size_t)(ve - vs);
                memcpy(e->value, vs, vlen);
                e->value[vlen] = '\0';
            }
        }

        p = eol + 1;
    }

    return 0;
}

/* ------------------------------------------------------------------ */
/* Lookup (varredura linear pequena, tabela estática)                  */
/* ------------------------------------------------------------------ */

const char *arwn_cfg_find(const arwn_cfg_t *cfg, const char *section, const char *key) {
    if (!cfg || !key) return NULL;
    for (int i = 0; i < cfg->count; i++) {
        const arwn_cfg_entry_t *e = &cfg->entries[i];
        int sect_ok = (section == NULL || section[0] == '\0' || e->section[0] == '\0')
                          ? (section == NULL || section[0] == '\0')
                          : (strcmp(e->section, section) == 0);
        if (!sect_ok) continue;
        if (strcmp(e->key, key) == 0) return e->value;
    }
    return NULL;
}

const char *arwn_cfg_find_def(const arwn_cfg_t *cfg, const char *section,
                              const char *key, const char *def) {
    const char *v = arwn_cfg_find(cfg, section, key);
    return v ? v : (def ? def : "");
}

int arwn_cfg_find_int(const arwn_cfg_t *cfg, const char *section, const char *key,
                      int def) {
    const char *v = arwn_cfg_find(cfg, section, key);
    if (!v) return def;
    int ok = 0;
    int r = parse_int(v, strlen(v), def, &ok);
    return ok ? r : def;
}

int arwn_cfg_find_bool(const arwn_cfg_t *cfg, const char *section, const char *key,
                       int def) {
    const char *v = arwn_cfg_find(cfg, section, key);
    if (!v) return def;
    if (strcmp(v, "yes") == 0 || strcmp(v, "true") == 0 || strcmp(v, "1") == 0) return 1;
    if (strcmp(v, "no") == 0 || strcmp(v, "false") == 0 || strcmp(v, "0") == 0) return 0;
    return def;
}