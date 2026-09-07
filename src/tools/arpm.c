/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ar_ipc.h"
#include "aros_hal.h"
#include "zip.h"
#include "arapp_parser.h"
#include "arpm.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <urlmon.h>
#pragma comment(lib, "urlmon.lib")
#define SEPARATOR '\\'
#define mkdir_p_(p) _mkdir(p)
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#define SEPARATOR '/'
#define mkdir_p_(p) mkdir(p, 0755)
#endif

#include <openssl/evp.h>

#define ARPM_VERSION "1.0.0"
#define DEFAULT_REGISTRY_URL "https://raw.githubusercontent.com/alrigroup/alrios/main/arcore/registry.json"

/* ANSI Colors */
#define CLR_RESET   "\033[0m"
#define CLR_BOLD    "\033[1m"
#define CLR_RED     "\033[1;31m"
#define CLR_GREEN   "\033[1;32m"
#define CLR_YELLOW  "\033[1;33m"
#define CLR_BLUE    "\033[1;34m"
#define CLR_MAGENTA "\033[1;35m"
#define CLR_CYAN    "\033[1;36m"
#define CLR_WHITE   "\033[1;37m"

/* Context Paths */
typedef struct {
    char root_dir[1024];
    char arcore_dir[1024];
    char apps_dir[1024];
    char programfiles_dir[1024];
    char storage_dir[1024];
    char etc_dir[1024];
    char staging_dir[1024];
    char autostart_cfg[1024];
    char packages_json[1024];
    char registry_cache[1024];
    char repositories_cfg[1024];
    char armake_bin[1024];
} arpm_ctx_t;

static arpm_ctx_t g_ctx;

/* File utilities */
static int file_exists(const char *path) {
#ifdef _WIN32
    return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
    return access(path, F_OK) == 0;
#endif
}

static long file_size(const char *path) {
    struct stat st;
    if (stat(path, &st) == 0) return (long)st.st_size;
    return -1;
}

static void mkdir_p(const char *dir) {
    char tmp[1024];
    snprintf(tmp, sizeof(tmp), "%s", dir);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
            mkdir_p_(tmp);
            *p = save;
        }
    }
    mkdir_p_(tmp);
}

static void remove_recursive(const char *path) {
#ifdef _WIN32
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "rmdir /S /Q \"%s\" >nul 2>nul", path);
    system(cmd);
#else
    char cmd[1200];
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
    (void)system(cmd);
#endif
}

/* Base directories discovery */
static void init_paths(void) {
    char exe[1024];
#ifdef _WIN32
    GetModuleFileNameA(NULL, exe, sizeof(exe));
#else
    char link[32] = "/proc/self/exe";
    ssize_t len = readlink(link, exe, sizeof(exe) - 1);
    if (len < 0) strncpy(exe, ".", sizeof(exe));
    else exe[len] = '\0';
#endif
    char *p = strrchr(exe, SEPARATOR);
    if (p) *p = '\0';

    /* If executable is in <root>/arcore */
    char *last = strrchr(exe, SEPARATOR);
    if (last && (strcmp(last + 1, "arcore") == 0 || strcmp(last + 1, "arcore.exe") == 0)) {
        snprintf(g_ctx.arcore_dir, sizeof(g_ctx.arcore_dir), "%s", exe);
        *last = '\0';
        snprintf(g_ctx.root_dir, sizeof(g_ctx.root_dir), "%s", exe);
    } else {
        snprintf(g_ctx.root_dir, sizeof(g_ctx.root_dir), "%s", exe);
        snprintf(g_ctx.arcore_dir, sizeof(g_ctx.arcore_dir), "%s%carcore", exe, SEPARATOR);
    }

    snprintf(g_ctx.apps_dir, sizeof(g_ctx.apps_dir), "%s%capps", g_ctx.arcore_dir, SEPARATOR);
    snprintf(g_ctx.programfiles_dir, sizeof(g_ctx.programfiles_dir), "%s%cprogramfiles", g_ctx.arcore_dir, SEPARATOR);
    snprintf(g_ctx.storage_dir, sizeof(g_ctx.storage_dir), "%s%cstorage", g_ctx.arcore_dir, SEPARATOR);
    snprintf(g_ctx.etc_dir, sizeof(g_ctx.etc_dir), "%s%cetc", g_ctx.arcore_dir, SEPARATOR);
    snprintf(g_ctx.staging_dir, sizeof(g_ctx.staging_dir), "%s%c.staging", g_ctx.arcore_dir, SEPARATOR);
    snprintf(g_ctx.autostart_cfg, sizeof(g_ctx.autostart_cfg), "%s%cautostart.cfg", g_ctx.arcore_dir, SEPARATOR);
    snprintf(g_ctx.packages_json, sizeof(g_ctx.packages_json), "%s%cpackages.json", g_ctx.etc_dir, SEPARATOR);
    snprintf(g_ctx.registry_cache, sizeof(g_ctx.registry_cache), "%s%cregistry_cache.json", g_ctx.etc_dir, SEPARATOR);
    snprintf(g_ctx.repositories_cfg, sizeof(g_ctx.repositories_cfg), "%s%crepositories.cfg", g_ctx.etc_dir, SEPARATOR);
#ifdef _WIN32
    snprintf(g_ctx.armake_bin, sizeof(g_ctx.armake_bin), "%s%carmake.exe", g_ctx.arcore_dir, SEPARATOR);
#else
    snprintf(g_ctx.armake_bin, sizeof(g_ctx.armake_bin), "%s%carmake", g_ctx.arcore_dir, SEPARATOR);
#endif

    mkdir_p(g_ctx.apps_dir);
    mkdir_p(g_ctx.etc_dir);
    mkdir_p(g_ctx.staging_dir);
}

/* SHA256 checksum calculator */
static int calc_file_sha256(const char *path, char *out_hex) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    EVP_MD_CTX *ctx = EVP_MD_CTX_new();
    if (!ctx) { fclose(f); return -1; }

    if (EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) != 1) {
        EVP_MD_CTX_free(ctx);
        fclose(f);
        return -1;
    }

    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        if (EVP_DigestUpdate(ctx, buf, n) != 1) {
            EVP_MD_CTX_free(ctx);
            fclose(f);
            return -1;
        }
    }
    fclose(f);

    unsigned char md[EVP_MAX_MD_SIZE];
    unsigned int md_len = 0;
    if (EVP_DigestFinal_ex(ctx, md, &md_len) != 1) {
        EVP_MD_CTX_free(ctx);
        return -1;
    }
    EVP_MD_CTX_free(ctx);

    for (unsigned int i = 0; i < md_len; i++) {
        sprintf(out_hex + (i * 2), "%02x", md[i]);
    }
    out_hex[md_len * 2] = '\0';
    return 0;
}

/* HTTP/HTTPS Downloader */
static int download_file(const char *url, const char *dest, int show_progress) {
    if (show_progress) {
        printf("  %sBaixando:%s %s\n", CLR_CYAN, CLR_RESET, url);
    }
#ifdef _WIN32
    HRESULT hr = URLDownloadToFileA(NULL, url, dest, 0, NULL);
    if (SUCCEEDED(hr) && file_exists(dest) && file_size(dest) > 0) {
        return 0;
    }
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "curl.exe -f -L -s -S -o \"%s\" \"%s\"", dest, url);
    return system(cmd);
#else
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "curl -f -L -s -S -o \"%s\" \"%s\"", dest, url);
    int rc = system(cmd);
    if (rc == 0 && file_exists(dest) && file_size(dest) > 0) {
        return 0;
    }
    return -1;
#endif
}

/* IPC Communications with arcore daemon */
static int ctl_connect(void) {
    return ar_ipc_client_connect("127.0.0.1", AR_CTL_PORT);
}

static int send_ctl(int type, const char *payload, char *resp_out, size_t resp_max) {
    int fd = ctl_connect();
    if (fd < 0) return -1;

    uint32_t plen = payload ? (uint32_t)strlen(payload) : 0;
    if (ar_ipc_send_frame(fd, type, payload, plen) < 0) {
        ar_socket_close(fd);
        return -1;
    }

    ar_socket_set_recv_timeout(fd, 5000);

    unsigned char buf[AR_IPC_BUF_SIZE];
    int rtype = 0;
    uint32_t rlen = sizeof(buf);
    if (ar_ipc_recv_frame(fd, &rtype, buf, &rlen) < 0) {
        ar_socket_close(fd);
        return -1;
    }
    ar_socket_close(fd);

    buf[rlen] = '\0';
    if (resp_out && resp_max > 0) {
        strncpy(resp_out, (char *)buf, resp_max - 1);
        resp_out[resp_max - 1] = '\0';
    }
    return (rtype == IPC_RESPONSE || rtype == IPC_QUERY_RESP || rtype == IPC_ACK) ? 0 : 1;
}

static void trigger_arcore_refresh(void) {
    printf("  %s[IPC]%s Notificando daemon arcore (hot reload)...\n", CLR_CYAN, CLR_RESET);
    char resp[1024] = {0};
    if (send_ctl(IPC_CTL_REFRESH, NULL, resp, sizeof(resp)) == 0) {
        printf("  %s✓%s arcore atualizado com sucesso.\n", CLR_GREEN, CLR_RESET);
    } else {
        printf("  %s!%s arcore nao esta em execucao (aplicativo sera ativado na proxima inicializacao).\n", CLR_YELLOW, CLR_RESET);
    }
}

static void trigger_arcore_start(const char *app) {
    char resp[1024] = {0};
    if (send_ctl(IPC_CTL_START, app, resp, sizeof(resp)) == 0) {
        printf("  %s✓%s Aplicativo '%s' iniciado.\n", CLR_GREEN, CLR_RESET, app);
    }
}

static void trigger_arcore_stop(const char *app) {
    char resp[1024] = {0};
    send_ctl(IPC_CTL_STOP, app, resp, sizeof(resp));
}

/* Autostart.cfg management */
static int autostart_has(const char *app) {
    FILE *f = fopen(g_ctx.autostart_cfg, "r");
    if (!f) return 0;
    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        char *end = p + strlen(p);
        while (end > p && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';
        if (*p && *p != '#' && strcmp(p, app) == 0) {
            fclose(f);
            return 1;
        }
    }
    fclose(f);
    return 0;
}

static int autostart_add(const char *app) {
    if (autostart_has(app)) return 0;
    FILE *f = fopen(g_ctx.autostart_cfg, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", app);
    fclose(f);
    printf("  %s+ [autostart]%s '%s' adicionado ao autostart.cfg\n", CLR_GREEN, CLR_RESET, app);
    return 0;
}

static int autostart_remove(const char *app) {
    if (!file_exists(g_ctx.autostart_cfg)) return 0;
    FILE *f = fopen(g_ctx.autostart_cfg, "r");
    if (!f) return -1;

    char tmp_file[1024];
    snprintf(tmp_file, sizeof(tmp_file), "%s.tmp", g_ctx.autostart_cfg);
    FILE *out = fopen(tmp_file, "w");
    if (!out) { fclose(f); return -1; }

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        char clean[256];
        strncpy(clean, line, sizeof(clean) - 1);
        clean[sizeof(clean) - 1] = '\0';
        char *p = clean;
        while (*p == ' ' || *p == '\t') p++;
        char *end = p + strlen(p);
        while (end > p && (end[-1] == '\r' || end[-1] == '\n' || end[-1] == ' ' || end[-1] == '\t')) end--;
        *end = '\0';

        if (*p && *p != '#' && strcmp(p, app) == 0) {
            found = 1;
            continue;
        }
        fputs(line, out);
    }
    fclose(f);
    fclose(out);

#ifdef _WIN32
    remove(g_ctx.autostart_cfg);
    rename(tmp_file, g_ctx.autostart_cfg);
#else
    rename(tmp_file, g_ctx.autostart_cfg);
#endif
    if (found) {
        printf("  %s-%s [autostart] '%s' removido do autostart.cfg\n", CLR_YELLOW, CLR_RESET, app);
    }
    return 0;
}

/* Local Packages metadata tracking */
static void record_package(const char *name, const char *version, const char *source, const char *type, int autostart, const char *sha) {
    FILE *f = fopen(g_ctx.packages_json, "a+");
    if (!f) return;

    time_t now = time(NULL);
    char tbuf[64];
    strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(f, "pkg:%s|ver:%s|src:%s|type:%s|auto:%d|sha:%s|date:%s\n",
            name, version ? version : "1.0.0", source ? source : "remote",
            type ? type : "binary", autostart, sha ? sha : "", tbuf);
    fclose(f);
}

/* Helper to find manifest in directory */
static int find_manifest(const char *dir, char *out, size_t out_size) {
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.*", dir);
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern, &ffd);
    if (h == INVALID_HANDLE_VALUE) return -1;
    do {
        if (!(ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            const char *ext = strrchr(ffd.cFileName, '.');
            if (ext && (strcasecmp(ext, ".arappmake") == 0 || strcasecmp(ext, ".armake") == 0)) {
                snprintf(out, out_size, "%s%c%s", dir, SEPARATOR, ffd.cFileName);
                FindClose(h);
                return 0;
            }
        }
    } while (FindNextFileA(h, &ffd));
    FindClose(h);
#else
    DIR *d = opendir(dir);
    if (!d) return -1;
    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (ent->d_type == DT_REG || ent->d_type == DT_UNKNOWN) {
            const char *ext = strrchr(ent->d_name, '.');
            if (ext && (strcasecmp(ext, ".arappmake") == 0 || strcasecmp(ext, ".armake") == 0)) {
                snprintf(out, out_size, "%s%c%s", dir, SEPARATOR, ent->d_name);
                closedir(d);
                return 0;
            }
        }
    }
    closedir(d);
#endif
    return -1;
}

/* Read manifest file and parse name and version */
/* Read manifest file and parse name and version */
static int read_manifest_info(const char *manifest_file, char *name_out, size_t name_max, char *ver_out, size_t ver_max) {
    FILE *f = fopen(manifest_file, "rb");
    if (!f) return -1;

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) { fclose(f); return -1; }
    fread(buf, 1, sz, f);
    buf[sz] = '\0';
    fclose(f);

    /* Skip leading whitespace / header until opening '{' */
    char *data = buf;
    while (*data && (*data == ' ' || *data == '\t' || *data == '\n' || *data == '\r')) data++;
    if (strncmp(data, "ALRIGROUP@APPMAKE", 17) == 0) {
        data += 17;
        while (*data && *data != '{') data++;
    }

    ar_app_manifest_t m;
    memset(&m, 0, sizeof(m));
    if (ar_manifest_parse(data, &m) == 0) {
        if (name_out && m.name[0]) {
            strncpy(name_out, m.name, name_max - 1);
            name_out[name_max - 1] = '\0';
        }
        if (ver_out && m.version[0]) {
            strncpy(ver_out, m.version, ver_max - 1);
            ver_out[ver_max - 1] = '\0';
        }
        free(buf);
        return 0;
    }
    free(buf);
    return -1;
}

/* Registry resolution helper */
static int resolve_registry_package(const char *app, char *url_out, size_t url_max, char *sha_out, size_t sha_max, char *ver_out, size_t ver_max) {
    char local_reg[1024];
    snprintf(local_reg, sizeof(local_reg), "%s%cregistry.json", g_ctx.arcore_dir, SEPARATOR);
    if (!file_exists(local_reg)) {
        snprintf(local_reg, sizeof(local_reg), "%s%cregistry.json", g_ctx.root_dir, SEPARATOR);
    }

    const char *reg_path = file_exists(local_reg) ? local_reg : g_ctx.registry_cache;

    if (!file_exists(reg_path)) {
        download_file(DEFAULT_REGISTRY_URL, g_ctx.registry_cache, 0);
        reg_path = g_ctx.registry_cache;
    }

    if (file_exists(reg_path)) {
        FILE *f = fopen(reg_path, "rb");
        if (f) {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fseek(f, 0, SEEK_SET);
            char *json = (char *)malloc(sz + 1);
            if (json) {
                fread(json, 1, sz, f);
                json[sz] = '\0';

                char search_key[128];
                snprintf(search_key, sizeof(search_key), "\"%s\":", app);
                char *pos = strstr(json, search_key);
                if (pos) {
                    char *u = strstr(pos, "\"download_url\":");
                    if (!u) u = strstr(pos, "\"url\":");
                    if (u) {
                        char *q1 = strchr(u + 6, '"');
                        if (q1) {
                            char *q2 = strchr(q1 + 1, '"');
                            if (q2 && (size_t)(q2 - q1) < url_max) {
                                strncpy(url_out, q1 + 1, q2 - q1 - 1);
                                url_out[q2 - q1 - 1] = '\0';
                            }
                        }
                    }

                    char *s = strstr(pos, "\"sha256\":");
                    if (s) {
                        char *q1 = strchr(s + 8, '"');
                        if (q1) {
                            char *q2 = strchr(q1 + 1, '"');
                            if (q2 && (size_t)(q2 - q1) < sha_max) {
                                strncpy(sha_out, q1 + 1, q2 - q1 - 1);
                                sha_out[q2 - q1 - 1] = '\0';
                            }
                        }
                    }

                    char *v = strstr(pos, "\"version\":");
                    if (v) {
                        char *q1 = strchr(v + 9, '"');
                        if (q1) {
                            char *q2 = strchr(q1 + 1, '"');
                            if (q2 && (size_t)(q2 - q1) < ver_max) {
                                strncpy(ver_out, q1 + 1, q2 - q1 - 1);
                                ver_out[q2 - q1 - 1] = '\0';
                            }
                        }
                    }
                    free(json);
                    fclose(f);
                    return (url_out[0] != '\0') ? 0 : -1;
                }
                free(json);
            }
            fclose(f);
        }
    }

    snprintf(url_out, url_max, "https://github.com/alrigroup/%s/releases/latest/download/%s.arapp", app, app);
    strncpy(ver_out, "latest", ver_max - 1);
    sha_out[0] = '\0';
    return 0;
}

/* ========================================================================= */
/* COMMANDS                                                                  */
/* ========================================================================= */

/* arpm install <target> [flags] */
static int cmd_install(int argc, char **argv) {
    if (argc < 3) {
        printf("%sUso:%s arpm install <pacote|url|arquivo.arapp> [--autostart] [--start] [--force]\n", CLR_YELLOW, CLR_RESET);
        return 1;
    }

    const char *target = argv[2];
    int opt_autostart = 0;
    int opt_start = 0;
    int opt_force = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--autostart") == 0 || strcmp(argv[i], "-a") == 0) opt_autostart = 1;
        else if (strcmp(argv[i], "--start") == 0 || strcmp(argv[i], "-s") == 0) opt_start = 1;
        else if (strcmp(argv[i], "--force") == 0 || strcmp(argv[i], "-f") == 0) opt_force = 1;
    }

    printf("\n%s=== ALRIOS Package Manager (arpm) — Instalador ===%s\n", CLR_CYAN, CLR_RESET);

    char app_name[128] = {0};
    char download_url_buf[1024] = {0};
    char expected_sha[128] = {0};
    char version[64] = "1.0.0";
    char local_tmp[1024];

    /* Check if target is a local file */
    if (file_exists(target) && strstr(target, ".arapp")) {
        const char *bn = strrchr(target, SEPARATOR);
        if (!bn) bn = strrchr(target, '/');
        bn = bn ? bn + 1 : target;
        strncpy(app_name, bn, sizeof(app_name) - 1);
        char *dot = strstr(app_name, ".arapp");
        if (dot) *dot = '\0';
        snprintf(local_tmp, sizeof(local_tmp), "%s", target);
    } else {
        /* Remote source */
        if (strncmp(target, "http://", 7) == 0 || strncmp(target, "https://", 8) == 0) {
            strncpy(download_url_buf, target, sizeof(download_url_buf) - 1);
            const char *slash = strrchr(target, '/');
            if (slash) {
                strncpy(app_name, slash + 1, sizeof(app_name) - 1);
                char *dot = strstr(app_name, ".arapp");
                if (dot) *dot = '\0';
            }
        } else if (strncmp(target, "github:", 7) == 0) {
            const char *repo = target + 7;
            const char *at = strchr(repo, '@');
            char tag[64] = "latest";
            char repo_path[256];
            if (at) {
                strncpy(tag, at + 1, sizeof(tag) - 1);
                size_t rlen = at - repo;
                strncpy(repo_path, repo, rlen);
                repo_path[rlen] = '\0';
            } else {
                strncpy(repo_path, repo, sizeof(repo_path) - 1);
            }
            const char *repo_name = strrchr(repo_path, '/');
            repo_name = repo_name ? repo_name + 1 : repo_path;
            strncpy(app_name, repo_name, sizeof(app_name) - 1);

            if (strcmp(tag, "latest") == 0) {
                snprintf(download_url_buf, sizeof(download_url_buf),
                         "https://github.com/%s/releases/latest/download/%s.arapp", repo_path, repo_name);
            } else {
                snprintf(download_url_buf, sizeof(download_url_buf),
                         "https://github.com/%s/releases/download/%s/%s.arapp", repo_path, tag, repo_name);
            }
        } else {
            /* Simple package name (ardcbot) */
            strncpy(app_name, target, sizeof(app_name) - 1);
            if (resolve_registry_package(app_name, download_url_buf, sizeof(download_url_buf),
                                         expected_sha, sizeof(expected_sha), version, sizeof(version)) != 0) {
                printf("%s[ERRO]%s Pacote '%s' nao encontrado no registry.\n", CLR_RED, CLR_RESET, app_name);
                return 1;
            }
        }

        if (app_name[0] == '\0') strncpy(app_name, "app", sizeof(app_name) - 1);

        /* Download to staging */
        snprintf(local_tmp, sizeof(local_tmp), "%s%c%s.arapp.download", g_ctx.staging_dir, SEPARATOR, app_name);
        printf("-> Baixando pacote '%s'...\n", app_name);
        if (download_file(download_url_buf, local_tmp, 1) != 0) {
            printf("%s[ERRO]%s Falha no download do pacote a partir de: %s\n", CLR_RED, CLR_RESET, download_url_buf);
            remove(local_tmp);
            return 1;
        }
    }

    /* Verify arapp magic header */
    printf("-> Verificando integridade e assinatura ALRIOS...\n");
    if (!zip_is_arapp(local_tmp)) {
        printf("%s[ERRO]%s Arquivo invalido! Cabecalho magico ALRIGROUP@APP nao encontrado.\n", CLR_RED, CLR_RESET);
        if (local_tmp != target) remove(local_tmp);
        return 1;
    }

    /* Verify SHA256 if expected */
    char actual_sha[128] = {0};
    calc_file_sha256(local_tmp, actual_sha);
    if (expected_sha[0] && strcasecmp(expected_sha, actual_sha) != 0) {
        printf("%s[ERRO]%s Divergencia de checksum SHA-256!\n  Esperado: %s\n  Obtido:   %s\n",
               CLR_RED, CLR_RESET, expected_sha, actual_sha);
        if (local_tmp != target) remove(local_tmp);
        return 1;
    }

    /* Target destination */
    char final_arapp[1024];
    char final_hash[1024];
    snprintf(final_arapp, sizeof(final_arapp), "%s%c%s.arapp", g_ctx.apps_dir, SEPARATOR, app_name);
    snprintf(final_hash, sizeof(final_hash), "%s%c%s.arapp.hash", g_ctx.apps_dir, SEPARATOR, app_name);

    if (file_exists(final_arapp) && !opt_force) {
        printf("  %s!%s Aplicativo '%s' ja esta instalado. Use --force para reinstalar.\n", CLR_YELLOW, CLR_RESET, app_name);
        if (local_tmp != target) remove(local_tmp);
        return 0;
    }

    /* Move / Install */
    printf("-> Instalando em %s...\n", final_arapp);
#ifdef _WIN32
    remove(final_arapp);
#endif
    if (local_tmp == target) {
        /* Copy local file */
        char cmd[2048];
#ifdef _WIN32
        snprintf(cmd, sizeof(cmd), "copy /Y \"%s\" \"%s\" >nul", local_tmp, final_arapp);
#else
        snprintf(cmd, sizeof(cmd), "cp -f \"%s\" \"%s\"", local_tmp, final_arapp);
#endif
        (void)system(cmd);
    } else {
        rename(local_tmp, final_arapp);
    }

    /* Write short hash file for arcore cache sync */
    FILE *hf = fopen(final_hash, "w");
    if (hf) {
        char short_hash[17];
        strncpy(short_hash, actual_sha, 16);
        short_hash[16] = '\0';
        fprintf(hf, "%s\n", short_hash);
        fclose(hf);
    }

    /* Autostart configuration */
    if (opt_autostart) {
        autostart_add(app_name);
    }

    /* Record in metadata database */
    record_package(app_name, version, target, "binary", opt_autostart, actual_sha);

    /* Signal arcore hot refresh */
    trigger_arcore_refresh();

    if (opt_start || opt_autostart) {
        trigger_arcore_start(app_name);
    }

    printf("\n%s✓ Aplicativo '%s' v%s instalado com sucesso!%s\n\n", CLR_GREEN, app_name, version, CLR_RESET);
    return 0;
}

/* arpm install-src <git_url|github_repo|dir> [flags] */
static int cmd_install_src(int argc, char **argv) {
    if (argc < 3) {
        printf("%sUso:%s arpm install-src <git-url|github:org/repo|dir> [--autostart] [--start]\n", CLR_YELLOW, CLR_RESET);
        return 1;
    }

    const char *src_target = argv[2];
    int opt_autostart = 0;
    int opt_start = 0;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--autostart") == 0 || strcmp(argv[i], "-a") == 0) opt_autostart = 1;
        else if (strcmp(argv[i], "--start") == 0 || strcmp(argv[i], "-s") == 0) opt_start = 1;
    }

    printf("\n%s=== ALRIOS Package Manager (arpm) — Build & Install from Source ===%s\n", CLR_CYAN, CLR_RESET);

    char clone_url[1024] = {0};
    char app_hint[128] = {0};

    if (strncmp(src_target, "github:", 7) == 0) {
        snprintf(clone_url, sizeof(clone_url), "https://github.com/%s.git", src_target + 7);
        const char *slash = strrchr(src_target, '/');
        strncpy(app_hint, slash ? slash + 1 : src_target + 7, sizeof(app_hint) - 1);
    } else if (strncmp(src_target, "http://", 7) == 0 || strncmp(src_target, "https://", 8) == 0) {
        strncpy(clone_url, src_target, sizeof(clone_url) - 1);
        const char *slash = strrchr(src_target, '/');
        if (slash) {
            strncpy(app_hint, slash + 1, sizeof(app_hint) - 1);
            char *dot = strstr(app_hint, ".git");
            if (dot) *dot = '\0';
        }
    } else if (file_exists(src_target)) {
        strncpy(clone_url, "local", sizeof(clone_url) - 1);
    } else {
        snprintf(clone_url, sizeof(clone_url), "https://github.com/alrigroup/%s.git", src_target);
        strncpy(app_hint, src_target, sizeof(app_hint) - 1);
    }

    if (!file_exists(g_ctx.armake_bin)) {
        printf("%s[ERRO]%s Empacotador 'armake' nao encontrado em: %s\n", CLR_RED, CLR_RESET, g_ctx.armake_bin);
        return 1;
    }

    char build_dir[1024];

    if (strcmp(clone_url, "local") == 0) {
        strncpy(build_dir, src_target, sizeof(build_dir) - 1);
    } else {
        snprintf(build_dir, sizeof(build_dir), "%s%carpm_src_%s", g_ctx.staging_dir, SEPARATOR, app_hint[0] ? app_hint : "app");
        remove_recursive(build_dir);

        printf("-> Clonando codigo-fonte remoto:\n   %s%s%s...\n", CLR_CYAN, clone_url, CLR_RESET);
        char cmd[2048];
        snprintf(cmd, sizeof(cmd), "git clone --depth 1 \"%s\" \"%s\"", clone_url, build_dir);
        if (system(cmd) != 0) {
            printf("%s[ERRO]%s Falha ao clonar repositorio git: %s\n", CLR_RED, CLR_RESET, clone_url);
            remove_recursive(build_dir);
            return 1;
        }
    }

    /* Locate .arappmake */
    printf("-> Localizando manifesto .arappmake...\n");
    char manifest_path[1024] = {0};
    if (find_manifest(build_dir, manifest_path, sizeof(manifest_path)) != 0) {
        printf("%s[ERRO]%s Nenhum manifesto (.arappmake/.armake) encontrado no projeto!\n", CLR_RED, CLR_RESET);
        if (strcmp(clone_url, "local") != 0) remove_recursive(build_dir);
        return 1;
    }

    char app_name[128] = {0};
    char version[64] = "1.0.0";
    read_manifest_info(manifest_path, app_name, sizeof(app_name), version, sizeof(version));
    if (app_name[0] == '\0') {
        strncpy(app_name, app_hint[0] ? app_hint : "app", sizeof(app_name) - 1);
    }

    printf("-> Compilando e empacotando aplicativo '%s' (v%s) via armake...\n", app_name, version);
    char cmd_build[2048];
    snprintf(cmd_build, sizeof(cmd_build), "\"%s\" buildapp -s \"%s\" -o apps", g_ctx.armake_bin, build_dir);
    if (system(cmd_build) != 0) {
        printf("%s[ERRO]%s Falha na compilacao/empacotamento via armake!\n", CLR_RED, CLR_RESET);
        if (strcmp(clone_url, "local") != 0) remove_recursive(build_dir);
        return 1;
    }

    /* Cleanup staging source */
    if (strcmp(clone_url, "local") != 0) {
        remove_recursive(build_dir);
    }

    /* Verify output */
    char final_arapp[1024];
    snprintf(final_arapp, sizeof(final_arapp), "%s%c%s.arapp", g_ctx.apps_dir, SEPARATOR, app_name);
    if (!file_exists(final_arapp)) {
        printf("%s[ERRO]%s Pacote esperado nao foi gerado em: %s\n", CLR_RED, CLR_RESET, final_arapp);
        return 1;
    }

    /* Autostart */
    if (opt_autostart) {
        autostart_add(app_name);
    }

    /* Record metadata */
    char sha[128] = {0};
    calc_file_sha256(final_arapp, sha);
    record_package(app_name, version, src_target, "source", opt_autostart, sha);

    /* Signal arcore */
    trigger_arcore_refresh();

    if (opt_start || opt_autostart) {
        trigger_arcore_start(app_name);
    }

    printf("\n%s✓ Pacote '%s' (v%s) compilado do SRC e instalado com sucesso!%s\n\n",
           CLR_GREEN, app_name, version, CLR_RESET);
    return 0;
}

/* arpm uninstall <app> [--purge] */
static int cmd_uninstall(int argc, char **argv) {
    if (argc < 3) {
        printf("%sUso:%s arpm uninstall <app> [--purge]\n", CLR_YELLOW, CLR_RESET);
        return 1;
    }

    const char *app = argv[2];
    int opt_purge = 0;
    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "--purge") == 0) opt_purge = 1;
    }

    printf("\n%s=== ALRIOS Package Manager (arpm) — Desinstalador ===%s\n", CLR_CYAN, CLR_RESET);
    printf("-> Parando servico '%s'...\n", app);
    trigger_arcore_stop(app);

    printf("-> Removendo do autostart.cfg...\n");
    autostart_remove(app);

    char arapp[1024], ahash[1024], pfiles[1024], storage[1024];
    snprintf(arapp, sizeof(arapp), "%s%c%s.arapp", g_ctx.apps_dir, SEPARATOR, app);
    snprintf(ahash, sizeof(ahash), "%s%c%s.arapp.hash", g_ctx.apps_dir, SEPARATOR, app);
    snprintf(pfiles, sizeof(pfiles), "%s%c%s", g_ctx.programfiles_dir, SEPARATOR, app);
    snprintf(storage, sizeof(storage), "%s%c%s", g_ctx.storage_dir, SEPARATOR, app);

    printf("-> Removendo binarios e cache de execucao...\n");
    remove(arapp);
    remove(ahash);
    remove_recursive(pfiles);

    if (opt_purge) {
        printf("  %s[PURGE]%s Removendo dados persistentes em %s...\n", CLR_RED, CLR_RESET, storage);
        remove_recursive(storage);
    } else if (file_exists(storage)) {
        printf("  %s[INFO]%s Dados persistentes preservados em: %s\n  (Use %sarpm uninstall %s --purge%s para apagar dados)\n",
               CLR_BLUE, CLR_RESET, storage, CLR_BOLD, app, CLR_RESET);
    }

    /* Notify arcore */
    trigger_arcore_refresh();

    printf("\n%s✓ Aplicativo '%s' desinstalado com sucesso!%s\n\n", CLR_GREEN, app, CLR_RESET);
    return 0;
}

/* arpm update [<app> | --all] */
static int cmd_update(int argc, char **argv) {
    const char *target = (argc >= 3) ? argv[2] : "--all";

    printf("\n%s=== ALRIOS Package Manager (arpm) — Atualizador ===%s\n", CLR_CYAN, CLR_RESET);

    if (strcmp(target, "--all") == 0 || strcmp(target, "all") == 0) {
        printf("-> Verificando atualizacoes para todos os pacotes em %s...\n", g_ctx.apps_dir);
#ifdef _WIN32
        char pattern[1024];
        snprintf(pattern, sizeof(pattern), "%s\\*.arapp", g_ctx.apps_dir);
        WIN32_FIND_DATAA ffd;
        HANDLE h = FindFirstFileA(pattern, &ffd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                char app[128];
                strncpy(app, ffd.cFileName, sizeof(app) - 1);
                char *dot = strstr(app, ".arapp");
                if (dot) *dot = '\0';
                char *sub_argv[] = {"arpm", "install", app, "--force"};
                cmd_install(4, sub_argv);
            } while (FindNextFileA(h, &ffd));
            FindClose(h);
        }
#else
        DIR *d = opendir(g_ctx.apps_dir);
        if (d) {
            struct dirent *ent;
            while ((ent = readdir(d)) != NULL) {
                const char *ext = strrchr(ent->d_name, '.');
                if (ext && strcmp(ext, ".arapp") == 0) {
                    char app[128];
                    strncpy(app, ent->d_name, sizeof(app) - 1);
                    char *dot = strstr(app, ".arapp");
                    if (dot) *dot = '\0';
                    char *sub_argv[] = {"arpm", "install", app, "--force"};
                    cmd_install(4, sub_argv);
                }
            }
            closedir(d);
        }
#endif
    } else {
        char *sub_argv[] = {"arpm", "install", (char *)target, "--force"};
        return cmd_install(4, sub_argv);
    }
    return 0;
}

/* arpm list */
static int cmd_list(void) {
    printf("\n%s=== Aplicativos Instalados no ALRIOS ===%s\n\n", CLR_CYAN, CLR_RESET);
    printf(" %-22s %-10s %-12s %-12s %s\n", "APLICATIVO", "TAMANHO", "AUTOSTART", "ORIGEM", "PACOTE .ARAPP");
    printf(" --------------------------------------------------------------------------------\n");

    int count = 0;
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*.arapp", g_ctx.apps_dir);
    WIN32_FIND_DATAA ffd;
    HANDLE h = FindFirstFileA(pattern, &ffd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            char app[128];
            strncpy(app, ffd.cFileName, sizeof(app) - 1);
            char *dot = strstr(app, ".arapp");
            if (dot) *dot = '\0';

            char fullpath[1024];
            snprintf(fullpath, sizeof(fullpath), "%s\\%s", g_ctx.apps_dir, ffd.cFileName);
            long sz = file_size(fullpath);
            double mb = sz / 1048576.0;

            int is_auto = autostart_has(app);
            printf(" %-22s %6.2f MB   %-12s %-12s %s\n",
                   app, mb, is_auto ? "Sim [on]" : "Nao", "local/arcore", ffd.cFileName);
            count++;
        } while (FindNextFileA(h, &ffd));
        FindClose(h);
    }
#else
    DIR *d = opendir(g_ctx.apps_dir);
    if (d) {
        struct dirent *ent;
        while ((ent = readdir(d)) != NULL) {
            const char *ext = strrchr(ent->d_name, '.');
            if (ext && strcmp(ext, ".arapp") == 0) {
                char app[128];
                strncpy(app, ent->d_name, sizeof(app) - 1);
                char *dot = strstr(app, ".arapp");
                if (dot) *dot = '\0';

                char fullpath[1024];
                snprintf(fullpath, sizeof(fullpath), "%s/%s", g_ctx.apps_dir, ent->d_name);
                long sz = file_size(fullpath);
                double mb = sz / 1048576.0;

                int is_auto = autostart_has(app);
                printf(" %-22s %6.2f MB   %-12s %-12s %s\n",
                       app, mb, is_auto ? "Sim [on]" : "Nao", "local/arcore", ent->d_name);
                count++;
            }
        }
        closedir(d);
    }
#endif

    printf(" --------------------------------------------------------------------------------\n");
    printf(" Total: %d aplicativo(s) instalado(s).\n\n", count);
    return 0;
}

/* arpm search <query> */
static int cmd_search(const char *query) {
    printf("\n%s=== Catalogo de Aplicativos Disponiveis ===%s\n\n", CLR_CYAN, CLR_RESET);

    char local_reg[1024];
    snprintf(local_reg, sizeof(local_reg), "%s%cregistry.json", g_ctx.arcore_dir, SEPARATOR);
    if (!file_exists(local_reg)) {
        snprintf(local_reg, sizeof(local_reg), "%s%cregistry.json", g_ctx.root_dir, SEPARATOR);
    }
    const char *reg_path = file_exists(local_reg) ? local_reg : g_ctx.registry_cache;

    if (!file_exists(reg_path)) {
        download_file(DEFAULT_REGISTRY_URL, g_ctx.registry_cache, 0);
        reg_path = g_ctx.registry_cache;
    }

    FILE *f = fopen(reg_path, "r");
    if (!f) {
        printf("Nenhum catalogo registry encontrado.\n");
        return 1;
    }

    printf(" %-18s %-10s %-10s %s\n", "NOME", "VERSAO", "RUNTIME", "DESCRICAO");
    printf(" --------------------------------------------------------------------------------\n");

    char line[512];
    char cur_name[64] = {0}, cur_ver[32] = {0}, cur_rt[32] = {0}, cur_desc[256] = {0};
    int in_pkg = 0;

    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "\"name\":")) {
            char *q1 = strchr(line, ':');
            if (q1) {
                char *q2 = strchr(q1, '"');
                if (q2) {
                    char *q3 = strchr(q2 + 1, '"');
                    if (q3) {
                        strncpy(cur_name, q2 + 1, q3 - q2 - 1);
                        cur_name[q3 - q2 - 1] = '\0';
                        in_pkg = 1;
                    }
                }
            }
        } else if (in_pkg && strstr(line, "\"version\":")) {
            char *q1 = strchr(line, ':');
            if (q1) {
                char *q2 = strchr(q1, '"');
                if (q2) {
                    char *q3 = strchr(q2 + 1, '"');
                    if (q3) {
                        strncpy(cur_ver, q2 + 1, q3 - q2 - 1);
                        cur_ver[q3 - q2 - 1] = '\0';
                    }
                }
            }
        } else if (in_pkg && strstr(line, "\"runtime\":")) {
            char *q1 = strchr(line, ':');
            if (q1) {
                char *q2 = strchr(q1, '"');
                if (q2) {
                    char *q3 = strchr(q2 + 1, '"');
                    if (q3) {
                        strncpy(cur_rt, q2 + 1, q3 - q2 - 1);
                        cur_rt[q3 - q2 - 1] = '\0';
                    }
                }
            }
        } else if (in_pkg && strstr(line, "\"description\":")) {
            char *q1 = strchr(line, ':');
            if (q1) {
                char *q2 = strchr(q1, '"');
                if (q2) {
                    char *q3 = strchr(q2 + 1, '"');
                    if (q3) {
                        strncpy(cur_desc, q2 + 1, q3 - q2 - 1);
                        cur_desc[q3 - q2 - 1] = '\0';
                    }
                }
            }
        } else if (in_pkg && strchr(line, '}')) {
            if (!query || query[0] == '\0' || strstr(cur_name, query) || strstr(cur_desc, query)) {
                printf(" %-18s %-10s %-10s %s\n", cur_name, cur_ver, cur_rt, cur_desc);
            }
            cur_name[0] = '\0'; cur_ver[0] = '\0'; cur_rt[0] = '\0'; cur_desc[0] = '\0';
            in_pkg = 0;
        }
    }
    fclose(f);
    printf(" --------------------------------------------------------------------------------\n\n");
    return 0;
}

static void print_usage(void) {
    printf("%sALRIOS Package Manager (arpm) v%s%s\n", CLR_CYAN, ARPM_VERSION, CLR_RESET);
    printf("Gerenciador nativo de pacotes e aplicacoes para o ecossistema ALRIOS.\n\n");
    printf("%sUso:%s\n", CLR_BOLD, CLR_RESET);
    printf("  arpm install <app|url|arquivo.arapp>   Instala pacote .arapp pre-compilado\n");
    printf("  arpm install-src <git|github:org/repo>  Clona, compila via .arappmake e instala\n");
    printf("  arpm update [<app> | --all]            Atualiza pacotes (preserva dados em storage/)\n");
    printf("  arpm uninstall <app> [--purge]         Desinstala app (use --purge para apagar dados)\n");
    printf("  arpm list                              Lista todos os pacotes instalados e status\n");
    printf("  arpm search [termo]                    Pesquisa no catalogo remoto\n");
    printf("  arpm info <app>                        Exibe detalhes de um pacote\n\n");
    printf("%sOpcoes comuns:%s\n", CLR_BOLD, CLR_RESET);
    printf("  --autostart, -a                        Adiciona automaticamente ao autostart.cfg\n");
    printf("  --start, -s                            Inicia o aplicativo imediatamente apos instalar\n");
    printf("  --force, -f                            Forca sobrescrita caso ja esteja instalado\n");
    printf("  --purge                                Remove dados persistentes em storage/ na desinstalacao\n\n");
}

int cmd_arpm(int argc, char **argv) {
    init_paths();

    if (argc < 2) {
        print_usage();
        return 0;
    }

    const char *cmd = argv[1];

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        print_usage();
        return 0;
    }

    if (strcmp(cmd, "install") == 0 || strcmp(cmd, "i") == 0 || strcmp(cmd, "add") == 0)
        return cmd_install(argc, argv);

    if (strcmp(cmd, "install-src") == 0 || strcmp(cmd, "install_src") == 0 || strcmp(cmd, "src") == 0)
        return cmd_install_src(argc, argv);

    if (strcmp(cmd, "uninstall") == 0 || strcmp(cmd, "remove") == 0 || strcmp(cmd, "rm") == 0)
        return cmd_uninstall(argc, argv);

    if (strcmp(cmd, "update") == 0 || strcmp(cmd, "upgrade") == 0 || strcmp(cmd, "u") == 0)
        return cmd_update(argc, argv);

    if (strcmp(cmd, "list") == 0 || strcmp(cmd, "ls") == 0)
        return cmd_list();

    if (strcmp(cmd, "search") == 0)
        return cmd_search(argc > 2 ? argv[2] : "");

    if (strcmp(cmd, "info") == 0 || strcmp(cmd, "show") == 0)
        return cmd_search(argc > 2 ? argv[2] : "");

    printf("%sComando desconhecido: %s%s\n", CLR_RED, cmd, CLR_RESET);
    print_usage();
    return 1;
}
