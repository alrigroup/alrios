/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "installer_core.h"
#include "cli_ui.h"
#include "gtk3_gui.h"
#include "win32_gui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

static void get_self_exe(char *buf, size_t size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, (DWORD)size);
#else
    ssize_t len = readlink("/proc/self/exe", buf, size - 1);
    if (len > 0) {
        buf[len] = '\0';
    } else {
        strncpy(buf, ".", size);
    }
#endif
}

static void print_usage(const char *prog) {
    printf("ALRIOS Sovereign Master Installer\n\n");
    printf("Uso:\n");
    printf("  %s [opcoes]\n\n", prog);
    printf("Opcoes:\n");
    printf("  --cli, -c              Forcar interface de linha de comando (CLI)\n");
    printf("  --gui, -g              Forcar interface grafica de instalacao (GUI)\n");
    printf("  -y, --yes              Modo nao-interativo / unattended (aceita padroes)\n");
    printf("  -d, --dir <caminho>    Definir diretorio de destino personalizado\n");
    printf("  --no-path              Nao registrar ALRIOS no PATH do sistema\n");
    printf("  --no-gcc               Nao instalar compilador GCC automaticamente\n");
    printf("  --no-node              Nao instalar runtime Node.js\n");
    printf("  --no-python            Nao instalar runtime Python\n");
    printf("  --dry-run              Simular processo sem gravar no disco\n");
    printf("  -h, --help             Exibir esta ajuda\n\n");
}

int main(int argc, char **argv) {
    installer_config_t cfg;
    memset(&cfg, 0, sizeof(cfg));

    get_self_exe(cfg.self_exe, sizeof(cfg.self_exe));
    cfg.add_to_path = 1;
    cfg.install_gcc = 1;
    cfg.install_node = 1;
    cfg.install_python = 1;

    int force_cli = 0;
    int force_gui = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--cli") == 0 || strcmp(argv[i], "-c") == 0) {
            force_cli = 1;
        } else if (strcmp(argv[i], "--gui") == 0 || strcmp(argv[i], "-g") == 0) {
            force_gui = 1;
        } else if (strcmp(argv[i], "-y") == 0 || strcmp(argv[i], "--yes") == 0 || strcmp(argv[i], "--unattended") == 0) {
            cfg.is_unattended = 1;
        } else if (strcmp(argv[i], "--no-path") == 0) {
            cfg.add_to_path = 0;
        } else if (strcmp(argv[i], "--no-gcc") == 0) {
            cfg.install_gcc = 0;
        } else if (strcmp(argv[i], "--no-node") == 0) {
            cfg.install_node = 0;
        } else if (strcmp(argv[i], "--no-python") == 0) {
            cfg.install_python = 0;
        } else if (strcmp(argv[i], "--dry-run") == 0) {
            cfg.dry_run = 1;
        } else if ((strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--dir") == 0) && i + 1 < argc) {
            strncpy(cfg.dest_dir, argv[++i], sizeof(cfg.dest_dir) - 1);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        }
    }

    /* ─── Deteccao de Interface (CLI vs GUI) ─── */
    int should_run_gui = 0;

    if (force_gui) {
        should_run_gui = 1;
    } else if (force_cli || cfg.is_unattended) {
        should_run_gui = 0;
    } else {
#ifdef _WIN32
        /* No Windows: se aberto por 2 cliques no Explorer (sem console anexado de pai) */
        DWORD procList[2];
        if (GetConsoleProcessList(procList, 2) <= 1) {
            should_run_gui = 1;
        } else {
            should_run_gui = 0;
        }
#else
        /* No Linux: se $DISPLAY ou $WAYLAND_DISPLAY estiver setado e stdout nao for terminal interativo unico */
        const char *disp = getenv("DISPLAY");
        const char *wayland = getenv("WAYLAND_DISPLAY");
        if ((disp && disp[0]) || (wayland && wayland[0])) {
            /* Se executado pelo terminal (isatty), prioriza CLI para conforto do desenvolvedor */
            if (isatty(0) && isatty(1)) {
                should_run_gui = 0;
            } else {
                should_run_gui = 1;
            }
        } else {
            should_run_gui = 0;
        }
#endif
    }

    /* ─── Execucao ─── */
    if (should_run_gui) {
#ifdef _WIN32
        int rc = run_win32_installer(&cfg);
        if (rc == 0) return 0;
#elif defined(HAS_GTK3)
        int rc = run_gtk3_installer(argc, argv, &cfg);
        if (rc == 0) return 0;
#endif
    }

    /* Fallback garantido para CLI */
    return run_cli_installer(&cfg);
}
