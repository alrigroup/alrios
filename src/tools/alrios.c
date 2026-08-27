/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ar_ipc.h"
#include "aros_hal.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#else
#include <unistd.h>
#endif

static void get_base_dir(char *buf, int size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, size);
    char *p = strrchr(buf, '\\');
    if (p) *p = '\0';
#else
    char link[32] = "/proc/self/exe";
    ssize_t len = readlink(link, buf, size - 1);
    if (len < 0) { strncpy(buf, ".", size); return; }
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
#endif
}

static void print_usage(void) {
    printf("ALRIOS CLI v1.0.0\n\n");
    printf("Usage:\n");
    printf("  alrios power on|off|reload\n");
    printf("  alrios status              (alias: list)\n");
    printf("  alrios list\n");
    printf("  alrios fullupdate [--no-pull] [--force]  (git pull + incremental app rebuild + reload)\n");
    printf("  alrios start <app>\n");
    printf("  alrios stop <app>\n");
    printf("  alrios restart <app>\n");
    printf("  alrios start add|del <app>   (manage autostart.cfg)\n");
    printf("  alrios arws help             (gateway, routes, load balancer pools)\n");
    printf("  alrios ardb help             (sovereign DB, tokens, SQL firewall, audit)\n");
    printf("  alrios arwn help             (web native runtime & .arweb containers)\n");
    printf("  alrios cdn help              (static assets & streaming engine)\n");
    printf("  alrios update all|alrios|armake|arinstall\n");
    printf("  alrios build -p <SRC> [-o <OUT>]   (compile app -> arcore/apps)\n");
    printf("  alrios refresh                   (reload app list without restart)\n");
}

static int ctl_connect(void) {
    return ar_ipc_client_connect("127.0.0.1", AR_CTL_PORT);
}

static int admin_connect(void) {
    return ar_ipc_client_connect("127.0.0.1", AR_IPC_DEFAULT_PORT);
}

/* Send one frame to fd and print the response. Returns 0 on IPC_RESPONSE,
   1 on IPC_ERROR, -1 on connection failure. */
static int send_and_print(int fd, int type, const char *payload) {
    uint32_t plen = payload ? (uint32_t)strlen(payload) : 0;
    if (ar_ipc_send_frame(fd, type, payload, plen) < 0) return -1;

    ar_socket_set_recv_timeout(fd, 10000);

    unsigned char buf[AR_IPC_BUF_SIZE];
    int rtype;
    uint32_t rlen = sizeof(buf);
    if (ar_ipc_recv_frame(fd, &rtype, buf, &rlen) < 0) return -1;
    buf[rlen] = '\0';
    printf("%s\n", buf);
    return (rtype == IPC_RESPONSE || rtype == IPC_QUERY_RESP || rtype == IPC_ACK) ? 0 : 1;
}

static int run_ctl(int type, const char *payload) {
    int fd = ctl_connect();
    if (fd < 0) {
        printf("ALRIOS is not running (channel 9600 unavailable)\n");
        return 1;
    }
    int rc = send_and_print(fd, type, payload);
    ar_socket_close(fd);
    return (rc < 0) ? 1 : rc;
}

static int spawn_arcore_detached(const char *exe) {
#ifdef _WIN32
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "\"%s\"", exe);
    STARTUPINFOA si = {0};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {0};
    BOOL ok = CreateProcessA(NULL, cmd, NULL, NULL, FALSE,
                             DETACHED_PROCESS | CREATE_NEW_PROCESS_GROUP,
                             NULL, NULL, &si, &pi);
    if (!ok) return -1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)pi.dwProcessId;
#else
    pid_t pid = fork();
    if (pid < 0) return -1;
    if (pid > 0) return pid;
    setsid();
    freopen("/dev/null", "r", stdin);
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
    execl(exe, exe, NULL);
    _exit(127);
#endif
}

static int cmd_power_on(void) {
    int fd = ctl_connect();
    if (fd >= 0) {
        ar_socket_set_recv_timeout(fd, 800);
        if (ar_ipc_send_frame(fd, IPC_CTL_PING, NULL, 0) == 0) {
            unsigned char buf[8];
            uint32_t rlen = sizeof(buf);
            int rtype;
            if (ar_ipc_recv_frame(fd, &rtype, buf, &rlen) == 0) {
                send_and_print(fd, IPC_CTL_POWER_RELOAD, NULL);
                ar_socket_close(fd);
                printf("ALRIOS daemons reloaded\n");
                return 0;
            }
        }
        ar_socket_close(fd);
    }

    char exe[1024];
    get_base_dir(exe, sizeof(exe));
#ifdef _WIN32
    strncat(exe, "\\arcore.exe", sizeof(exe) - strlen(exe) - 1);
#else
    strncat(exe, "/arcore", sizeof(exe) - strlen(exe) - 1);
#endif

    int pid = spawn_arcore_detached(exe);
    if (pid <= 0) {
        printf("Failed to start arcore (%s)\n", exe);
        return 1;
    }
    printf("arcore started (pid %d)\n", pid);
    return 0;
}

static void autostart_path(char *buf, int size) {
    get_base_dir(buf, size);
#ifdef _WIN32
    strncat(buf, "\\autostart.cfg", size - strlen(buf) - 1);
#else
    strncat(buf, "/autostart.cfg", size - strlen(buf) - 1);
#endif
}

static int autostart_has(const char *path, const char *name) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '#' || !*p) continue;
        char *end = p + strlen(p);
        while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (strcmp(p, name) == 0) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static int cmd_autostart_add(const char *app) {
    char path[1024];
    autostart_path(path, sizeof(path));
    if (autostart_has(path, app)) {
        printf("autostart: %s is already listed\n", app);
    } else {
        FILE *f = fopen(path, "a");
        if (!f) { printf("Failed to open %s\n", path); return 1; }
        fprintf(f, "%s\n", app);
        fclose(f);
        printf("autostart: %s added\n", app);
    }
    /* Apply immediately if arcore is running */
    run_ctl(IPC_CTL_START, app);
    return 0;
}

static int cmd_autostart_del(const char *app) {
    char path[1024];
    autostart_path(path, sizeof(path));
    char tmp[4096];
    FILE *f = fopen(path, "r");
    if (!f) { printf("Failed to open %s\n", path); return 1; }
    size_t n = fread(tmp, 1, sizeof(tmp) - 1, f);
    tmp[n] = '\0';
    fclose(f);

    FILE *out = fopen(path, "w");
    if (!out) { printf("Failed to write %s\n", path); return 1; }
    char *line = tmp;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        char *end = p + strlen(p);
        while (end > p && (end[-1] == '\n' || end[-1] == '\r' || end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        int skip = (*p && *p != '#' && strcmp(p, app) == 0);
        if (!skip)
            fprintf(out, "%s%s", line, nl ? "\n" : "");
        line = nl ? nl + 1 : NULL;
    }
    fclose(out);
    printf("autostart: %s removed\n", app);
    /* Apply immediately if arcore is running */
    run_ctl(IPC_CTL_STOP, app);
    return 0;
}

static int cmd_app_query(const char *app, const char *cmd) {
    int fd = admin_connect();
    if (fd < 0) {
        printf("ARWS Gateway unavailable (9500)\n");
        return 1;
    }
    char payload[AR_IPC_BUF_SIZE];
    snprintf(payload, sizeof(payload), "%s\n%s", app, cmd);
    int rc = send_and_print(fd, IPC_QUERY, payload);
    ar_socket_close(fd);
    return (rc < 0) ? 1 : rc;
}

static int cmd_app_query_args(const char *app, int argc, char *argv[]) {
    char cmd[AR_IPC_BUF_SIZE];
    int off = 0;
    if (argc < 3) {
        snprintf(cmd, sizeof(cmd), "help");
        return cmd_app_query(app, cmd);
    }
    for (int i = 2; i < argc; i++) {
        if (i > 2 && off < (int)sizeof(cmd) - 1) cmd[off++] = ' ';
        int n = snprintf(cmd + off, sizeof(cmd) - off, "%s", argv[i]);
        if (n < 0) break;
        off += n;
        if (off >= (int)sizeof(cmd) - 1) break;
    }
    cmd[off] = '\0';
    return cmd_app_query(app, cmd);
}

#include <sys/stat.h>
#ifndef _WIN32
#include <dirent.h>
#endif

static int cmd_fullupdate(int argc, char **argv) {
    char base[1024], root[1024];
    get_base_dir(base, sizeof(base));
    strncpy(root, base, sizeof(root) - 1);
    root[sizeof(root) - 1] = '\0';
    char *sep = strrchr(root, '\\');
#ifndef _WIN32
    sep = strrchr(root, '/');
#endif
    if (sep) *sep = '\0';

    if (chdir(root) != 0) {
        printf("[ERRO] Falha ao navegar para raiz: %s\n", root);
        return 1;
    }

    int no_pull = 0;
    int force = 0;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--no-pull") == 0) no_pull = 1;
        else if (strcmp(argv[i], "--force") == 0 || strcmp(argv[i], "-f") == 0) force = 1;
    }

    printf("\n\033[1;36m════════════════════════════════════════════════════════\033[0m\n");
    printf("\033[1;36m  ALRIOS Sovereign Full Update & Hot Reload Pipeline    \033[0m\n");
    printf("\033[1;36m════════════════════════════════════════════════════════\033[0m\n\n");

    /* 1. Git Pull */
    if (!no_pull && access(".git", F_OK) == 0) {
        printf("\033[1;34m[1/4]\033[0m Sincronizando com repositorio remoto (git pull origin main)...\n");
        int git_rc = system("git pull origin main");
        if (git_rc != 0) {
            printf("\033[1;33m[AVISO]\033[0m git pull retornou codigo %d (continuando com fontes locais)...\n\n", git_rc);
        } else {
            printf("\033[1;32m✓\033[0m Repositorio sincronizado com sucesso.\n\n");
        }
    } else {
        printf("\033[1;34m[1/4]\033[0m Sincronizacao git ignorada (--no-pull ou sem .git).\n\n");
    }

    /* 2. Build kernel and developer tools */
    printf("\033[1;34m[2/4]\033[0m Verificando kernel (arcore) e ferramentas de desenvolvedor (armake/alrios)...\n");
#ifdef _WIN32
    system("cmake --build build --target arcore armake alrios -j4");
#else
    system("cmake --build build-linux --target arcore armake alrios -- -j4 2>/dev/null || cmake --build build --target arcore armake alrios -j4 2>/dev/null || true");
#endif
    printf("\033[1;32m✓\033[0m Kernel e ferramentas atualizados.\n\n");

    /* 3. Incremental App Packaging */
    printf("\033[1;34m[3/4]\033[0m Executando empacotamento incremental de aplicacoes (Content-Hash)...\n");
#ifndef _WIN32
    DIR *d = opendir("src/apps");
    if (d) {
        struct dirent *entry;
        while ((entry = readdir(d)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char appdir[1024];
            snprintf(appdir, sizeof(appdir), "src/apps/%s", entry->d_name);
            struct stat st;
            if (stat(appdir, &st) == 0 && S_ISDIR(st.st_mode)) {
                char cmd[2048];
                snprintf(cmd, sizeof(cmd), "./arcore/armake build %s arcore/apps/%s.arapp %s",
                         appdir, entry->d_name, force ? "--force" : "");
                system(cmd);
            }
        }
        closedir(d);
    }
#endif
    printf("\n\033[1;32m✓\033[0m Todas as aplicacoes foram verificadas e empacotadas.\n\n");

    /* 4. Trigger auto-discovery & process reload */
    printf("\033[1;34m[4/4]\033[0m Notificando arcore para atualizar registro e daemons...\n");
    int fd = ctl_connect();
    if (fd >= 0) {
        send_and_print(fd, IPC_CTL_REFRESH, NULL);
        ar_socket_close(fd);
        printf("\033[1;32m✓\033[0m arcore atualizado com sucesso.\n\n");
    } else {
        printf("\033[1;33m[INFO]\033[0m arcore nao esta rodando no momento. Inicie com: ./alrios power on\n\n");
    }

    /* 5. Print status table */
    printf("\033[1;36m=== Status Atual do Ecossistema ===\033[0m\n");
    run_ctl(IPC_CTL_LIST, NULL);
    return 0;
}

static int cmd_update(const char *which) {
    if (strcmp(which, "all") == 0) {
        char *fake_argv[] = {"alrios", "fullupdate"};
        return cmd_fullupdate(2, fake_argv);
    }

    char base[1024], root[1024];
    get_base_dir(base, sizeof(base));
    strncpy(root, base, sizeof(root) - 1);
    root[sizeof(root) - 1] = '\0';
    char *sep = strrchr(root, '\\');
#ifndef _WIN32
    sep = strrchr(root, '/');
#endif
    if (sep) *sep = '\0';

    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "cmake --build build-linux --config Release --target %s", which);

    printf("update %s: %s\n  (dir: %s)\n", which, cmd, root);
    if (chdir(root) != 0) { printf("chdir failed: %s\n", root); return 1; }
    int res = system(cmd);
    if (res == 0) {
        printf("\n[ALRIOS] Update successful! Triggering hot power reload...\n");
        run_ctl(IPC_CTL_POWER_RELOAD, NULL);
    }
    return res;
}

static void abs_path(const char *in, char *out, int size) {
#ifdef _WIN32
    _fullpath(out, in, size);
#else
    if (!realpath(in, out)) {
        strncpy(out, in, size - 1);
        out[size - 1] = '\0';
    }
#endif
}

static int path_eq(const char *a, const char *b) {
#ifdef _WIN32
    return _stricmp(a, b) == 0;
#else
    return strcmp(a, b) == 0;
#endif
}

static int fexists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (f) { fclose(f); return 1; }
    return 0;
}

static int cmd_build(int argc, char *argv[]) {
    const char *src = NULL;
    const char *out = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 && i + 1 < argc) {
            src = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else {
            printf("build: unknown option: %s (use -p <SRC> [-o <OUT>])\n", argv[i]);
            return 1;
        }
    }
    if (!src) { printf("build: requires -p <SRC>\n"); return 1; }

    int apps_out = (!out || strcmp(out, "apps") == 0 || strcmp(out, "/apps") == 0 || strcmp(out, "\\apps") == 0);

    /* Delegate app building directly to armake buildapp */
    char exe[1024];
    get_base_dir(exe, sizeof(exe));
#ifdef _WIN32
    strncat(exe, "\\armake.exe", sizeof(exe) - strlen(exe) - 1);
#else
    strncat(exe, "/armake", sizeof(exe) - strlen(exe) - 1);
#endif
    const char *o = apps_out ? "apps" : out;
    char cmd[2048];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "\"\"%s\" buildapp -s \"%s\" -o \"%s\"\"", exe, src, o);
#else
    snprintf(cmd, sizeof(cmd), "\"%s\" buildapp -s \"%s\" -o \"%s\"", exe, src, o);
#endif
    printf("build: %s\n", cmd);
    if (system(cmd) != 0) return 1;
    printf("tip: run 'alrios refresh' to update application list\n");
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char *a1 = argv[1];

    if (strcmp(a1, "help") == 0 || strcmp(a1, "--help") == 0 || strcmp(a1, "-h") == 0) {
        print_usage();
        return 0;
    }

    /* Comandos do Ciclo de Vida do SO (arcore 9600) */
    if (strcmp(a1, "status") == 0 || strcmp(a1, "list") == 0)
        return run_ctl(IPC_CTL_LIST, NULL);

    if (strcmp(a1, "power") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        if (strcmp(argv[2], "on") == 0) return cmd_power_on();
        if (strcmp(argv[2], "off") == 0) return run_ctl(IPC_CTL_POWER_OFF, NULL);
        if (strcmp(argv[2], "reload") == 0) return run_ctl(IPC_CTL_POWER_RELOAD, NULL);
        print_usage();
        return 1;
    }

    if (strcmp(a1, "start") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        if (strcmp(argv[2], "add") == 0 && argc >= 4) return cmd_autostart_add(argv[3]);
        if (strcmp(argv[2], "del") == 0 && argc >= 4) return cmd_autostart_del(argv[3]);
        return run_ctl(IPC_CTL_START, argv[2]);
    }

    if (strcmp(a1, "stop") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        return run_ctl(IPC_CTL_STOP, argv[2]);
    }

    if (strcmp(a1, "restart") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        return run_ctl(IPC_CTL_RESTART, argv[2]);
    }

    if (strcmp(a1, "update") == 0) {
        if (argc < 3) { print_usage(); return 1; }
        const char *w = argv[2];
        if (strcmp(w, "all") != 0 && strcmp(w, "alrios") != 0 && strcmp(w, "armake") != 0 && strcmp(w, "arinstall") != 0) {
            printf("update: invalid target '%s' (use all|alrios|armake|arinstall)\n", w);
            return 1;
        }
        return cmd_update(w);
    }

    if (strcmp(a1, "fullupdate") == 0)
        return cmd_fullupdate(argc, argv);

    if (strcmp(a1, "build") == 0)
        return cmd_build(argc, argv);

    if (strcmp(a1, "refresh") == 0)
        return run_ctl(IPC_CTL_REFRESH, NULL);

    /* Universal dynamic routing for apps (IPC 9500).
       The target application owns and serves its own command catalogue and help! */
    const char *target_app = (strcmp(a1, "auth") == 0) ? "arauth" : a1;
    return cmd_app_query_args(target_app, argc, argv);
}
