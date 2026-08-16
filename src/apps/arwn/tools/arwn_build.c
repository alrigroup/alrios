/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

/* arwn_build — CLI da ARWN (Fase 0/1):
     init <dir> [--framework vanilla|react|vue]
     build <dir> [--out <outdir>]
     config --validate <dir>
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <unistd.h>
#endif

#include "arwn.h"
#include "aros_hal.h"

static const char *VANILLA_CONFIG =
    "# Configuracao do app ARWN\n"
    "[app]\n"
    "name=meuapp\n"
    "port=3001\n"
    "bind=127.0.0.1\n"
    "\n"
    "[arws]\n"
    "gateway=127.0.0.1:9500\n"
    "route.host=alrigroup.com\n"
    "route.path=/*\n"
    "route.mode=production\n"
    "\n"
    "[unit:main]\n"
    "source=web/\n"
    "entry=index.arhtml\n"
    "compile=main.js\n"
    "compile.lang=js\n"
    "obfuscate=yes\n"
    "\n"
    "[assets]\n"
    "dir=assets\n"
    "obfuscate=no\n"
    "cache=immutable\n";

static const char *VANILLA_HTML =
    "<!DOCTYPE html>\n"
    "<html lang=\"pt-BR\">\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "  <title>meuapp</title>\n"
    "</head>\n"
    "<body>\n"
    "  <h1 id=\"app\">ARWN</h1>\n"
    "  <script src=\"./main.js\"></script>\n"
    "</body>\n"
    "</html>\n";

static const char *VANILLA_JS =
    "// app ARWN - front-end JS (bundled by esbuild)\n"
    "window.ARWN && ARWN.ready(function (bridge) {\n"
    "  const el = document.getElementById('app');\n"
    "  if (el) el.textContent = 'ARWN pronto (' + bridge.version + ')';\n"
    "});\n";

static const char *REACT_CONFIG =
    "# Configuracao do app ARWN (React)\n"
    "[app]\n"
    "name=meuapp-react\n"
    "port=3001\n"
    "bind=127.0.0.1\n"
    "\n"
    "[arws]\n"
    "gateway=127.0.0.1:9500\n"
    "route.host=alrigroup.com\n"
    "route.path=/*\n"
    "route.mode=production\n"
    "\n"
    "[unit:main]\n"
    "source=web/\n"
    "entry=index.arhtml\n"
    "compile=src/App.tsx,src/main.tsx\n"
    "compile.lang=vite\n"
    "obfuscate=yes\n"
    "\n"
    "[assets]\n"
    "dir=assets\n"
    "obfuscate=no\n"
    "cache=immutable\n";

static const char *REACT_HTML =
    "<!DOCTYPE html>\n"
    "<html lang=\"pt-BR\">\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "  <title>meuapp-react</title>\n"
    "</head>\n"
    "<body>\n"
    "  <div id=\"root\"></div>\n"
    "  <script type=\"module\" src=\"/src/main.tsx\"></script>\n"
    "</body>\n"
    "</html>\n";

static const char *REACT_TSX =
    "import React, { useEffect, useState } from 'react';\n"
    "import ReactDOM from 'react-dom/client';\n"
    "\n"
    "export function App() {\n"
    "  const [status, setStatus] = useState('conectando...');\n"
    "  useEffect(() => {\n"
    "    if (window.ARWN) {\n"
    "      window.ARWN.ready((bridge) => setStatus('ARWN pronto (' + bridge.version + ')'));\n"
    "    }\n"
    "  }, []);\n"
    "  return <h1>ARWN React: {status}</h1>;\n"
    "}\n"
    "\n"
    "const root = document.getElementById('root');\n"
    "if (root) ReactDOM.createRoot(root).render(<App />);\n";

static const char *VUE_CONFIG =
    "# Configuracao do app ARWN (Vue)\n"
    "[app]\n"
    "name=meuapp-vue\n"
    "port=3001\n"
    "bind=127.0.0.1\n"
    "\n"
    "[arws]\n"
    "gateway=127.0.0.1:9500\n"
    "route.host=alrigroup.com\n"
    "route.path=/*\n"
    "route.mode=production\n"
    "\n"
    "[unit:main]\n"
    "source=web/\n"
    "entry=index.arhtml\n"
    "compile=src/App.vue,src/main.ts\n"
    "compile.lang=vite\n"
    "obfuscate=yes\n"
    "\n"
    "[assets]\n"
    "dir=assets\n"
    "obfuscate=no\n"
    "cache=immutable\n";

static const char *VUE_HTML =
    "<!DOCTYPE html>\n"
    "<html lang=\"pt-BR\">\n"
    "<head>\n"
    "  <meta charset=\"UTF-8\">\n"
    "  <meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
    "  <title>meuapp-vue</title>\n"
    "</head>\n"
    "<body>\n"
    "  <div id=\"app\"></div>\n"
    "  <script type=\"module\" src=\"/src/main.ts\"></script>\n"
    "</body>\n"
    "</html>\n";

static const char *VUE_MAIN_TS =
    "import { createApp } from 'vue';\n"
    "import App from './App.vue';\n"
    "\n"
    "createApp(App).mount('#app');\n";

static const char *VUE_APP =
    "<template>\n"
    "  <h1>ARWN Vue: {{ status }}</h1>\n"
    "</template>\n"
    "<script setup lang=\"ts\">\n"
    "import { ref, onMounted } from 'vue';\n"
    "const status = ref('conectando...');\n"
    "onMounted(() => {\n"
    "  if (window.ARWN) {\n"
    "    window.ARWN.ready((bridge: any) => {\n"
    "      status.value = 'ARWN pronto (' + bridge.version + ')';\n"
    "    });\n"
    "  }\n"
    "});\n"
    "</script>\n";

static void usage(void) {
    printf(
        "arwn_build - ARWN builder CLI\n"
        "Usage:\n"
        "  arwn_build init <dir> [--framework vanilla|react|vue]\n"
        "  arwn_build build <dir>\n"
        "  arwn_build serve <dir>\n"
        "  arwn_build config --validate <dir>\n");
}

static int write_file(const char *path, const char *content) {
    FILE *f = fopen(path, "wb");
    if (!f) { printf("cannot write %s\n", path); return -1; }
    fputs(content, f);
    fclose(f);
    printf("  created %s\n", path);
    return 0;
}

static int ensure_dir(const char *path) {
    if (ar_fs_exists(path) == 1) return 0;
    if (ar_fs_mkdir(path) == 0) return 0;
    printf("cannot create dir %s\n", path);
    return -1;
}

static int cmd_init(const char *dir, const char *framework) {
    char cfg[1300];
    snprintf(cfg, sizeof(cfg), "%s/config.arwn", dir);
    char web[1300];
    snprintf(web, sizeof(web), "%s/web", dir);
    char web_src[1300];
    snprintf(web_src, sizeof(web_src), "%s/web/src", dir);
    char assets[1300];
    snprintf(assets, sizeof(assets), "%s/assets", dir);

    if (ensure_dir(dir) != 0) return 1;
    if (ensure_dir(web) != 0) return 1;
    if (ensure_dir(assets) != 0) return 1;

    char html[1300];
    snprintf(html, sizeof(html), "%s/web/index.arhtml", dir);

    if (framework && strcmp(framework, "react") == 0) {
        if (ensure_dir(web_src) != 0) return 1;
        char tsx[1300];
        snprintf(tsx, sizeof(tsx), "%s/web/src/main.tsx", dir);
        write_file(cfg, REACT_CONFIG);
        write_file(html, REACT_HTML);
        write_file(tsx, REACT_TSX);
        printf("ARWN app scaffolded in %s (react)\n", dir);
    } else if (framework && strcmp(framework, "vue") == 0) {
        if (ensure_dir(web_src) != 0) return 1;
        char main_ts[1300];
        snprintf(main_ts, sizeof(main_ts), "%s/web/src/main.ts", dir);
        char app_vue[1300];
        snprintf(app_vue, sizeof(app_vue), "%s/web/src/App.vue", dir);
        write_file(cfg, VUE_CONFIG);
        write_file(html, VUE_HTML);
        write_file(main_ts, VUE_MAIN_TS);
        write_file(app_vue, VUE_APP);
        printf("ARWN app scaffolded in %s (vue)\n", dir);
    } else {
        char js[1300];
        snprintf(js, sizeof(js), "%s/web/main.js", dir);
        write_file(cfg, VANILLA_CONFIG);
        write_file(html, VANILLA_HTML);
        write_file(js, VANILLA_JS);
        printf("ARWN app scaffolded in %s (vanilla)\n", dir);
    }

    return 0;
}

static int cmd_build(const char *dir, const char *out_dir) {
    arwn_app_t *app = arwn_app_new("app");
    if (!app) return 1;

    char cfg[1300];
    snprintf(cfg, sizeof(cfg), "%s/config.arwn", dir);
    if (arwn_config_load(app, cfg) != 0) {
        printf("config error: %s\n", arwn_config_last_error(app));
        arwn_app_free(app);
        return 1;
    }
    printf("[arwn] config ok: %d unit(s)\n", arwn_config_unit_count(app));

    if (arwn_builder_execute_out(app, out_dir) != 0) {
        printf("[arwn] build failed\n");
        arwn_app_free(app);
        return 1;
    }

    arwn_app_free(app);
    printf("[arwn] build done\n");
    return 0;
}

static int get_exe_dir(char *buf, size_t size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, (DWORD)size);
    char *p = strrchr(buf, '\\');
    if (p) *p = '\0';
    return 0;
#else
    ssize_t len = readlink("/proc/self/exe", buf, size - 1);
    if (len < 0) return -1;
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
    return 0;
#endif
}

/* serve <dir>: build (se preciso) + mount (event loop do server + gateway) */
static int cmd_serve(const char *dir) {
    char target_dir[1300];
    snprintf(target_dir, sizeof(target_dir), "%s", dir ? dir : ".");

    char cfg[1300];
    snprintf(cfg, sizeof(cfg), "%s/config.arwn", target_dir);

    /* If config.arwn not in target_dir, check next to executable */
    FILE *chk = fopen(cfg, "rb");
    if (!chk) {
        char exe_dir[1024];
        if (get_exe_dir(exe_dir, sizeof(exe_dir)) == 0) {
            snprintf(cfg, sizeof(cfg), "%s/config.arwn", exe_dir);
            FILE *chk2 = fopen(cfg, "rb");
            if (chk2) {
                fclose(chk2);
                snprintf(target_dir, sizeof(target_dir), "%s", exe_dir);
            }
        }
    } else {
        fclose(chk);
    }

    arwn_app_t *app = arwn_app_new(target_dir);
    if (!app) return 1;

    if (arwn_config_load(app, cfg) != 0) {
        printf("config error: %s\n", arwn_config_last_error(app));
        arwn_app_free(app);
        return 1;
    }
    printf("[arwn] config ok: %d unit(s)\n", arwn_config_unit_count(app));

    int rc = arwn_mount(app);
    arwn_app_free(app);
    return rc;
}

static int cmd_config_validate(const char *dir) {
    arwn_app_t *app = arwn_app_new("app");
    if (!app) return 1;

    char cfg[1300];
    snprintf(cfg, sizeof(cfg), "%s/config.arwn", dir);
    if (arwn_config_load(app, cfg) != 0) {
        printf("config INVALID: %s\n", arwn_config_last_error(app));
        arwn_app_free(app);
        return 1;
    }

    printf("config VALID: %d unit(s)\n", arwn_config_unit_count(app));
    for (int i = 0; i < arwn_config_unit_count(app); i++) {
        const arwn_unit_t *u = arwn_config_unit(app, i);
        printf("  unit %s: source=%s entry=%s langs=%s\n",
               u->name, u->source, u->entry,
               u->lang_count > 0 ? u->langs[0] : "(none)");
    }
    arwn_app_free(app);
    return 0;
}

int main(int argc, char **argv) {
    /* If run without arguments or with flags, default to serving current directory */
    if (argc < 2 || (argc >= 2 && argv[1][0] == '-')) {
        return cmd_serve(".");
    }

    if (strcmp(argv[1], "init") == 0) {
        if (argc < 3) { usage(); return 1; }
        const char *framework = "vanilla";
        if (argc >= 5 && strcmp(argv[3], "--framework") == 0) framework = argv[4];
        return cmd_init(argv[2], framework);
    }
    if (strcmp(argv[1], "build") == 0) {
        if (argc < 3) { usage(); return 1; }
        const char *out_dir = NULL;
        /* Procura --out <dir> na linha de comando */
        for (int i = 3; i < argc - 1; i++) {
            if (strcmp(argv[i], "--out") == 0) {
                out_dir = argv[i + 1];
                break;
            }
        }
        return cmd_build(argv[2], out_dir);
    }
    if (strcmp(argv[1], "serve") == 0) {
        const char *dir = (argc >= 3) ? argv[2] : ".";
        return cmd_serve(dir);
    }
    if (strcmp(argv[1], "config") == 0 && argc >= 4 &&
        strcmp(argv[2], "--validate") == 0) {
        return cmd_config_validate(argv[3]);
    }

    /* If passed a directory directly, serve it */
    if (argc == 2 && argv[1][0] != '-') {
        return cmd_serve(argv[1]);
    }

    usage();
    return 1;
}