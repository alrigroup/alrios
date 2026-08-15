/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arwn_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arwn_config.h"
#include "arwn_pack.h"
#include "arwn_internal.h"
#include "arwn_bridge_embed.h"
#include "arwn_obfuscator.h"
#include "aros_hal.h"

#define ARWN_BUILD_DIR "build"

/* ------------------------------------------------------------------ */
/* Helpers de arquivo/processo (via HAL arkernel)                      */
/* ------------------------------------------------------------------ */

static int file_exists(const char *path) {
    return ar_file_exists(path) == 1;
}

/* Procura `name` no PATH. Retorna 1 se encontrado. */
static int tool_find(const char *name) {
    const char *path = getenv("PATH");
    if (!path) return 0;
    char buf[4096];
    size_t n = strlen(path);
    if (n >= sizeof(buf)) return 0;
    memcpy(buf, path, n);
    buf[n] = '\0';

    char *save = NULL;
    char *tok = strtok_r(buf, ":", &save);
    while (tok) {
        char full[1200];
        if (snprintf(full, sizeof(full), "%s/%s", tok, name) < (int)sizeof(full)) {
            if (file_exists(full)) return 1;
        }
        tok = strtok_r(NULL, ":", &save);
    }
    return 0;
}

static int mkdir_p(const char *path) {
    if (ar_fs_exists(path) == 1) return 0;
    return ar_fs_mkdir(path) == 0 ? 0 : -1;
}

/* Injeta <script src="/arwn-bridge.js"> antes de </head> se ausente.
   Devolve novo buffer malloc'd e *out_len. NULL em erro. */
static char *inject_bridge_tag(const char *html, size_t hlen, size_t *out_len) {
    const char *tag = "<script src=\"/arwn-bridge.js\"></script>\n";
    size_t tlen = strlen(tag);
    if (strstr(html, "arwn-bridge.js")) {
        /* já injetada (idempotente) */
        char *copy = (char *)malloc(hlen + 1);
        if (!copy) return NULL;
        memcpy(copy, html, hlen);
        copy[hlen] = '\0';
        *out_len = hlen;
        return copy;
    }

    const char *head_end = strstr(html, "</head>");
    if (!head_end) head_end = strstr(html, "</HEAD>");
    size_t pos = head_end ? (size_t)(head_end - html) : hlen;

    char *out = (char *)malloc(hlen + tlen + 1);
    if (!out) return NULL;
    memcpy(out, html, pos);
    memcpy(out + pos, tag, tlen);
    memcpy(out + pos + tlen, html + pos, hlen - pos);
    out[hlen + tlen] = '\0';
    *out_len = hlen + tlen;
    return out;
}

static char *read_file_alloc(const char *path, size_t *out_len) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long sz = ftell(f);
    if (sz < 0 || sz > ARWN_PACK_MAX_PAYLOAD) { fclose(f); return NULL; }
    fseek(f, 0, SEEK_SET);
    char *data = (char *)malloc((size_t)sz + 1);
    if (!data) { fclose(f); return NULL; }
    size_t got = fread(data, 1, (size_t)sz, f);
    fclose(f);
    data[got] = '\0';
    if (out_len) *out_len = got;
    return data;
}

static int write_file(const char *path, const void *data, size_t len) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    size_t n = fwrite(data, 1, len, f);
    fclose(f);
    return n == len ? 0 : -1;
}

/* Executa toolchain com argv (NULL-terminado). Timeout em ms. */
static int run_tool(char *const argv[], int timeout_ms, char *err, int err_cap) {
    if (!argv || !argv[0]) return -1;
    int pid = ar_process_create(argv[0], argv);
    if (pid < 0) {
        snprintf(err, (size_t)err_cap, "cannot spawn %s (%d)", argv[0], pid);
        return -1;
    }
    (void)timeout_ms;
    int code = 0;
    while (ar_process_wait_status(pid, &code) == 0) {
        ar_sleep_ms(50);
    }
    if (code == 127) {
        /* execvp falhou: comando não encontrado no PATH */
        snprintf(err, (size_t)err_cap, "toolchain '%s' not found in PATH. "
                 "Install it and try again.", argv[0]);
        return -1;
    }
    if (code != 0) {
        snprintf(err, (size_t)err_cap, "%s exited with status %d", argv[0], code);
        return -1;
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Resolução de paths                                                  */
/* ------------------------------------------------------------------ */

static void resolve_path(const char *approot, const char *rel, char *out, int cap) {
    if (rel[0] == '/' || rel[0] == '\\') {
        snprintf(out, (size_t)cap, "%s", rel);
    } else if (approot[0] == '\0') {
        snprintf(out, (size_t)cap, "%s", rel);
    } else {
        snprintf(out, (size_t)cap, "%s/%s", approot, rel);
    }
}

/* Junta dois segmentos de path com limite explícito. Retorna 0 ou -1. */
int arwn_path_join(char *out, int cap, const char *a, const char *b) {
    if (!out || cap <= 0 || !a || !b) return -1;
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    if (alen + 1 + blen + 1 > (size_t)cap) return -1;
    memcpy(out, a, alen);
    out[alen] = '/';
    memcpy(out + alen + 1, b, blen + 1);
    return 0;
}

/* Monta "<a>/<b><suffix>" com bounds. Retorna 0 ou -1. */
int arwn_path_join_suffix(char *out, int cap, const char *a, const char *b,
                          const char *suffix) {
    if (!out || cap <= 0 || !a || !b || !suffix) return -1;
    size_t alen = strlen(a);
    size_t blen = strlen(b);
    size_t slen = strlen(suffix);
    if (alen + 1 + blen + slen + 1 > (size_t)cap) return -1;
    memcpy(out, a, alen);
    out[alen] = '/';
    memcpy(out + alen + 1, b, blen);
    memcpy(out + alen + 1 + blen, suffix, slen + 1);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Pipelines por linguagem (Fase 1)                                    */
/* ------------------------------------------------------------------ */

/* JS/TS → esbuild (bundle + minify) */
static int build_js_ts(arwn_unit_t *u, const char *approot, const char *tmpdir,
                       char *err, int err_cap) {
    char src_path[1300];
    resolve_path(approot, u->source, src_path, sizeof(src_path));
    char entry[1300];
    if (arwn_path_join(entry, sizeof(entry), src_path, u->files[0]) != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
        return -1;
    }

    if (!file_exists(entry)) {
        snprintf(err, (size_t)err_cap, "entry not found for unit %s", u->name);
        return -1;
    }

    char out[1300];
    if (arwn_path_join_suffix(out, sizeof(out), tmpdir, u->name, ".bundle.js") != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
        return -1;
    }

    if (!tool_find("esbuild")) {
        /* Fallback sem esbuild: copia direta do arquivo de entrada */
        size_t flen = 0;
        char *content = read_file_alloc(entry, &flen);
        if (!content) {
            snprintf(err, (size_t)err_cap, "cannot read entry for unit %s", u->name);
            return -1;
        }
        int wrc = write_file(out, content, flen);
        free(content);
        if (wrc != 0) {
            snprintf(err, (size_t)err_cap, "cannot copy bundle for unit %s", u->name);
            return -1;
        }
        return 0;
    }

    char *argv[] = {
        (char *)"esbuild",
        (char *)entry,
        (char *)"--bundle",
        (char *)"--minify",
        (char *)"--outfile",
        (char *)out,
        NULL
    };
    return run_tool(argv, 60000, err, err_cap);
}

/* C/C++ → emcc/em++ (uma entrada por execução; várias langs agrupadas) */
static int build_c_cpp(arwn_unit_t *u, const char *approot, const char *tmpdir,
                       char *err, int err_cap) {
    char out[1300];
    if (arwn_path_join_suffix(out, sizeof(out), tmpdir, u->name, ".wasm") != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
        return -1;
    }

    char src_path[1300];
    resolve_path(approot, u->source, src_path, sizeof(src_path));

    /* Se arquivo .wasm direto fornecido, copia diretamente */
    for (int i = 0; i < u->file_count; i++) {
        char p[1300];
        if (arwn_path_join(p, sizeof(p), src_path, u->files[i]) == 0 && file_exists(p)) {
            size_t plen = strlen(p);
            if (plen > 5 && strcmp(p + plen - 5, ".wasm") == 0) {
                size_t flen = 0;
                char *content = read_file_alloc(p, &flen);
                if (content) {
                    int wrc = write_file(out, content, flen);
                    free(content);
                    if (wrc == 0) return 0;
                }
            }
        }
    }

    const char *toolname = "emcc";
    for (int i = 0; i < u->lang_count; i++) {
        if (strcmp(u->langs[i], "cpp") == 0) { toolname = "em++"; break; }
    }
    if (!tool_find(toolname)) {
        snprintf(err, (size_t)err_cap,
                 "toolchain '%s' not found in PATH. Install Emscripten: "
                 "https://emscripten.org/docs/getting_started/downloads.html", toolname);
        return -1;
    }

    char *argv[48];
    int ai = 0;
    argv[ai++] = (char *)toolname;
    argv[ai++] = (char *)"-O3";
    argv[ai++] = (char *)"-s";
    argv[ai++] = (char *)"WASM=1";
    argv[ai++] = (char *)"-s";
    argv[ai++] = (char *)"EXPORTED_FUNCTIONS=_arwn_main,_malloc,_free";
    for (int i = 0; i < u->file_count && ai < 44; i++) {
        char p[1300];
        if (arwn_path_join(p, sizeof(p), src_path, u->files[i]) == 0 && file_exists(p))
            argv[ai++] = strdup(p);
    }
    if (ai <= 6) {
        snprintf(err, (size_t)err_cap, "unit %s: no C sources found", u->name);
        return -1;
    }
    argv[ai++] = (char *)"-o";
    argv[ai++] = (char *)out;
    argv[ai] = NULL;

    int rc = run_tool(argv, 120000, err, err_cap);
    for (int i = 6; i < ai - 2; i++) free(argv[i]);
    return rc;
}

/* Go → GOOS=js GOARCH=wasm (go -C <dir> build -o out.wasm .) */
static int build_go(arwn_unit_t *u, const char *approot, const char *tmpdir,
                    char *err, int err_cap) {
    char out[1300];
    if (arwn_path_join_suffix(out, sizeof(out), tmpdir, u->name, ".wasm") != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
        return -1;
    }

    char src_dir[1300];
    resolve_path(approot, u->source, src_dir, sizeof(src_dir));

    /* Se arquivo .wasm direto fornecido, copia diretamente */
    for (int i = 0; i < u->file_count; i++) {
        char p[1300];
        if (arwn_path_join(p, sizeof(p), src_dir, u->files[i]) == 0 && file_exists(p)) {
            size_t plen = strlen(p);
            if (plen > 5 && strcmp(p + plen - 5, ".wasm") == 0) {
                size_t flen = 0;
                char *content = read_file_alloc(p, &flen);
                if (content) {
                    int wrc = write_file(out, content, flen);
                    free(content);
                    if (wrc == 0) return 0;
                }
            }
        }
    }

    if (!tool_find("go")) {
        snprintf(err, (size_t)err_cap,
                 "toolchain 'go' not found in PATH. Install: https://go.dev/dl/");
        return -1;
    }
    if (!ar_fs_exists(src_dir)) {
        snprintf(err, (size_t)err_cap, "unit %s: source dir not found", u->name);
        return -1;
    }

    /* GOOS/GOARCH via wrapper `env` (execvp herda o env do processo) */
    char *argv[] = {
        (char *)"/usr/bin/env",
        (char *)"GOOS=js",
        (char *)"GOARCH=wasm",
        (char *)"go",
        (char *)"-C",
        (char *)src_dir,
        (char *)"build",
        (char *)"-o",
        (char *)out,
        (char *)".",
        NULL
    };
    return run_tool(argv, 120000, err, err_cap);
}

/* Vite (React/Vue/Angular) → vite build */
static int build_vite(arwn_unit_t *u, const char *approot, const char *tmpdir,
                      char *err, int err_cap) {
    (void)tmpdir;
    if (!tool_find("vite") && !tool_find("npx")) {
        snprintf(err, (size_t)err_cap,
                 "toolchain 'vite' not found in PATH. Add it to the app web/ project.");
        return -1;
    }
    char src_dir[1300];
    resolve_path(approot, u->source, src_dir, sizeof(src_dir));
    if (!ar_fs_exists(src_dir)) {
        snprintf(err, (size_t)err_cap, "unit %s: vite dir not found", u->name);
        return -1;
    }

    char *argv[] = { (char *)"vite", (char *)"build", NULL };
    return run_tool(argv, 120000, err, err_cap);
}

/* Rust → wasm-pack */
static int build_rust(arwn_unit_t *u, const char *approot, const char *tmpdir,
                      char *err, int err_cap) {
    char out[1300];
    if (arwn_path_join_suffix(out, sizeof(out), tmpdir, u->name, ".wasm") != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
        return -1;
    }

    char src_dir[1300];
    resolve_path(approot, u->source, src_dir, sizeof(src_dir));

    /* Se arquivo .wasm direto fornecido, copia diretamente */
    for (int i = 0; i < u->file_count; i++) {
        char p[1300];
        if (arwn_path_join(p, sizeof(p), src_dir, u->files[i]) == 0 && file_exists(p)) {
            size_t plen = strlen(p);
            if (plen > 5 && strcmp(p + plen - 5, ".wasm") == 0) {
                size_t flen = 0;
                char *content = read_file_alloc(p, &flen);
                if (content) {
                    int wrc = write_file(out, content, flen);
                    free(content);
                    if (wrc == 0) return 0;
                }
            }
        }
    }

    if (!tool_find("wasm-pack")) {
        snprintf(err, (size_t)err_cap,
                 "toolchain 'wasm-pack' not found in PATH. Install: cargo install wasm-pack");
        return -1;
    }
    char *argv[] = { (char *)"wasm-pack", (char *)"build", (char *)"--target", (char *)"web", NULL };
    return run_tool(argv, 120000, err, err_cap);
}

/* ------------------------------------------------------------------ */
/* Montagem do .arweb                                                  */
/* ------------------------------------------------------------------ */

static int assemble_arweb(arwn_unit_t *u, const char *approot,
                          const char *build_dir, char *err, int err_cap) {
    arwn_pack_section_t sections[ARWN_ARWEB_MAX_SECTIONS];
    int sc = 0;

    char tmpdir[1300];
    if (arwn_path_join(tmpdir, sizeof(tmpdir), build_dir, u->name) != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
        return -1;
    }
    mkdir_p(tmpdir);

#define ARWN_ADD_SECTION(_name, _data, _size)                                  \
    do {                                                                       \
        if ((_data) && sc < ARWN_ARWEB_MAX_SECTIONS) {                         \
            snprintf(sections[sc].name, sizeof(sections[sc].name), "%s", _name); \
            sections[sc].data = (_data);                                       \
            sections[sc].size = (uint32_t)(_size);                             \
            sc++;                                                              \
        }                                                                      \
    } while (0)

    /* config.arwn embutido apenas em dev (se não ofuscado) */
    if (!u->obfuscate) {
        char cfg_path[1300];
        resolve_path(approot, "config.arwn", cfg_path, sizeof(cfg_path));
        size_t clen = 0;
        char *cfg = read_file_alloc(cfg_path, &clen);
        ARWN_ADD_SECTION("config.arwn", cfg, clen);
    }

    /* app.html: o .arhtml principal da unit (+ bridge injetada, F3) */
    if (u->entry[0] != '\0') {
        char html_dir[1300];
        resolve_path(approot, u->source, html_dir, sizeof(html_dir));
        char html_path[1300];
        if (arwn_path_join(html_path, sizeof(html_path), html_dir, u->entry) == 0 && file_exists(html_path)) {
            size_t hlen = 0;
            char *html = read_file_alloc(html_path, &hlen);
            if (html) {
                size_t hlen2 = 0;
                char *html2 = inject_bridge_tag(html, hlen, &hlen2);
                free(html);
                if (html2) {
                    html = html2;
                    hlen = hlen2;
                    ARWN_ADD_SECTION("app.html", html, hlen);
                }
            }
        }

        /* main.css / style.css: folha de estilo externa modular */
        char css_path[1300];
        if ((arwn_path_join(css_path, sizeof(css_path), html_dir, "main.css") == 0 && file_exists(css_path)) ||
            (arwn_path_join(css_path, sizeof(css_path), html_dir, "style.css") == 0 && file_exists(css_path))) {
            size_t clen = 0;
            char *css = read_file_alloc(css_path, &clen);
            if (css) {
                ARWN_ADD_SECTION("main.css", css, clen);
            }
        }
    }

    /* arwn-bridge.js (F3): embutido no .arweb para servir com hash estável */
    {
        size_t blen = arwn_bridge_js_len;
        char *bridge = (char *)malloc(blen ? blen + 1 : 1);
        if (!bridge) return -1;
        memcpy(bridge, arwn_bridge_js, blen);
        bridge[blen] = '\0';
        if (u->obfuscate) {
            size_t obf_len = 0;
            char *obf_bridge = arwn_obfuscate_js(bridge, blen, u->copyright, &obf_len);
            if (obf_bridge) {
                free(bridge);
                bridge = obf_bridge;
                blen = obf_len;
            }
        } else {
            char hdr[4096];
            size_t hlen = arwn_format_copyright(hdr, sizeof(hdr), u->copyright);
            char *combined = (char *)malloc(hlen + blen + 1);
            if (combined) {
                memcpy(combined, hdr, hlen);
                memcpy(combined + hlen, bridge, blen);
                combined[hlen + blen] = '\0';
                free(bridge);
                bridge = combined;
                blen = hlen + blen;
            }
        }
        ARWN_ADD_SECTION("arwn-bridge.js", bridge, blen);
    }

    /* mod - o artefato wasm compilado */
    {
        char wasm_path[1300];
        if (arwn_path_join_suffix(wasm_path, sizeof(wasm_path), build_dir, u->name, ".wasm") != 0) {
            snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
            return -1;
        }
        size_t wlen = 0;
        char *wasm = read_file_alloc(wasm_path, &wlen);
        if (wasm && u->obfuscate) {
            size_t new_wlen = wlen;
            if (arwn_obfuscate_wasm((uint8_t *)wasm, wlen, &new_wlen) == 0) {
                wlen = new_wlen;
            }
        }
        ARWN_ADD_SECTION("mod/main.wasm", wasm, wlen);
    }

    /* bundle.js (esbuild/vite output) */
    {
        char js_path[1300];
        if (arwn_path_join_suffix(js_path, sizeof(js_path), build_dir, u->name, ".bundle.js") != 0) {
            snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
            return -1;
        }
        size_t jlen = 0;
        char *js = read_file_alloc(js_path, &jlen);
        if (js) {
            if (u->obfuscate) {
                size_t obf_len = 0;
                char *obf_js = arwn_obfuscate_js(js, jlen, u->copyright, &obf_len);
                if (obf_js) {
                    free(js);
                    js = obf_js;
                    jlen = obf_len;
                }
            } else {
                char hdr[4096];
                size_t hlen = arwn_format_copyright(hdr, sizeof(hdr), u->copyright);
                char *combined = (char *)malloc(hlen + jlen + 1);
                if (combined) {
                    memcpy(combined, hdr, hlen);
                    memcpy(combined + hlen, js, jlen);
                    combined[hlen + jlen] = '\0';
                    free(js);
                    js = combined;
                    jlen = hlen + jlen;
                }
            }
        }
        ARWN_ADD_SECTION("bundle.js", js, jlen);
    }

#undef ARWN_ADD_SECTION

    if (sc < 2) {
        snprintf(err, (size_t)err_cap, "unit %s: no artifacts to pack", u->name);
        return -1;
    }

    /* serializa em memória */
    uint8_t *packed = (uint8_t *)malloc(ARWN_PACK_MAX_PAYLOAD);
    if (!packed) return -1;
    size_t plen = 0;
    int rc = arwn_pack_build(sections, sc, packed, ARWN_PACK_MAX_PAYLOAD, &plen);
    if (rc != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: arwn_pack_build failed", u->name);
        free(packed);
        for (int i = 0; i < sc; i++) free((void *)sections[i].data);
        return -1;
    }

    /* valida (auto-teste) */
    if (arwn_pack_validate(packed, plen) != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: pack self-check failed", u->name);
        free(packed);
        for (int i = 0; i < sc; i++) free((void *)sections[i].data);
        return -1;
    }

    /* grava <build>/<unit>.arweb */
    char out_path[1300];
    if (arwn_path_join_suffix(out_path, sizeof(out_path), build_dir, u->name, ".arweb") != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: path too long", u->name);
        free(packed);
        for (int i = 0; i < sc; i++) free((void *)sections[i].data);
        return -1;
    }
    rc = write_file(out_path, packed, plen);
    if (rc != 0) {
        snprintf(err, (size_t)err_cap, "unit %s: cannot write .arweb", u->name);
    } else {
        printf("[arwn] packed %s (%zu bytes, %d sections, CRC ok)\n",
               out_path, plen, sc);
    }

    free(packed);
    for (int i = 0; i < sc; i++) free((void *)sections[i].data);
    return rc;
}

/* ------------------------------------------------------------------ */
/* Execução principal                                                   */
/* ------------------------------------------------------------------ */

int arwn_builder_execute(arwn_app_t *app) {
    if (!app) return -1;

    char build_dir[1300];
    resolve_path(app->root, ARWN_BUILD_DIR, build_dir, sizeof(build_dir));
    mkdir_p(build_dir);

    int failures = 0;
    for (int i = 0; i < app->unit_count; i++) {
        arwn_unit_t *u = &app->units[i];
        char err[256];
        int compiled = 0;

        for (int l = 0; l < u->lang_count; l++) {
            int rc;
            if (strcmp(u->langs[l], "c") == 0 || strcmp(u->langs[l], "cpp") == 0)
                rc = build_c_cpp(u, app->root, build_dir, err, sizeof(err));
            else if (strcmp(u->langs[l], "go") == 0)
                rc = build_go(u, app->root, build_dir, err, sizeof(err));
            else if (strcmp(u->langs[l], "js") == 0 || strcmp(u->langs[l], "ts") == 0)
                rc = build_js_ts(u, app->root, build_dir, err, sizeof(err));
            else if (strcmp(u->langs[l], "vite") == 0)
                rc = build_vite(u, app->root, build_dir, err, sizeof(err));
            else if (strcmp(u->langs[l], "rust") == 0)
                rc = build_rust(u, app->root, build_dir, err, sizeof(err));
            else {
                snprintf(err, sizeof(err), "unknown lang '%s'", u->langs[l]);
                rc = -1;
            }
            if (rc != 0) {
                printf("[arwn] unit %s (%s): BUILD FAILED: %s\n", u->name, u->langs[l], err);
                failures++;
                break;
            }
            compiled++;
        }

        if (compiled > 0) {
            if (assemble_arweb(u, app->root, build_dir, err, sizeof(err)) != 0) {
                printf("[arwn] unit %s: PACK FAILED: %s\n", u->name, err);
                failures++;
            }
        }
    }

    return failures == 0 ? 0 : -1;
}