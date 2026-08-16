/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "loader.h"
#include "arapp_parser.h"
#include "ar_path.h"
#include "aros_hal.h"
#include "ar_kernel.h"
#include "zip.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

#define MAX_TEMP_DIRS 4
static char temp_dirs[MAX_TEMP_DIRS][1024];
static int temp_dir_count = 0;

static void *proc_group = NULL;

#define AR_MAX_APPS 64
typedef enum {
    APP_STOPPED,
    APP_RUNNING,
    APP_CRASHED
} loader_app_state_t;

typedef struct {
    char name[AR_APP_NAME_MAX];
    char dir[1024];
    ar_app_manifest_t m;
    int pid;
    loader_app_state_t state;
    int is_native_service;
} loader_app_t;

static loader_app_t apps[AR_MAX_APPS];
static int app_count = 0;
static void *app_mutex = NULL;
static int g_refresh_scan = 0;

static char autostart_apps[AR_MAX_APPS][AR_APP_NAME_MAX];
static int autostart_count = 0;

static void app_lock(void) {
    if (!app_mutex) app_mutex = ar_mutex_create();
    if (app_mutex) ar_mutex_lock(app_mutex);
}

static void app_unlock(void) {
    if (app_mutex) ar_mutex_unlock(app_mutex);
}

static loader_app_t *app_register(const ar_app_manifest_t *m, const char *app_dir) {
    if (!m) return NULL;
    app_lock();
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].name, m->name) == 0) {
            strncpy(apps[i].dir, app_dir, sizeof(apps[i].dir) - 1);
            apps[i].dir[sizeof(apps[i].dir) - 1] = '\0';
            apps[i].m = *m;
            app_unlock();
            return &apps[i];
        }
    }
    if (app_count >= AR_MAX_APPS) { app_unlock(); return NULL; }
    loader_app_t *a = &apps[app_count++];
    memset(a, 0, sizeof(*a));
    strncpy(a->name, m->name, sizeof(a->name) - 1);
    strncpy(a->dir, app_dir, sizeof(a->dir) - 1);
    a->m = *m;
    a->state = APP_STOPPED;
    app_unlock();
    return a;
}

static int is_autostart(const char *name);
static void spawn_survive_check(loader_app_t *a);

loader_app_t *loader_find_app(const char *name) {
    if (!name) return NULL;
    app_lock();
    for (int i = 0; i < app_count; i++) {
        if (strcmp(apps[i].name, name) == 0) {
            loader_app_t *a = &apps[i];
            app_unlock();
            return a;
        }
    }
    app_unlock();
    return NULL;
}

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

static int is_cache_fresh(const char *arapp_path, const char *cache_dir) {
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
    return cache_time.QuadPart >= arapp_time.QuadPart;
#else
    struct stat arapp_stat, cache_stat;
    if (stat(arapp_path, &arapp_stat) != 0) return 0;
    if (stat(cache_dir, &cache_stat) != 0) return 0;
    if (!S_ISDIR(cache_stat.st_mode)) return 0;
    return cache_stat.st_mtime >= arapp_stat.st_mtime;
#endif
}

static int read_file(const char *path, char *buf, int max) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    int len = (int)fread(buf, 1, max - 1, f);
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
    if (!z) { alri_printf("    " RED "x" RST " Failed to open: " DIM "%s" RST "\n", archive); return; }

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

static void load_native(const ar_app_manifest_t *m, const char *app_dir, loader_app_t *a) {
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
            strncpy(alt_path, libpath, sizeof(alt_path) - 1);
            char *dot = strrchr(alt_path, '.');
            if (dot) *dot = '\0';
            if (file_exists(alt_path)) {
                strncpy(libpath, alt_path, sizeof(libpath) - 1);
            }
        } else {
            snprintf(alt_path, sizeof(alt_path), "%s.exe", libpath);
            if (file_exists(alt_path)) {
                strncpy(libpath, alt_path, sizeof(libpath) - 1);
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
        int pid = ar_process_create(libpath, argv);
        if (pid > 0) {
            if (proc_group)
                ar_process_group_add(proc_group, pid);
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
        int (*entry)(void) = (int (*)(void))ar_module_sym(handle, svc->entry);
        if (!entry) {
            alri_printf("    " YLW "!" RST " Symbol '" DIM "%s" RST "' not found in " BLD "%s" RST "\n", svc->entry, m->entry);
            continue;
        }
        ar_svc_register(svc->name, entry);
        alri_printf("    " GRN "✓" RST " Service " CYN "%s" RST " registered\n", svc->name);
    }
}

static void spawn_script(const ar_app_manifest_t *m, const char *app_dir, loader_app_t *a) {
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

#ifdef _WIN32
    char saved_cwd[1024];
    if (!_getcwd(saved_cwd, sizeof(saved_cwd))) saved_cwd[0] = '\0';
    if (_chdir(app_dir) != 0) {
        alri_printf("    " RED "x" RST " chdir(%s) failed\n", app_dir);
        return;
    }
    int pid = ar_process_create(rt_path, argv);
    if (saved_cwd[0]) _chdir(saved_cwd);
#else
    pid_t pid = fork();
    if (pid == 0) {
        if (chdir(app_dir) != 0) _exit(127);
        execvp(rt_path, argv);
        _exit(127);
    }
#endif
    if (pid > 0) {
        if (proc_group)
            ar_process_group_add(proc_group, (int)pid);
        if (a) { a->pid = (int)pid; a->state = APP_RUNNING; }
        alri_printf("    " GRN "✓" RST " " BLD "%s" RST " started (" DIM "pid %d" RST ")\n", m->name, (int)pid);
        spawn_survive_check(a);
    } else {
        if (a) { a->pid = 0; a->state = APP_CRASHED; }
        alri_printf("    " RED "x" RST " Failed to start " BLD "%s" RST "\n", m->name);
    }
}

/* After a successful spawn, confirm the child survived its first moments.
   On POSIX fork() always returns a pid, so a child that dies instantly
   (exec failure, missing shared library, immediate crash) would otherwise be
   reported as a successful start. Windows CreateProcess already fails fast on
   missing/invalid executables, but an app that exits on startup benefits too. */
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

static int has_manifest_magic(const char *path) {
    unsigned char hdr[20];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int n = (int)fread(hdr, 1, 20, f);
    fclose(f);
    for (int i = 0; i < n; i++) {
        if (hdr[i] == ' ' || hdr[i] == '\t' || hdr[i] == '\n' || hdr[i] == '\r')
            continue;
        if (hdr[i] == '{') return 1;
        if (i + 17 <= n && memcmp(hdr + i, "ALRIGROUP@APPMAKE", 17) == 0) return 1;
        if (i + 13 <= n && memcmp(hdr + i, "ALRIGROUP@APP", 13) == 0) return 1;
        break;
    }
    return 0;
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
    /* First pass: only .arappmake files with ALRIGROUP header */
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
    /* Second pass: any file with ALRIGROUP header or JSON with "name" */
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
    /* First pass: only .arappmake files with ALRIGROUP header */
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
    /* Second pass: any file with ALRIGROUP header or JSON with "name" */
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

static void loader_spawn(loader_app_t *a) {
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
            load_native(&a->m, a->dir, a);
            break;
        case AR_RUNTIME_PYTHON3:
        case AR_RUNTIME_NODE:
        case AR_RUNTIME_JAVA:
        case AR_RUNTIME_LUA:
        case AR_RUNTIME_RUBY:
        case AR_RUNTIME_GO:
            spawn_script(&a->m, a->dir, a);
            break;
        default:
            a->state = APP_CRASHED;
            alri_printf(" " RED "?" RST " Unknown runtime\n");
            break;
    }
}

static void process_app(const char *app_dir, int phase) {
    ar_app_manifest_t m;
    if (parse_extracted_manifest(app_dir, &m) != 0) {
        if (phase == 0) {
            alri_printf("    " YLW "!" RST " No valid manifest in " DIM "%s" RST "\n", app_dir);
        }
        return;
    }

    /* Phase 0: only register runtimes */
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

    /* Phase 1: load C services (arws, arproxy) */
    if (phase == 1) {
        if (m.service_count > 0) {
            alri_printf("    " DIM "├─" RST " " BLD "%-20s" RST " (" CYN "native" RST ")", m.name);
            loader_app_t *a = app_register(&m, app_dir);
            if (a) a->is_native_service = 1;
            load_native(&m, app_dir, a);
        }
        return;
    }

    /* Phase 2: spawn standalone app processes (home, detroitwebsite, etc.) */
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

        loader_app_t *a = app_register(&m, app_dir);
        if (!a) { alri_printf(" " RED "x" RST " registry full\n"); return; }
        if (g_refresh_scan) {
            alri_printf(" " DIM "(list updated)" RST "\n");
            return;
        }
        if (!is_autostart(m.name)) {
            alri_printf(" " DIM "(autostart off)" RST "\n");
            return;
        }
        loader_spawn(a);
    }
}

void loader_get_apps_dir(char *buf, int size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, size);
    char *p = strrchr(buf, '\\');
    if (p) *p = '\0';
    strncat_s(buf, size, "\\apps", size - strlen(buf) - 1);
#else
    char link[32] = "/proc/self/exe";
    ssize_t len = readlink(link, buf, size - 1);
    if (len < 0) { strncpy(buf, "apps", size); return; }
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
    strncat(buf, "/apps", size - strlen(buf) - 1);
#endif
}

void loader_get_run_dir(char *buf, int size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, size);
    char *p = strrchr(buf, '\\');
    if (p) *p = '\0';
    strncat_s(buf, size, "\\run", size - strlen(buf) - 1);
#else
    char link[32] = "/proc/self/exe";
    ssize_t len = readlink(link, buf, size - 1);
    if (len < 0) { strncpy(buf, "run", size); return; }
    buf[len] = '\0';
    char *p = strrchr(buf, '/');
    if (p) *p = '\0';
    strncat(buf, "/run", size - strlen(buf) - 1);
#endif
}

void loader_get_base_dir(char *buf, int size) {
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

void loader_get_autostart_path(char *buf, int size) {
    loader_get_base_dir(buf, size);
#ifdef _WIN32
    strncat_s(buf, size, "\\autostart.cfg", size - strlen(buf) - 1);
#else
    strncat(buf, "/autostart.cfg", size - strlen(buf) - 1);
#endif
}

void loader_load_autostart(void) {
    char path[1024];
    loader_get_autostart_path(path, sizeof(path));
    autostart_count = 0;

    char buf[4096];
    if (read_file(path, buf, sizeof(buf)) < 0) return;

    char *line = buf;
    while (line && *line && autostart_count < AR_MAX_APPS) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = '\0';
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p && *p != '#') {
            char *end = p + strlen(p);
            while (end > p && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) end--;
            *end = '\0';
            if (*p) {
                strncpy(autostart_apps[autostart_count], p, AR_APP_NAME_MAX - 1);
                autostart_apps[autostart_count][AR_APP_NAME_MAX - 1] = '\0';
                autostart_count++;
            }
        }
        line = nl ? nl + 1 : NULL;
    }
}

static int is_autostart(const char *name) {
    for (int i = 0; i < autostart_count; i++)
        if (strcmp(autostart_apps[i], name) == 0) return 1;
    return 0;
}

int loader_autostart_add(const char *name) {
    if (!name || !name[0]) return -1;
    if (is_autostart(name)) return 0;
    char path[1024];
    loader_get_autostart_path(path, sizeof(path));
    FILE *f = fopen(path, "a");
    if (!f) return -1;
    fprintf(f, "%s\n", name);
    fclose(f);
    loader_load_autostart();
    return 0;
}

int loader_autostart_del(const char *name) {
    if (!name || !name[0]) return -1;
    char path[1024];
    loader_get_autostart_path(path, sizeof(path));
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
    loader_load_autostart();
    return 0;
}

void loader_overlay_storage(const char *apps_dir, const char *app_name, const char *tmpdir) {
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
            fwrite(buf, 1, n, out);
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
            fwrite(buf, 1, n, out);
        fclose(out);
        fclose(in);
    }
    closedir(sd);
#endif
}

/* Scan a single .arapp file: extract and process */
static void scan_arapp_file(const char *arapp_path, const char *apps_dir,
                             const char *subdir_name, int phase) {
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

    /* Ensure programfiles/ parent exists */
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

    if (!is_cache_fresh(arapp_path, cache_dir)) {
        extract_arapp(arapp_path, cache_dir);
    } else {
        alri_printf("    " DIM "→ cache fresh" RST " %s\n", cache_dir);
    }

    /* Storage overlay: files em storage/<app>/ substituem os extraidos */
    char app_name[128];
    strncpy(app_name, base, sizeof(app_name) - 1);
    app_name[sizeof(app_name) - 1] = '\0';
    char *dot2 = strrchr(app_name, '.');
    if (dot2) *dot2 = '\0';
    process_app(cache_dir, phase);
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

void loader_scan_phase(const char *apps_dir, int phase) {
    if (!apps_dir) return;

    if (!proc_group)
        proc_group = ar_process_group_create();

    if (phase == 2)
        loader_load_autostart();

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
                        scan_arapp_file(sub_full, apps_dir, ffd.cFileName, phase);
                    }
                } while (FindNextFileA(hSub, &subffd) != 0);
                FindClose(hSub);
            }
        } else if (has_arapp_header(full_path)) {
            scan_arapp_file(full_path, apps_dir, NULL, phase);
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
                    scan_arapp_file(sub_full, apps_dir, entry->d_name, phase);
                }
            }
            closedir(sd);
        } else if (has_arapp_header(full_path)) {
            scan_arapp_file(full_path, apps_dir, NULL, phase);
        }
    }
    closedir(d);
#endif
}

void loader_scan(const char *apps_dir) {
    loader_scan_phase(apps_dir, 0);
    loader_scan_phase(apps_dir, 1);
}

void loader_set_temp_dir(const char *base_dir) {
    if (temp_dir_count >= MAX_TEMP_DIRS) return;
#ifdef _WIN32
    snprintf(temp_dirs[temp_dir_count], sizeof(temp_dirs[0]), "%s\\..\\tmp", base_dir);
#else
    snprintf(temp_dirs[temp_dir_count], sizeof(temp_dirs[0]), "%s/../tmp", base_dir);
#endif
    temp_dir_count++;
}

void *loader_get_proc_group(void) {
    return proc_group;
}

void loader_cleanup_temp(void) {
    for (int i = 0; i < temp_dir_count; i++) {
        if (temp_dirs[i][0])
            rm_rf(temp_dirs[i]);
    }
}

static const char *app_state_str(const loader_app_t *a) {
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

int loader_start_app(const char *name) {
    loader_app_t *a = loader_find_app(name);
    if (!a) return -1;
    if (a->is_native_service) {
        if (ar_svc_status(name) == SVC_RUNNING) return 0;
        return (ar_svc_start(name) == 0) ? 0 : -1;
    }
    if (a->state == APP_RUNNING) return 0;
    app_lock();
    a->pid = 0;
    a->state = APP_STOPPED;
    app_unlock();
    loader_spawn(a);
    return (a->state == APP_RUNNING) ? 0 : -1;
}

int loader_stop_app(const char *name) {
    loader_app_t *a = loader_find_app(name);
    if (!a) return -1;
    if (a->is_native_service) {
        if (ar_svc_status(name) != SVC_RUNNING) return 0;
        return (ar_svc_stop(name) == 0) ? 0 : -1;
    }
    if (a->state == APP_STOPPED) return 0;
    if (a->pid > 0) {
        ar_process_kill(a->pid);
        int stopped = 0;
        for (int i = 0; i < 20; i++) {
            if (ar_process_wait_nohang(a->pid) != 0) {
                stopped = 1;
                break;
            }
            ar_sleep_ms(25);
        }
        if (!stopped) {
#ifndef _WIN32
            kill((pid_t)a->pid, SIGKILL);
#endif
            for (int i = 0; i < 10; i++) {
                if (ar_process_wait_nohang(a->pid) != 0) break;
                ar_sleep_ms(10);
            }
        }
    }
    app_lock();
    a->pid = 0;
    a->state = APP_STOPPED;
    app_unlock();
    return 0;
}

int loader_restart_app(const char *name) {
    loader_app_t *a = loader_find_app(name);
    if (!a) return -1;
    if (a->is_native_service)
        return (ar_svc_restart(name) == 0) ? 0 : -1;
    loader_stop_app(name);
    return loader_start_app(name);
}

int loader_list_apps(char *out, int size) {
    if (!out || size <= 0) return -1;
    app_lock();
    int used = 0;
    for (int i = 0; i < app_count; i++) {
        int n = snprintf(out + used, size - used, "%s %s pid=%d %s\n",
                         apps[i].name, app_state_str(&apps[i]), apps[i].pid,
                         apps[i].is_native_service ? "svc" : "app");
        if (n <= 0 || used + n >= size) break;
        used += n;
    }
    app_unlock();
    return used;
}

int loader_status_app(const char *name, char *out, int size) {
    loader_app_t *a = loader_find_app(name);
    if (!a) return -1;
    if (out && size > 0)
        snprintf(out, size, "%s", app_state_str(a));
    return 0;
}

void loader_reap_apps(void) {
    app_lock();
    for (int i = 0; i < app_count; i++) {
        loader_app_t *a = &apps[i];
        if (a->is_native_service) continue;
        if (a->state == APP_RUNNING && a->pid > 0) {
            if (ar_process_wait_nohang(a->pid) != 0) {
                a->state = APP_CRASHED;
                a->pid = 0;
            }
        }
    }
    app_unlock();
}

int loader_power_reload(void) {
    app_lock();
    for (int i = 0; i < app_count; i++) {
        char n[AR_APP_NAME_MAX];
        strncpy(n, apps[i].name, sizeof(n) - 1);
        n[sizeof(n) - 1] = '\0';
        if (apps[i].is_native_service) {
            app_unlock();
            ar_svc_stop(n);
            app_lock();
        } else if (apps[i].state == APP_RUNNING) {
            app_unlock();
            loader_stop_app(n);
            app_lock();
        }
    }
    app_unlock();

    char apps_dir[1024], run_dir[1024];
    loader_get_apps_dir(apps_dir, sizeof(apps_dir));
    loader_get_run_dir(run_dir, sizeof(run_dir));

    alri_printf("  " BLD "Reload" RST " " DIM "re-extracting apps..." RST "\n");
    loader_scan_phase(run_dir, 0);
    loader_scan_phase(apps_dir, 1);

    ar_svc_start_all();

    loader_scan_phase(apps_dir, 2);
    return 0;
}

/* Remove apps from the table whose .arapp no longer exists in apps_dir */
static void loader_prune_missing(const char *apps_dir) {
    app_lock();
    for (int i = 0; i < app_count; ) {
        loader_app_t *a = &apps[i];
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
        app_unlock();
        if (running) loader_stop_app(n);
        app_lock();

        if (i < app_count - 1) apps[i] = apps[app_count - 1];
        memset(&apps[app_count - 1], 0, sizeof(apps[0]));
        app_count--;
    }
    app_unlock();
}

/* Refresh: re-scan apps/ and update the app table WITHOUT stopping/spawning
   anything. Stopped autostart apps stay stopped; running apps keep running. */
int loader_refresh(void) {
    char apps_dir[1024];
    loader_get_apps_dir(apps_dir, sizeof(apps_dir));

    loader_load_autostart();
    alri_printf("  " BLD "Refresh" RST " " DIM "re-scanning apps (list only)..." RST "\n");
    g_refresh_scan = 1;
    loader_scan_phase(apps_dir, 2);
    g_refresh_scan = 0;

    loader_prune_missing(apps_dir);
    return 0;
}
