/*
 * Copyright (c) 2026 ALRIGROUP and its affiliates.
 * Engineered and maintained by ALRI Development.
 *
 * This code is licensed under the ARGLP - ALRI GROUP LICENSE PERMISSIVE
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses
 */

#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic ignored "-Wformat-truncation"
#endif

#include "alrios/supervisor/loader.h"
#include "alrios/supervisor/context.h"
#include "arapp_parser.h"
#include "ar_path.h"
#include "aros_hal.h"
#include "ar_kernel.h"
#include "zip.h"
#include "log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define RST "\033[0m"
#define RED "\033[31m"
#define GRN "\033[32m"
#define YLW "\033[33m"
#define CYN "\033[36m"
#define BLD "\033[1m"
#define DIM "\033[2m"

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <process.h>
#else
    #include <dlfcn.h>
    #include <dirent.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #include <sys/prctl.h>
    #include <signal.h>
#endif

static void rm_rf(const char *path) {
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", path);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
            char full[1024];
            snprintf(full, sizeof(full), "%s\\%s", path, ffd.cFileName);
            if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                rm_rf(full);
            else
                DeleteFileA(full);
        } while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
    }
    RemoveDirectoryA(path);
#else
    DIR *d = opendir(path);
    if (!d) return;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char full[1024];
        snprintf(full, sizeof(full), "%s/%s", path, entry->d_name);
        struct stat st;
        if (stat(full, &st) == 0 && S_ISDIR(st.st_mode))
            rm_rf(full);
        else
            unlink(full);
    }
    closedir(d);
    rmdir(path);
#endif
}

static int is_cache_fresh(const ar_supervisor_context_t *ctx, const char *arapp_path, const char *cache_dir) {
    if (ctx && ctx->force_extract) return 0;
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA arapp_info, cache_info;
    if (!GetFileAttributesExA(arapp_path, GetFileExInfoStandard, &arapp_info)) return 0;
    if (!GetFileAttributesExA(cache_dir, GetFileExInfoStandard, &cache_info)) return 0;
    if (!(cache_info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) return 0;
    ULARGE_INTEGER arapp_time, cache_time;
    arapp_time.LowPart = arapp_info.ftLastWriteTime.dwLowDateTime;
    arapp_time.HighPart = arapp_info.ftLastWriteTime.dwHighDateTime;
    cache_time.LowPart = cache_info.ftLastWriteTime.dwLowDateTime;
    cache_time.HighPart = cache_info.ftLastWriteTime.dwHighDateTime;
    return cache_time.QuadPart > arapp_time.QuadPart;
#else
    struct stat arapp_stat, cache_stat;
    if (stat(arapp_path, &arapp_stat) != 0) return 0;
    if (stat(cache_dir, &cache_stat) != 0) return 0;
    if (!S_ISDIR(cache_stat.st_mode)) return 0;
    return cache_stat.st_mtime > arapp_stat.st_mtime;
#endif
}

static int read_file(const char *path, char *buf, int max) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    int len = (int)fread(buf, 1, (size_t)(max - 1), f);
    fclose(f);
    buf[len] = '\0';
    return len;
}

static int has_arapp_header(const char *path) {
    if (!path) return 0;
    if (zip_is_arapp(path) || zip_is_armake(path)) return 1;
    const char *ext = strrchr(path, '.');
    if (ext && (strcmp(ext, ".arapp") == 0 || strcmp(ext, ".ARAPP") == 0)) return 1;
    return 0;
}

static void extract_arapp(const char *archive, const char *outdir) {
    rm_rf(outdir);
    zip_reader_t *z = zip_reader_open_arapp(archive);
    if (!z) z = zip_reader_open(archive);
    if (!z) {
        alri_printf("    " RED "x" RST " Failed to open: " DIM "%s" RST "\n", archive);
        return;
    }

    int count = zip_reader_count(z);
    alri_printf("    " DIM "→ extracting" RST " %s  (" DIM "%d entries" RST ")\n", archive, count);
    for (int i = 0; i < count; i++) {
        if (zip_reader_extract(z, i, outdir) == 0) {
            zip_entry_t e;
            zip_reader_entry(z, i, &e);
            alri_printf("    " DIM "  %s" RST "\n", e.name);
        }
    }
    zip_reader_close(z);
}

static int is_exe_file(const char *path) {
    if (!path) return 0;
    const char *ext = strrchr(path, '.');
    if (ext) {
        if (strcmp(ext, ".exe") == 0 || strcmp(ext, ".EXE") == 0) return 1;
        if (strcmp(ext, ".arlib") == 0 || strcmp(ext, ".so") == 0 || strcmp(ext, ".dll") == 0 || strcmp(ext, ".dylib") == 0) return 0;
    }
#ifndef _WIN32
    return 1;
#else
    return 0;
#endif
}

static void register_runtime(const ar_app_manifest_t *m, const char *extracted_dir) {
    char entry[AR_ENTRY_MAX] = {0};
    char platform[32];
    ar_platform_detect(platform, sizeof(platform));

    if (ar_manifest_get_platform_entry(m, platform, entry, sizeof(entry)) != 0)
        snprintf(entry, sizeof(entry), "%s", m->entry);

    if (!entry[0]) return;

    char full_path[1024];
    snprintf(full_path, sizeof(full_path), "%s%c%s", extracted_dir,
#ifdef _WIN32
             '\\',
#else
             '/',
#endif
             entry);

    ar_path_register(m->name, full_path);
    alri_printf("    " GRN "✓" RST " Runtime " CYN "%s" RST " v" DIM "%s" RST " registered at " DIM "%s" RST "\n",
           m->name, m->version, full_path);
}

static int file_exists(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && !S_ISDIR(st.st_mode));
#endif
}

static void spawn_survive_check(loader_app_t *a) {
    if (!a || a->state != APP_RUNNING || a->pid <= 0) return;
    for (int i = 0; i < 6; i++) {
        int code = 0;
        if (ar_process_wait_status(a->pid, &code) != 0) {
            int pid = a->pid;
            a->pid = 0;
            a->state = APP_CRASHED;
            if (code < 0)
                alri_printf("    " RED "x" RST " " BLD "%s" RST " died right after start (pid %d, signal %d)\n",
                            a->name, pid, -code);
            else
                alri_printf("    " RED "x" RST " " BLD "%s" RST " died right after start (pid %d, exit code %d)\n",
                            a->name, pid, code);
            return;
        }
        ar_sleep_ms(50);
    }
}

static void load_native(ar_supervisor_context_t *ctx, const ar_app_manifest_t *m, const char *app_dir, loader_app_t *a) {
    char entry[AR_ENTRY_MAX] = {0};
    char platform[32];
    ar_platform_detect(platform, sizeof(platform));

    if (ar_manifest_get_platform_entry(m, platform, entry, sizeof(entry)) != 0 || !entry[0])
        snprintf(entry, sizeof(entry), "%s", m->entry);

    char libpath[1024];
#ifdef _WIN32
    snprintf(libpath, sizeof(libpath), "%s\\%s", app_dir, entry);
#else
    snprintf(libpath, sizeof(libpath), "%s/%s", app_dir, entry);
#endif

    if (!file_exists(libpath)) {
        char alt_path[1024];
        if (is_exe_file(libpath)) {
            snprintf(alt_path, sizeof(alt_path), "%s", libpath);
            char *dot = strrchr(alt_path, '.');
            if (dot) *dot = '\0';
            if (file_exists(alt_path)) {
                snprintf(libpath, sizeof(libpath), "%s", alt_path);
            }
        } else {
            snprintf(alt_path, sizeof(alt_path), "%s.exe", libpath);
            if (file_exists(alt_path)) {
                snprintf(libpath, sizeof(libpath), "%s", alt_path);
            }
        }
    }

    if (is_exe_file(libpath)) {
#ifndef _WIN32
        chmod(libpath, 0755);
#endif
        if (!file_exists(libpath)) {
            if (a) { a->pid = 0; a->state = APP_CRASHED; }
            alri_printf("    " RED "x" RST " Binary not found for " BLD "%s" RST ": " DIM "%s" RST "\n",
                        m->name, libpath);
            return;
        }
        char *argv[2];
        argv[0] = libpath;
        argv[1] = NULL;
        int pid = ar_process_spawn(libpath, argv, NULL, app_dir,
                                   AR_PROCESS_FLAG_CLOSE_EXTRA_FDS |
                                   AR_PROCESS_FLAG_NEW_GROUP);
        if (pid > 0) {
            void *pg = ctx ? ctx->proc_group : NULL;
            if (pg && ar_process_group_add(pg, pid) != 0) {
                (void)ar_process_signal(pid, AR_PROCESS_SIGNAL_KILL, 1);
                (void)ar_process_signal(pid, AR_PROCESS_SIGNAL_KILL, 0);
                (void)ar_process_wait(pid);
                if (a) { a->pid = 0; a->state = APP_CRASHED; }
                alri_printf("    " RED "x" RST " Failed to track " BLD "%s" RST " process\n", m->name);
                return;
            }
            if (a) { a->pid = pid; a->state = APP_RUNNING; }
            alri_printf("    " GRN "✓" RST " " BLD "%s" RST " started (" DIM "pid %d" RST " " DIM "%s" RST ")\n",
                        m->name, pid, libpath);
            spawn_survive_check(a);
        } else {
            if (a) { a->pid = 0; a->state = APP_CRASHED; }
            alri_printf("    " RED "x" RST " Failed to start " BLD "%s" RST " (" DIM "%s" RST ")\n",
                        m->name, libpath);
        }
        return;
    }

    void *handle = ar_module_load(libpath);
    if (!handle) {
        alri_printf("    " RED "x" RST " Failed to load " BLD "%s" RST "\n", m->name);
        return;
    }

    for (int i = 0; i < m->service_count; i++) {
        const ar_service_def_t *svc = &m->services[i];
        int (*service_entry)(void) = NULL;
        *(void **)(&service_entry) = ar_module_sym(handle, svc->entry);
        if (!service_entry) {
            alri_printf("    " YLW "!" RST " Symbol '" DIM "%s" RST "' not found in " BLD "%s" RST "\n", svc->entry, m->entry);
            continue;
        }
        ar_svc_register(svc->name, service_entry);
        alri_printf("    " GRN "✓" RST " Service " CYN "%s" RST " registered\n", svc->name);
    }
}

static void spawn_script(ar_supervisor_context_t *ctx, const ar_app_manifest_t *m, const char *app_dir, loader_app_t *a) {
    char script_path[1024];
#ifdef _WIN32
    snprintf(script_path, sizeof(script_path), "%s\\%s", app_dir, m->entry);
#else
    snprintf(script_path, sizeof(script_path), "%s/%s", app_dir, m->entry);
#endif

    const char *rt_name = "?";
    const char *rt_path = NULL;
    int extra_args = 0;
    switch (m->runtime) {
        case AR_RUNTIME_PYTHON3:
            rt_path = ar_path_find("python3"); rt_name = "python3"; extra_args = 1; break;
        case AR_RUNTIME_NODE:
            rt_path = ar_path_find("node");    rt_name = "node";    break;
        case AR_RUNTIME_JAVA:
            rt_path = ar_path_find("java");    rt_name = "java";    break;
        case AR_RUNTIME_LUA:
            rt_path = ar_path_find("lua");     rt_name = "lua";     break;
        case AR_RUNTIME_RUBY:
            rt_path = ar_path_find("ruby");    rt_name = "ruby";   break;
        case AR_RUNTIME_GO:
            rt_path = ar_path_find("go");      rt_name = "go";     break;
        default:
            return;
    }

    if (!rt_path) {
        rt_path = rt_name;
    }

    char *argv[6];
    argv[0] = (char *)rt_path;
    int ai = 1;
    if (m->runtime == AR_RUNTIME_GO) {
        argv[ai++] = "run";
    } else if (extra_args) {
        argv[ai++] = "-u";
    }
    argv[ai++] = script_path;
    argv[ai] = NULL;

    int pid = ar_process_spawn(rt_path, argv, NULL, app_dir,
                               AR_PROCESS_FLAG_SEARCH_PATH |
                               AR_PROCESS_FLAG_CLOSE_EXTRA_FDS |
                               AR_PROCESS_FLAG_NEW_GROUP);
    if (pid > 0) {
        void *pg = ctx ? ctx->proc_group : NULL;
        if (pg && ar_process_group_add(pg, pid) != 0) {
            (void)ar_process_signal(pid, AR_PROCESS_SIGNAL_KILL, 1);
            (void)ar_process_signal(pid, AR_PROCESS_SIGNAL_KILL, 0);
            (void)ar_process_wait(pid);
            if (a) { a->pid = 0; a->state = APP_CRASHED; }
            alri_printf("    " RED "x" RST " Failed to track " BLD "%s" RST " process\n", m->name);
            return;
        }
        if (a) { a->pid = pid; a->state = APP_RUNNING; }
        alri_printf("    " GRN "✓" RST " " BLD "%s" RST " started (" DIM "pid %d" RST ")\n", m->name, (int)pid);
        spawn_survive_check(a);
    } else {
        if (a) { a->pid = 0; a->state = APP_CRASHED; }
        alri_printf("    " RED "x" RST " Failed to start " BLD "%s" RST "\n", m->name);
    }
}

static int try_read_manifest(const char *path, char *json, int json_size, ar_app_manifest_t *m) {
    int len = read_file(path, json, json_size);
    if (len < 0) return -1;

    char *data = json;
    while (data < json + len && (*data == ' ' || *data == '\t' || *data == '\n' || *data == '\r'))
        data++;
    if (json + len - data >= 17 && memcmp(data, "ALRIGROUP@APPMAKE", 17) == 0) {
        data += 17;
        while (data < json + len && *data != '{') data++;
    } else if (json + len - data >= 13 && memcmp(data, "ALRIGROUP@APP", 13) == 0) {
        data += 13;
        while (data < json + len && *data != '{') data++;
    }

    ar_app_manifest_t tmp;
    if (ar_manifest_parse(data, &tmp) != 0) return -1;
    if (tmp.name[0] == '\0') return -1;
    *m = tmp;
    return 0;
}

static int parse_extracted_manifest(const char *app_dir, ar_app_manifest_t *m) {
    char manifest_path[1024];
    char json[4096];

#ifdef _WIN32
    char pattern[1024];
    WIN32_FIND_DATAA ffd;
    HANDLE hFind;
    snprintf(pattern, sizeof(pattern), "%s\\*", app_dir);
    hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return -1;
    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        const char *ext = strrchr(ffd.cFileName, '.');
        if (!ext || strcmp(ext, ".arappmake") != 0) continue;
        snprintf(manifest_path, sizeof(manifest_path), "%s\\%s", app_dir, ffd.cFileName);
        if (try_read_manifest(manifest_path, json, sizeof(json), m) == 0) {
            FindClose(hFind);
            return 0;
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);

    hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return -1;
    do {
        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
        snprintf(manifest_path, sizeof(manifest_path), "%s\\%s", app_dir, ffd.cFileName);
        if (try_read_manifest(manifest_path, json, sizeof(json), m) == 0) {
            FindClose(hFind);
            return 0;
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR *d = opendir(app_dir);
    if (!d) return -1;
    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".arappmake") != 0) continue;
        snprintf(manifest_path, sizeof(manifest_path), "%s/%s", app_dir, entry->d_name);
        if (try_read_manifest(manifest_path, json, sizeof(json), m) == 0) {
            closedir(d);
            return 0;
        }
    }
    rewinddir(d);
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        snprintf(manifest_path, sizeof(manifest_path), "%s/%s", app_dir, entry->d_name);
        if (try_read_manifest(manifest_path, json, sizeof(json), m) == 0) {
            closedir(d);
            return 0;
        }
    }
    closedir(d);
#endif

    return -1;
}

static void loader_spawn(ar_supervisor_context_t *ctx, loader_app_t *a) {
    char policy_reason[160];
    int policy_result;

    if (!a || a->package_path[0] == '\0') return;
    policy_result = ar_supervisor_policy_evaluate_package(
        ctx, a->package_path, policy_reason, sizeof(policy_reason));
    if (policy_result != ALRIOS_POLICY_OK) {
        a->pid = 0;
        a->state = APP_CRASHED;
        alri_printf("    " RED "x" RST " Policy rejected " BLD "%s" RST
                    " (rc=%d, %s)\n", a->name, policy_result,
                    policy_reason[0] ? policy_reason : "fail-closed");
        return;
    }

    if (a->m.requires_count > 0) {
        int missing = 0;
        for (int i = 0; i < a->m.requires_count; i++) {
            const char *req = a->m.requires[i];
            if (!ar_path_exists(req)) {
                if (!missing) alri_printf("\n");
                alri_printf("    " DIM "  needs:" RST " " RED "%s" RST " " DIM "(missing)" RST "\n", req);
                missing = 1;
            }
        }
        if (missing) {
            alri_printf("    " DIM "  └─" RST " " RED "REJECTED" RST " — runtime(s) not in path\n");
            a->state = APP_CRASHED;
            return;
        }
    }

    switch (a->m.runtime) {
        case AR_RUNTIME_NATIVE:
            load_native(ctx, &a->m, a->dir, a);
            break;
        case AR_RUNTIME_PYTHON3:
        case AR_RUNTIME_NODE:
        case AR_RUNTIME_JAVA:
        case AR_RUNTIME_LUA:
        case AR_RUNTIME_RUBY:
        case AR_RUNTIME_GO:
            spawn_script(ctx, &a->m, a->dir, a);
            break;
        default:
            a->state = APP_CRASHED;
            alri_printf(" " RED "?" RST " Unknown runtime\n");
            break;
    }
}

static int is_autostart(const ar_supervisor_context_t *ctx, const char *name) {
    if (!ctx || !name) return 0;
    for (int i = 0; i < ctx->autostart_count; i++) {
        if (strcmp(ctx->autostart_apps[i], name) == 0) return 1;
    }
    return 0;
}

static int app_priority(const char *name) {
    if (!name) return 10;
    if (strcmp(name, "ardb") == 0) return 1;
    if (strcmp(name, "arauth") == 0) return 2;
    if (strcmp(name, "arapiauth") == 0) return 3;
    if (strcmp(name, "arcdn") == 0) return 4;
    if (strcmp(name, "arenterprise") == 0) return 5;
    if (strcmp(name, "home-web") == 0 || strcmp(name, "home.web") == 0 || strcmp(name, "alrigroup-web") == 0 || strcmp(name, "alrigroup.web") == 0) return 6;
    if (strcmp(name, "detroit-web") == 0 || strcmp(name, "detroit.web") == 0) return 7;
    return 10;
}

static void process_app(ar_supervisor_context_t *ctx,
                        const char *app_dir,
                        const char *package_path,
                        int phase) {
    ar_app_manifest_t m;
    if (parse_extracted_manifest(app_dir, &m) != 0) {
        if (phase == 0) {
            alri_printf("    " YLW "!" RST " No valid manifest in " DIM "%s" RST "\n", app_dir);
        }
        return;
    }

    if (phase == 0) {
        if (m.is_runtime) {
            const char *rt_str = "unknown";
            switch (m.runtime) {
                case AR_RUNTIME_NATIVE:  rt_str = "native";  break;
                case AR_RUNTIME_PYTHON3: rt_str = "python3"; break;
                case AR_RUNTIME_NODE:    rt_str = "node";    break;
                case AR_RUNTIME_JAVA:    rt_str = "java";    break;
                case AR_RUNTIME_LUA:     rt_str = "lua";     break;
                case AR_RUNTIME_RUBY:    rt_str = "ruby";    break;
                case AR_RUNTIME_GO:      rt_str = "go";      break;
                default: break;
            }
            alri_printf("    " DIM "├─" RST " " BLD "%-20s" RST " (" CYN "%s" RST ")", m.name, rt_str);
            register_runtime(&m, app_dir);
        }
        return;
    }

    if (m.is_runtime) return;

    if (phase == 1) {
        if (m.service_count > 0) {
            alri_printf("    " DIM "├─" RST " " BLD "%-20s" RST " (" CYN "native" RST ")", m.name);
            loader_app_t *a = ar_supervisor_register_app(ctx, &m, app_dir, package_path);
            if (a) {
                a->is_native_service = 1;
                loader_spawn(ctx, a);
            }
        }
        return;
    }

    if (phase == 2) {
        if (m.service_count > 0) return;

        const char *rt_str = "unknown";
        switch (m.runtime) {
            case AR_RUNTIME_NATIVE:  rt_str = "native";  break;
            case AR_RUNTIME_PYTHON3: rt_str = "python3"; break;
            case AR_RUNTIME_NODE:    rt_str = "node";    break;
            case AR_RUNTIME_JAVA:    rt_str = "java";    break;
            case AR_RUNTIME_LUA:     rt_str = "lua";     break;
            case AR_RUNTIME_RUBY:    rt_str = "ruby";    break;
            case AR_RUNTIME_GO:      rt_str = "go";      break;
            default: break;
        }
        alri_printf("    " DIM "├─" RST " " BLD "%-20s" RST " (" CYN "%s" RST ")", m.name, rt_str);

        loader_app_t *a = ar_supervisor_register_app(ctx, &m, app_dir, package_path);
        if (!a) { alri_printf(" " RED "x" RST " registry full\n"); return; }
        if (ctx && ctx->refresh_scan) {
            alri_printf(" " DIM "(list updated)" RST "\n");
            return;
        }
        if (!is_autostart(ctx, m.name)) {
            alri_printf(" " DIM "(autostart off)" RST "\n");
            return;
        }
        alri_printf(" " GRN "✓" RST " queued (prio %d)\n", app_priority(m.name));
    }
}

static void scan_arapp_file(ar_supervisor_context_t *ctx,
                            const char *arapp_path,
                            const char *apps_dir,
                            const char *subdir_name,
                            int phase) {
    (void)subdir_name;
    char cache_dir[1024];
    const char *base = strrchr(arapp_path, '/');
#ifdef _WIN32
    if (!base) base = strrchr(arapp_path, '\\');
#endif
    if (!base) base = arapp_path; else base++;
    snprintf(cache_dir, sizeof(cache_dir), "%s/../programfiles/%s",
             apps_dir, base);
    char *dot = strrchr(cache_dir, '.');
    if (dot) *dot = '\0';

    char parent[1024];
    strncpy(parent, cache_dir, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char *sep = strrchr(parent, '/');
#ifdef _WIN32
    if (!sep) sep = strrchr(parent, '\\');
#endif
    if (sep) {
        *sep = '\0';
#ifdef _WIN32
        CreateDirectoryA(parent, NULL);
#else
        mkdir(parent, 0755);
#endif
    }

    if (!is_cache_fresh(ctx, arapp_path, cache_dir)) {
        extract_arapp(arapp_path, cache_dir);
    } else {
        alri_printf("    " DIM "→ cache fresh" RST " %s\n", cache_dir);
    }

    char app_name[128];
    strncpy(app_name, base, sizeof(app_name) - 1);
    app_name[sizeof(app_name) - 1] = '\0';
    char *dot2 = strrchr(app_name, '.');
    if (dot2) *dot2 = '\0';
    process_app(ctx, cache_dir, arapp_path, phase);
}

static int is_dir_path(const char *path) {
#ifdef _WIN32
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
#else
    struct stat st;
    return (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
#endif
}

void ar_supervisor_get_apps_dir(const ar_supervisor_context_t *ctx, char *buf, int size) {
    if (!buf || size <= 0) return;
    if (ctx && ctx->apps_dir[0] != '\0') {
        snprintf(buf, (size_t)size, "%s", ctx->apps_dir);
        return;
    }
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, size);
    char *p = strrchr(buf, '\\');
    if (p) *p = '\0';
    snprintf(buf + strlen(buf), (size_t)(size - (int)strlen(buf)), "\\apps");
#else
    char link[32] = "/proc/self/exe";
    ssize_t len = readlink(link, buf, (size_t)(size - 1));
    if (len < 0) { snprintf(buf, (size_t)size, "apps"); return; }
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
    snprintf(buf + strlen(buf), (size_t)(size - (int)strlen(buf)), "/apps");
#endif
}

void ar_supervisor_get_run_dir(const ar_supervisor_context_t *ctx, char *buf, int size) {
    if (!buf || size <= 0) return;
    if (ctx && ctx->run_dir[0] != '\0') {
        snprintf(buf, (size_t)size, "%s", ctx->run_dir);
        return;
    }
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, size);
    char *p = strrchr(buf, '\\');
    if (p) *p = '\0';
    snprintf(buf + strlen(buf), (size_t)(size - (int)strlen(buf)), "\\run");
#else
    char link[32] = "/proc/self/exe";
    ssize_t len = readlink(link, buf, (size_t)(size - 1));
    if (len < 0) { snprintf(buf, (size_t)size, "run"); return; }
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
    snprintf(buf + strlen(buf), (size_t)(size - (int)strlen(buf)), "/run");
#endif
}

void ar_supervisor_get_base_dir(const ar_supervisor_context_t *ctx, char *buf, int size) {
    if (!buf || size <= 0) return;
    if (ctx && ctx->base_dir[0] != '\0') {
        snprintf(buf, (size_t)size, "%s", ctx->base_dir);
        return;
    }
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, size);
    char *p = strrchr(buf, '\\');
    if (p) *p = '\0';
#else
    char link[32] = "/proc/self/exe";
    ssize_t len = readlink(link, buf, (size_t)(size - 1));
    if (len < 0) { snprintf(buf, (size_t)size, "."); return; }
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
#endif
}

void ar_supervisor_get_autostart_path(const ar_supervisor_context_t *ctx, char *buf, int size) {
    if (!buf || size <= 0) return;
    ar_supervisor_get_base_dir(ctx, buf, size);
#ifdef _WIN32
    snprintf(buf + strlen(buf), (size_t)(size - (int)strlen(buf)), "\\autostart.cfg");
#else
    snprintf(buf + strlen(buf), (size_t)(size - (int)strlen(buf)), "/autostart.cfg");
#endif
}

void ar_supervisor_load_autostart(ar_supervisor_context_t *ctx) {
    if (!ctx) return;
    char path[1024];
    ar_supervisor_get_autostart_path(ctx, path, sizeof(path));
    ctx->autostart_count = 0;

    char buf[4096];
    if (read_file(path, buf, sizeof(buf)) < 0) return;

    char *line = buf;
    while (line && *line && ctx->autostart_count < AR_MAX_APPS) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p && *p != '#') {
            char *end = p + strlen(p);
            while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
            *end = '\0';
            if (*p) {
                snprintf(ctx->autostart_apps[ctx->autostart_count], AR_APP_NAME_MAX, "%s", p);
                ctx->autostart_count++;
            }
        }
        line = nl ? nl + 1 : NULL;
    }
}

void ar_supervisor_spawn_autostart_priority(ar_supervisor_context_t *ctx) {
    if (!ctx) return;
    ar_supervisor_load_autostart(ctx);
    for (int p = 1; p <= 10; p++) {
        for (int i = 0; i < ctx->app_count; i++) {
            if (ctx->apps[i].is_native_service) continue;
            if (app_priority(ctx->apps[i].name) == p) {
                if (is_autostart(ctx, ctx->apps[i].name) && ctx->apps[i].state == APP_STOPPED) {
                    alri_printf("    " DIM "⚡ Booting (" CYN "priority %d" RST "): " BLD "%s" RST "\n",
                                p, ctx->apps[i].name);
                    loader_spawn(ctx, &ctx->apps[i]);
                    if (p <= 3) ar_sleep_ms(150);
                    else ar_sleep_ms(50);
                }
            }
        }
    }
}

int ar_supervisor_autostart_add(ar_supervisor_context_t *ctx, const char *name) {
    if (!ctx || !name || !name[0]) return -1;
    if (is_autostart(ctx, name)) return 0;
    char path[1024];
    ar_supervisor_get_autostart_path(ctx, path, sizeof(path));
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", name);
    fclose(f);
    ar_supervisor_load_autostart(ctx);
    return 0;
}

int ar_supervisor_autostart_del(ar_supervisor_context_t *ctx, const char *name) {
    if (!ctx || !name || !name[0]) return -1;
    char path[1024];
    ar_supervisor_get_autostart_path(ctx, path, sizeof(path));
    char buf[4096];
    if (read_file(path, buf, sizeof(buf)) < 0) return -1;
    FILE *f = fopen(path, "w");
    if (!f) return -1;
    char *line = buf;
    while (line && *line) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        char *end = p + strlen(p);
        while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
        *end = '\0';
        int skip = 0;
        if (*p && *p != '#' && strcmp(p, name) == 0)
            skip = 1;
        if (!skip)
            fprintf(f, "%s%s", line, nl ? "\n" : "");
        line = nl ? nl + 1 : NULL;
    }
    fclose(f);
    ar_supervisor_load_autostart(ctx);
    return 0;
}

void ar_supervisor_overlay_storage(ar_supervisor_context_t *ctx,
                                   const char *apps_dir,
                                   const char *app_name,
                                   const char *tmpdir) {
    (void)ctx;
    if (!apps_dir || !app_name || !tmpdir) return;
    char storage_dir[1024];
    snprintf(storage_dir, sizeof(storage_dir), "%s/../storage/%s", apps_dir, app_name);
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", storage_dir);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) return;
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
        char src[1024], dst[1024];
        snprintf(src, sizeof(src), "%s\\%s", storage_dir, ffd.cFileName);
        snprintf(dst, sizeof(dst), "%s\\%s", tmpdir, ffd.cFileName);
        FILE *in = fopen(src, "rb");
        if (!in) continue;
        FILE *out = fopen(dst, "wb");
        if (!out) { fclose(in); continue; }
        unsigned char buf[4096];
        int n;
        while ((n = (int)fread(buf, 1, sizeof(buf), in)) > 0)
            fwrite(buf, 1, (size_t)n, out);
        fclose(out);
        fclose(in);
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR *sd = opendir(storage_dir);
    if (!sd) return;
    struct dirent *entry;
    while ((entry = readdir(sd)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char src[1024], dst[1024];
        snprintf(src, sizeof(src), "%s/%s", storage_dir, entry->d_name);
        snprintf(dst, sizeof(dst), "%s/%s", tmpdir, entry->d_name);
        FILE *in = fopen(src, "rb");
        if (!in) continue;
        FILE *out = fopen(dst, "wb");
        if (!out) { fclose(in); continue; }
        unsigned char buf[4096];
        int n;
        while ((n = (int)fread(buf, 1, sizeof(buf), in)) > 0)
            fwrite(buf, 1, (size_t)n, out);
        fclose(out);
        fclose(in);
    }
    closedir(sd);
#endif
}

void ar_supervisor_scan_phase(ar_supervisor_context_t *ctx, const char *apps_dir, int phase) {
    if (!ctx || !apps_dir) return;

    if (!ctx->proc_group)
        ctx->proc_group = ar_process_group_create();

    if (phase == 2)
        ar_supervisor_load_autostart(ctx);

    if (phase == 0) {
        alri_printf("    " DIM "Scanning:" RST " %s\n", apps_dir);
    }

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", apps_dir);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind == INVALID_HANDLE_VALUE) {
        if (phase == 0) alri_printf("    " DIM "(no apps found)" RST "\n");
        return;
    }
    do {
        if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s\\%s", apps_dir, ffd.cFileName);

        if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            char subpattern[1024];
            snprintf(subpattern, sizeof(subpattern), "%s\\*", full_path);
            WIN32_FIND_DATAA subffd;
            HANDLE hSub = FindFirstFileA(subpattern, &subffd);
            if (hSub != INVALID_HANDLE_VALUE) {
                do {
                    if (strcmp(subffd.cFileName, ".") == 0 || strcmp(subffd.cFileName, "..") == 0) continue;
                    char sub_full[1024];
                    snprintf(sub_full, sizeof(sub_full), "%s\\%s", full_path, subffd.cFileName);
                    if (!(subffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && has_arapp_header(sub_full)) {
                        scan_arapp_file(ctx, sub_full, apps_dir, ffd.cFileName, phase);
                    }
                } while (FindNextFileA(hSub, &subffd) != 0);
                FindClose(hSub);
            }
        } else if (has_arapp_header(full_path)) {
            scan_arapp_file(ctx, full_path, apps_dir, NULL, phase);
        }
    } while (FindNextFileA(hFind, &ffd) != 0);
    FindClose(hFind);
#else
    DIR *d = opendir(apps_dir);
    if (!d) {
        if (phase == 0) alri_printf("    " RED "x" RST " Cannot open: %s\n", apps_dir);
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(d)) != NULL) {
        if (entry->d_name[0] == '.') continue;
        char full_path[1024];
        snprintf(full_path, sizeof(full_path), "%s/%s", apps_dir, entry->d_name);

        if (is_dir_path(full_path)) {
            char subdir[1024];
            snprintf(subdir, sizeof(subdir), "%s/%s", apps_dir, entry->d_name);
            DIR *sd = opendir(subdir);
            if (!sd) continue;
            struct dirent *sub;
            while ((sub = readdir(sd)) != NULL) {
                if (sub->d_name[0] == '.') continue;
                char sub_full[1024];
                snprintf(sub_full, sizeof(sub_full), "%s/%s", subdir, sub->d_name);
                if (!is_dir_path(sub_full) && has_arapp_header(sub_full)) {
                    scan_arapp_file(ctx, sub_full, apps_dir, entry->d_name, phase);
                }
            }
            closedir(sd);
        } else if (has_arapp_header(full_path)) {
            scan_arapp_file(ctx, full_path, apps_dir, NULL, phase);
        }
    }
    closedir(d);
#endif
}

void ar_supervisor_scan(ar_supervisor_context_t *ctx, const char *apps_dir) {
    ar_supervisor_scan_phase(ctx, apps_dir, 0);
    ar_supervisor_scan_phase(ctx, apps_dir, 1);
}

void ar_supervisor_set_temp_dir(ar_supervisor_context_t *ctx, const char *base_dir) {
    if (!ctx || !base_dir || ctx->temp_dir_count >= AR_SUPERVISOR_MAX_TEMP_DIRS) return;
#ifdef _WIN32
    snprintf(ctx->temp_dirs[ctx->temp_dir_count], sizeof(ctx->temp_dirs[0]), "%s\\..\\tmp", base_dir);
#else
    snprintf(ctx->temp_dirs[ctx->temp_dir_count], sizeof(ctx->temp_dirs[0]), "%s/../tmp", base_dir);
#endif
    ctx->temp_dir_count++;
}

void *ar_supervisor_get_proc_group(const ar_supervisor_context_t *ctx) {
    return ctx ? ctx->proc_group : NULL;
}

void ar_supervisor_cleanup_temp(ar_supervisor_context_t *ctx) {
    if (!ctx) return;
    for (int i = 0; i < ctx->temp_dir_count; i++) {
        if (ctx->temp_dirs[i][0])
            rm_rf(ctx->temp_dirs[i]);
    }
    ctx->temp_dir_count = 0;
}

static const char *app_state_str(const loader_app_t *a) {
    if (!a) return "UNKNOWN";
    if (a->is_native_service) {
        switch (ar_svc_status(a->name)) {
            case SVC_RUNNING: return "RUNNING";
            case SVC_CRASHED: return "CRASHED";
            default: return "STOPPED";
        }
    }
    switch (a->state) {
        case APP_RUNNING: return "RUNNING";
        case APP_CRASHED: return "CRASHED";
        default: return "STOPPED";
    }
}

loader_app_t *ar_supervisor_find_app(ar_supervisor_context_t *ctx, const char *name) {
    if (!ctx || !name) return NULL;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    for (int i = 0; i < ctx->app_count; i++) {
        if (strcmp(ctx->apps[i].name, name) == 0) {
            loader_app_t *a = &ctx->apps[i];
            if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
            return a;
        }
    }
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
    return NULL;
}

loader_app_t *ar_supervisor_register_app(
    ar_supervisor_context_t *ctx,
    const ar_app_manifest_t *m,
    const char *app_dir,
    const char *package_path) {
    if (!ctx || !m || !package_path) return NULL;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    for (int i = 0; i < ctx->app_count; i++) {
        if (strcmp(ctx->apps[i].name, m->name) == 0) {
            strncpy(ctx->apps[i].dir, app_dir ? app_dir : "", sizeof(ctx->apps[i].dir) - 1);
            ctx->apps[i].dir[sizeof(ctx->apps[i].dir) - 1] = '\0';
            strncpy(ctx->apps[i].package_path, package_path,
                    sizeof(ctx->apps[i].package_path) - 1);
            ctx->apps[i].package_path[sizeof(ctx->apps[i].package_path) - 1] = '\0';
            ctx->apps[i].m = *m;
            loader_app_t *existing = &ctx->apps[i];
            if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
            return existing;
        }
    }
    if (ctx->app_count >= AR_MAX_APPS) {
        if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
        return NULL;
    }
    loader_app_t *a = &ctx->apps[ctx->app_count++];
    memset(a, 0, sizeof(*a));
    snprintf(a->name, sizeof(a->name), "%s", m->name);
    snprintf(a->dir, sizeof(a->dir), "%s", app_dir ? app_dir : "");
    snprintf(a->package_path, sizeof(a->package_path), "%s", package_path);
    a->m = *m;
    a->state = APP_STOPPED;
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
    return a;
}

int ar_supervisor_update_app_process(
    ar_supervisor_context_t *ctx,
    const char *name,
    int new_pid,
    const char *new_dir) {
    if (!ctx || !name) return -1;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    for (int i = 0; i < ctx->app_count; i++) {
        if (strcmp(ctx->apps[i].name, name) == 0) {
            ctx->apps[i].pid = new_pid;
            ctx->apps[i].state = APP_RUNNING;
            if (new_dir && new_dir[0]) {
                snprintf(ctx->apps[i].dir, sizeof(ctx->apps[i].dir), "%s", new_dir);
            }
            if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
            return 0;
        }
    }
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
    return -1;
}

int ar_supervisor_start_app(ar_supervisor_context_t *ctx, const char *name) {
    if (!ctx || !name) return -1;
    loader_app_t *a = ar_supervisor_find_app(ctx, name);
    if (!a) return -1;
    if (a->is_native_service) {
        char policy_reason[160];
        int policy_result;

        if (ar_svc_status(name) == SVC_RUNNING) return 0;
        policy_result = ar_supervisor_policy_evaluate_package(
            ctx, a->package_path, policy_reason, sizeof(policy_reason));
        if (policy_result != ALRIOS_POLICY_OK) {
            alri_printf("    " RED "x" RST " Policy rejected " BLD "%s" RST
                        " (rc=%d, %s)\n", a->name, policy_result,
                        policy_reason[0] ? policy_reason : "fail-closed");
            return -1;
        }
        return (ar_svc_start(name) == 0) ? 0 : -1;
    }
    if (a->state == APP_RUNNING) return 0;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    a->pid = 0;
    a->state = APP_STOPPED;
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
    loader_spawn(ctx, a);
    return (a->state == APP_RUNNING) ? 0 : -1;
}

int ar_supervisor_stop_app(ar_supervisor_context_t *ctx, const char *name) {
    if (!ctx || !name) return -1;
    loader_app_t *a = ar_supervisor_find_app(ctx, name);
    if (!a) return -1;
    if (a->is_native_service) {
        if (ar_svc_status(name) != SVC_RUNNING) return 0;
        return (ar_svc_stop(name) == 0) ? 0 : -1;
    }
    if (a->state == APP_STOPPED && a->pid <= 0) return 0;
    if (a->pid > 0) {
        (void)ar_process_signal(a->pid, AR_PROCESS_SIGNAL_TERM, 1);
        (void)ar_process_signal(a->pid, AR_PROCESS_SIGNAL_TERM, 0);
        int stopped = 0;
        for (int i = 0; i < 30; i++) {
            if (ar_process_wait_nohang(a->pid) != 0) {
                stopped = 1;
                break;
            }
            ar_sleep_ms(30);
        }
        if (!stopped) {
            (void)ar_process_signal(a->pid, AR_PROCESS_SIGNAL_KILL, 1);
            (void)ar_process_signal(a->pid, AR_PROCESS_SIGNAL_KILL, 0);
            for (int i = 0; i < 15; i++) {
                if (ar_process_wait_nohang(a->pid) != 0) break;
                ar_sleep_ms(20);
            }
        }
    }
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    a->pid = 0;
    a->state = APP_STOPPED;
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
    return 0;
}

int ar_supervisor_restart_app(ar_supervisor_context_t *ctx, const char *name) {
    if (!ctx || !name) return -1;
    loader_app_t *a = ar_supervisor_find_app(ctx, name);
    if (!a) return -1;
    if (a->is_native_service) {
        char policy_reason[160];

        if (ar_supervisor_policy_evaluate_package(ctx, a->package_path, policy_reason,
                                                   sizeof(policy_reason)) !=
            ALRIOS_POLICY_OK) {
            return -1;
        }
        return (ar_svc_restart(name) == 0) ? 0 : -1;
    }
    ar_supervisor_stop_app(ctx, name);
    ar_sleep_ms(100);
    return ar_supervisor_start_app(ctx, name);
}

int ar_supervisor_list_apps(ar_supervisor_context_t *ctx, char *out, int size) {
    if (!ctx || !out || size <= 0) return -1;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    int used = 0;
    for (int i = 0; i < ctx->app_count; i++) {
        int n = snprintf(out + used, (size_t)(size - used), "%s %s pid=%d %s\n",
                         ctx->apps[i].name, app_state_str(&ctx->apps[i]), ctx->apps[i].pid,
                         ctx->apps[i].is_native_service ? "svc" : "app");
        if (n <= 0 || used + n >= size) break;
        used += n;
    }
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
    return used;
}

int ar_supervisor_status_app(ar_supervisor_context_t *ctx, const char *name, char *out, int size) {
    if (!ctx || !name) return -1;
    loader_app_t *a = ar_supervisor_find_app(ctx, name);
    if (!a) return -1;
    if (out && size > 0)
        snprintf(out, (size_t)size, "%s", app_state_str(a));
    return 0;
}

void ar_supervisor_reap_apps(ar_supervisor_context_t *ctx) {
    if (!ctx) return;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    for (int i = 0; i < ctx->app_count; i++) {
        loader_app_t *a = &ctx->apps[i];
        if (a->is_native_service) continue;
        if (a->state == APP_RUNNING && a->pid > 0) {
            if (ar_process_wait_nohang(a->pid) != 0) {
                a->state = APP_CRASHED;
                a->pid = 0;
            }
        }
    }
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
}

void ar_supervisor_stop_all(ar_supervisor_context_t *ctx) {
    if (!ctx) return;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);

    for (int i = 0; i < ctx->app_count; i++) {
        if (!ctx->apps[i].is_native_service && ctx->apps[i].pid > 0) {
            (void)ar_process_signal(ctx->apps[i].pid, AR_PROCESS_SIGNAL_TERM, 0);
        }
    }

    for (int i = 0; i < ctx->app_count; i++) {
        if (ctx->apps[i].is_native_service) {
            char n[AR_APP_NAME_MAX];
            strncpy(n, ctx->apps[i].name, sizeof(n) - 1);
            n[sizeof(n) - 1] = '\0';
            if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
            ar_svc_stop(n);
            if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
        }
    }

    for (int t = 0; t < 15; t++) {
        int all_gone = 1;
        for (int i = 0; i < ctx->app_count; i++) {
            if (!ctx->apps[i].is_native_service && ctx->apps[i].pid > 0) {
                if (ar_process_wait_nohang(ctx->apps[i].pid) == 0) {
                    all_gone = 0;
                } else {
                    ctx->apps[i].pid = 0;
                    ctx->apps[i].state = APP_STOPPED;
                }
            }
        }
        if (all_gone) break;
        ar_sleep_ms(20);
    }

    for (int i = 0; i < ctx->app_count; i++) {
        if (!ctx->apps[i].is_native_service && ctx->apps[i].pid > 0) {
            (void)ar_process_signal(ctx->apps[i].pid, AR_PROCESS_SIGNAL_KILL, 0);
            (void)ar_process_wait_nohang(ctx->apps[i].pid);
            ctx->apps[i].pid = 0;
            ctx->apps[i].state = APP_STOPPED;
        }
    }
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
}

static void loader_prune_missing(ar_supervisor_context_t *ctx, const char *apps_dir) {
    if (!ctx) return;
    if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);
    for (int i = 0; i < ctx->app_count; ) {
        loader_app_t *a = &ctx->apps[i];
        if (a->is_native_service) { i++; continue; }

        char base[256];
        strncpy(base, a->dir, sizeof(base) - 1);
        base[sizeof(base) - 1] = '\0';
        char *slash = strrchr(base, '/');
#ifdef _WIN32
        if (!slash) slash = strrchr(base, '\\');
#endif
        const char *stem = slash ? slash + 1 : base;
        if (!stem[0]) { i++; continue; }

        char path[1024];
        snprintf(path, sizeof(path), "%s/%s.arapp", apps_dir, stem);
        if (file_exists(path)) { i++; continue; }

        char n[AR_APP_NAME_MAX];
        strncpy(n, a->name, sizeof(n) - 1);
        n[sizeof(n) - 1] = '\0';
        int running = (a->state == APP_RUNNING || a->pid > 0);
        if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
        if (running) ar_supervisor_stop_app(ctx, n);
        if (ctx->app_mutex) ar_mutex_lock(ctx->app_mutex);

        if (i < ctx->app_count - 1) ctx->apps[i] = ctx->apps[ctx->app_count - 1];
        memset(&ctx->apps[ctx->app_count - 1], 0, sizeof(ctx->apps[0]));
        ctx->app_count--;
    }
    if (ctx->app_mutex) ar_mutex_unlock(ctx->app_mutex);
}

int ar_supervisor_power_reload(ar_supervisor_context_t *ctx) {
    if (!ctx) return -1;
    ar_supervisor_stop_all(ctx);

    char apps_dir[1024];
    ar_supervisor_get_apps_dir(ctx, apps_dir, sizeof(apps_dir));

    alri_printf("  " BLD "Reload" RST " " DIM "force re-extracting and reloading user apps..." RST "\n");
    ctx->force_extract = 1;
    ar_supervisor_scan_phase(ctx, apps_dir, 1);

    ar_svc_start_all();

    ar_supervisor_scan_phase(ctx, apps_dir, 2);
    ctx->force_extract = 0;

    ar_supervisor_spawn_autostart_priority(ctx);
    return 0;
}

int ar_supervisor_refresh(ar_supervisor_context_t *ctx) {
    if (!ctx) return -1;
    char apps_dir[1024];
    ar_supervisor_get_apps_dir(ctx, apps_dir, sizeof(apps_dir));

    ar_supervisor_load_autostart(ctx);
    alri_printf("  " BLD "Refresh" RST " " DIM "re-scanning apps (list only)..." RST "\n");
    ctx->refresh_scan = 1;
    ar_supervisor_scan_phase(ctx, apps_dir, 2);
    ctx->refresh_scan = 0;

    loader_prune_missing(ctx, apps_dir);
    return 0;
}
