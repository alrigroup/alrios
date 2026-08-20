/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "zip.h"
#include "arapp_parser.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

#ifdef _WIN32
    #include <windows.h>
    #include <direct.h>
    #include <io.h>
    #define SEPARATOR '\\'
    #define OTHER_SEP '/'
    #define mkdir_p_(p) _mkdir(p)
    #define remove_file_(p) _unlink(p)
    #define remove_dir_(p) _rmdir(p)
#else
    #include <dirent.h>
    #include <unistd.h>
    #include <sys/stat.h>
    #define SEPARATOR '/'
    #define OTHER_SEP '\\'
    #define mkdir_p_(p) mkdir(p, 0755)
    #define remove_file_(p) unlink(p)
    #define remove_dir_(p) rmdir(p)
#endif

static void mkdir_p(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    for (char *p = tmp + 1; *p; p++) {
        if (*p == SEPARATOR) {
            *p = '\0';
            mkdir_p_(tmp);
            *p = SEPARATOR;
        }
    }
    mkdir_p_(tmp);
}

static void print_usage(void) {
    printf("ALRIOS App Packager (multi-OS) — como o GCC, procura o manifesto no diretorio atual\n");
    printf("Busca por: ALRIGROUP@APPMAKE header, *.arappmake, *.armake (legado)\n");
    printf("Usage:\n");
    printf("  armake [--target <os>] [--universal]       Build .arapp from manifest in current dir\n");
    printf("  armake build [dir] [output] [--target <os>|--universal]\n");
    printf("  armake buildapp -s <SRC> -o <OUT>          Build .arapp (-o apps => arcore/apps/)\n");
    printf("  armake pack <dir> <output.arapp>          Pack directory into .arapp\n");
    printf("  armake extract <file.arapp> <dir>          Extract .arapp to directory\n");
    printf("  armake list <file.arapp>                   List contents of .arapp\n");
    printf("  armake header <file>                       Prepend ALRIGROUP@APP header\n");
    printf("  armake snapshot <dir> [-o <file>]          Map tree BEFORE compiling (cleanup base)\n");
    printf("  armake cleanup <dir> -f <file>             Remove build residue AFTER compiling\n");
    printf("\n");
    printf("  --target <os>   (windows|linux)  Default: auto-detect\n");
    printf("  --universal     Inclui files + files_windows + files_linux (para runtimes)\n");
    printf("  Manifest fields: files (comum), files_windows, files_linux\n");
}

static void normalize_path(char *path) {
    for (; *path; path++)
        if (*path == OTHER_SEP) *path = SEPARATOR;
}

/* --- Directory walker (platform-independent interface) --- */
typedef struct {
#ifdef _WIN32
    HANDLE          hFind;
    WIN32_FIND_DATAA ffd;
    int             first;
#else
    DIR            *dir;
    struct dirent  *entry;
#endif
    char            base[1024];
} walker_t;

static int walker_open(walker_t *w, const char *base) {
    memset(w, 0, sizeof(*w));
    strncpy(w->base, base, sizeof(w->base) - 1);
    normalize_path(w->base);
    /* remove trailing separator */
    int len = (int)strlen(w->base);
    if (len > 0 && w->base[len-1] == SEPARATOR)
        w->base[len-1] = '\0';

#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", w->base);
    w->hFind = FindFirstFileA(pattern, &w->ffd);
    if (w->hFind == INVALID_HANDLE_VALUE) return -1;
    w->first = 1;
#else
    w->dir = opendir(base);
    if (!w->dir) return -1;
#endif
    return 0;
}

static int walker_next(walker_t *w, char *relpath, int *is_dir) {
#ifdef _WIN32
    while (1) {
        if (w->first) {
            w->first = 0;
        } else {
            if (!FindNextFileA(w->hFind, &w->ffd)) return 0;
        }
        if (strcmp(w->ffd.cFileName, ".") == 0 || strcmp(w->ffd.cFileName, "..") == 0)
            continue;
        snprintf(relpath, 1024, "%s", w->ffd.cFileName);
        *is_dir = (w->ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 1 : 0;
        return 1;
    }
#else
    while ((w->entry = readdir(w->dir)) != NULL) {
        if (strcmp(w->entry->d_name, ".") == 0 || strcmp(w->entry->d_name, "..") == 0)
            continue;
        snprintf(relpath, 1024, "%s", w->entry->d_name);
        *is_dir = (w->entry->d_type == DT_DIR) ? 1 : 0;
        return 1;
    }
    return 0;
#endif
}

static void walker_close(walker_t *w) {
#ifdef _WIN32
    if (w->hFind != INVALID_HANDLE_VALUE) FindClose(w->hFind);
#else
    if (w->dir) closedir(w->dir);
#endif
}

static int should_skip(const char *name) {
    if (strcmp(name, "CMakeLists.txt") == 0 || strcmp(name, "CMakeCache.txt") == 0)
        return 1;
    const char *ext = strrchr(name, '.');
    if (ext) {
        if (strcmp(ext, ".cmake") == 0 || strcmp(ext, ".pdb") == 0 ||
            strcmp(ext, ".ilk") == 0 || strcmp(ext, ".obj") == 0 ||
            strcmp(ext, ".lib") == 0 || strcmp(ext, ".exp") == 0 ||
            strcmp(ext, ".idb") == 0 || strcmp(ext, ".user") == 0 ||
            strcmp(ext, ".suo") == 0)
            return 1;
    }
    return 0;
}

static int walk_dir(const char *base, const char *rel_prefix,
                    zip_writer_t *z, char *path_buf, int path_size) {
    walker_t w;
    if (walker_open(&w, base) != 0) return 0;

    char name[1024];
    int is_dir;
    while (walker_next(&w, name, &is_dir)) {
        if (is_dir && (strcmp(name, ".git") == 0 || strcmp(name, ".svn") == 0)) continue;
        if (!is_dir && should_skip(name)) continue;
        char fullpath[1024];
        snprintf(fullpath, sizeof(fullpath), "%s%c%s", base, SEPARATOR, name);

        char zipname[1024];
        if (rel_prefix && rel_prefix[0]) {
            snprintf(zipname, sizeof(zipname), "%s/%s", rel_prefix, name);
        } else {
            snprintf(zipname, sizeof(zipname), "%s", name);
        }

        if (is_dir) {
            /* add a directory entry (trailing /) */
            char dirname[1024];
            snprintf(dirname, sizeof(dirname), "%s/", zipname);
            zip_add_entry(z, dirname, ZIP_METHOD_STORED);
            walk_dir(fullpath, zipname, z, path_buf, path_size);
        } else {
            FILE *f = fopen(fullpath, "rb");
            if (!f) continue;
            fseek(f, 0, SEEK_END);
            int size = (int)ftell(f);
            fseek(f, 0, SEEK_SET);

            zip_add_entry(z, zipname, ZIP_METHOD_STORED);
            unsigned char buf[4096];
            while (size > 0) {
                int chunk = (size > 4096) ? 4096 : size;
                fread(buf, 1, chunk, f);
                zip_write(z, buf, chunk);
                size -= chunk;
            }
            fclose(f);
        }
    }
    walker_close(&w);
    return 0;
}

/* --- Snapshot & build-residue cleanup ------------------------------------ */
/* Before running build.command, armake records "what was here" (paths, sizes,
   content hashes). After the build it removes only what was ADDED during the
   build and REPORTS (but keeps) any pre-existing file that was MODIFIED. */

#define SNAP_PATH_MAX 1024
#define RM_CAP 16384

typedef struct {
    char path[SNAP_PATH_MAX];
    int  is_dir;
    long size;
    unsigned hash;
} snap_entry_t;

typedef struct {
    char path[2048];
    int  is_dir;
} rm_entry_t;

static snap_entry_t *g_snap = NULL;
static int g_snap_count = 0;
static char g_appdir[SNAP_PATH_MAX] = {0};
static rm_entry_t g_rm[RM_CAP];
static int g_rm_count = 0;

static long file_size(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fclose(f);
    return s;
}

static unsigned file_hash(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    unsigned h = 2166136261u;
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        for (size_t i = 0; i < n; i++) {
            h ^= buf[i];
            h *= 16777619u;
        }
    }
    fclose(f);
    return h;
}

static int is_abs_path(const char *p) {
#ifdef _WIN32
    return (p[0] && p[1] == ':') || p[0] == '\\' || p[0] == '/';
#else
    return p[0] == '/';
#endif
}

static void set_g_appdir(const char *dir) {
    char cwd[SNAP_PATH_MAX];
#ifdef _WIN32
    if (!_getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
#else
    if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
#endif
    if (is_abs_path(dir)) {
        snprintf(g_appdir, sizeof(g_appdir), "%s", dir);
    } else if (cwd[0]) {
        snprintf(g_appdir, sizeof(g_appdir), "%s%c%s", cwd, SEPARATOR, dir);
    } else {
        snprintf(g_appdir, sizeof(g_appdir), "%s", dir);
    }
}

static int is_build_artifact(const char *name) {
    /* dirs that are never source: always treated as build output */
    static const char *dirs[] = {
        "node_modules", ".next", ".cache", "__pycache__", ".venv", "dist", NULL
    };
    for (int i = 0; dirs[i]; i++)
        if (strcmp(name, dirs[i]) == 0) return 1;
    return 0;
}

static void snap_dir(const char *dir, const char *rel_prefix,
                     snap_entry_t **arr, int *count) {
    walker_t w;
    if (walker_open(&w, dir) != 0) return;

    char name[1024];
    int is_dir;
    while (walker_next(&w, name, &is_dir)) {
        if (is_dir && (strcmp(name, ".git") == 0 || strcmp(name, ".svn") == 0))
            continue;
        if (is_dir && is_build_artifact(name))
            continue;

        char rel[2048];
        if (rel_prefix && rel_prefix[0])
            snprintf(rel, sizeof(rel), "%s/%s", rel_prefix, name);
        else
            snprintf(rel, sizeof(rel), "%s", name);

        snap_entry_t *e = (snap_entry_t *)realloc(*arr, (*count + 1) * sizeof(**arr));
        if (!e) break;
        *arr = e;
        memset(&(*arr)[*count], 0, sizeof((*arr)[*count]));
        snprintf((*arr)[*count].path, sizeof((*arr)[*count].path), "%s", rel);
        (*arr)[*count].is_dir = is_dir;
        int idx = (*count)++;

        if (is_dir) {
            char sub[2048];
            snprintf(sub, sizeof(sub), "%s%c%s", dir, SEPARATOR, name);
            snap_dir(sub, rel, arr, count);
        } else {
            char full[2048];
            snprintf(full, sizeof(full), "%s%c%s", dir, SEPARATOR, name);
            (*arr)[idx].size = file_size(full);
            (*arr)[idx].hash = file_hash(full);
        }
    }
    walker_close(&w);
}

static int in_snapshot(const char *rel, snap_entry_t *out) {
    for (int i = 0; i < g_snap_count; i++) {
        if (strcmp(g_snap[i].path, rel) == 0) {
            *out = g_snap[i];
            return 1;
        }
    }
    return 0;
}

static void rm_tree(const char *path) {
    walker_t w;
    if (walker_open(&w, path) != 0) {
#ifdef _WIN32
        SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
#endif
        remove_file_(path);
        return;
    }
    char name[1024];
    int is_dir;
    while (walker_next(&w, name, &is_dir)) {
        char full[2048];
        snprintf(full, sizeof(full), "%s%c%s", path, SEPARATOR, name);
#ifdef _WIN32
        SetFileAttributesA(full, FILE_ATTRIBUTE_NORMAL);
#endif
        if (is_dir)
            rm_tree(full);
        else
            remove_file_(full);
    }
    walker_close(&w);
#ifdef _WIN32
    SetFileAttributesA(path, FILE_ATTRIBUTE_NORMAL);
#endif
    remove_dir_(path);
}

static void scan_for_added(const char *dir, const char *rel, int *changed) {
    walker_t w;
    if (walker_open(&w, dir) != 0) return;

    char name[1024];
    int is_dir;
    while (walker_next(&w, name, &is_dir)) {
        if (is_dir && (strcmp(name, ".git") == 0 || strcmp(name, ".svn") == 0))
            continue;
        if (strcmp(name, ".armake-snapshot") == 0)
            continue;

        char child_rel[2048];
        if (rel && rel[0])
            snprintf(child_rel, sizeof(child_rel), "%s/%s", rel, name);
        else
            snprintf(child_rel, sizeof(child_rel), "%s", name);

        snap_entry_t before;
        if (in_snapshot(child_rel, &before)) {
            if (is_dir) {
                char child_full[2048];
                snprintf(child_full, sizeof(child_full), "%s%c%s", dir, SEPARATOR, name);
                scan_for_added(child_full, child_rel, changed);
            } else {
                char child_full[2048];
                snprintf(child_full, sizeof(child_full), "%s%c%s", dir, SEPARATOR, name);
                if (before.size != file_size(child_full) || before.hash != file_hash(child_full)) {
                    printf("  [AVISO] Alterado durante o build: %s\n", child_rel);
                    (*changed)++;
                }
            }
        } else if (g_rm_count < RM_CAP) {
            snprintf(g_rm[g_rm_count].path, sizeof(g_rm[g_rm_count].path), "%s", child_rel);
            g_rm[g_rm_count].is_dir = is_dir;
            g_rm_count++;
        }
    }
    walker_close(&w);
}

static void cleanup_and_report(void) {
    if (!g_snap || !g_appdir[0]) return;
    int changed = 0;
    g_rm_count = 0;
    scan_for_added(g_appdir, "", &changed);

    int removed = 0;
    for (int i = 0; i < g_rm_count; i++) {
        char full[2048];
        snprintf(full, sizeof(full), "%s%c%s", g_appdir, SEPARATOR, g_rm[i].path);
        if (g_rm[i].is_dir) {
            rm_tree(full);
            printf("  [LIMPOU] %s/ (gerado no build)\n", g_rm[i].path);
        } else {
            remove_file_(full);
            printf("  [LIMPOU] %s (gerado no build)\n", g_rm[i].path);
        }
        removed++;
    }
    if (removed > 0)
        printf("  [LIMPOU] %d entrada(s) adicionada(s) pelo build\n", removed);
    if (changed > 0)
        printf("  [AVISO] %d arquivo(s) preexistente(s) alterado(s) pelo build (mantidos)\n", changed);
}

static void on_signal(int sig) {
    cleanup_and_report();
    fflush(stdout);
    fflush(stderr);
    signal(sig, SIG_DFL);
    _exit(128 + sig);
}

/* --- Snapshot/cleanup standalone commands (Windows build wrapper) --------- */
/* armake snapshot <dir> [-o <file>]  → map the tree BEFORE compiling
   armake cleanup  <dir> -f <file>    → remove build residue AFTER compiling  */

static int save_snapshot_file(const char *file) {
    FILE *f = fopen(file, "wb");
    if (!f) return -1;
    for (int i = 0; i < g_snap_count; i++)
        fprintf(f, "%s\t%d\t%ld\t%u\n", g_snap[i].path, g_snap[i].is_dir,
                g_snap[i].size, g_snap[i].hash);
    fclose(f);
    return 0;
}

static int load_snapshot_file(const char *file) {
    FILE *f = fopen(file, "rb");
    if (!f) return -1;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        size_t len = strlen(line);
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if (len == 0) continue;

        char *t1 = strchr(line, '\t');
        if (!t1) continue;
        *t1 = '\0';
        char *t2 = strchr(t1 + 1, '\t');
        if (!t2) continue;
        *t2 = '\0';
        char *t3 = strchr(t2 + 1, '\t');
        if (!t3) continue;
        *t3 = '\0';

        snap_entry_t *e = (snap_entry_t *)realloc(g_snap, (g_snap_count + 1) * sizeof(*g_snap));
        if (!e) { fclose(f); return -1; }
        g_snap = e;
        memset(&g_snap[g_snap_count], 0, sizeof(g_snap[g_snap_count]));
        snprintf(g_snap[g_snap_count].path, sizeof(g_snap[g_snap_count].path), "%s", line);
        g_snap[g_snap_count].is_dir = atoi(t1 + 1);
        g_snap[g_snap_count].size = atol(t2 + 1);
        g_snap[g_snap_count].hash = (unsigned)strtoul(t3 + 1, NULL, 10);
        g_snap_count++;
    }
    fclose(f);
    return 0;
}

static int cmd_snapshot(int argc, char **argv) {
    const char *dir = ".";
    const char *out = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc)
            out = argv[++i];
        else if (argv[i][0] == '-') {
            printf("[ERRO] Opcao desconhecida: %s\n", argv[i]);
            return 1;
        } else {
            dir = argv[i];
        }
    }

    g_snap_count = 0;
    g_snap = NULL;
    snap_dir(dir, "", &g_snap, &g_snap_count);
    if (g_snap_count <= 0) {
        printf("[SNAPSHOT] Nada mapeado em: %s\n", dir);
        return 1;
    }

    char buf[2048];
    if (!out) {
        snprintf(buf, sizeof(buf), "%s%c.armake-snapshot", dir, SEPARATOR);
        out = buf;
    }

    if (save_snapshot_file(out) != 0) {
        printf("[ERRO] Nao foi possivel gravar snapshot: %s\n", out);
        return 1;
    }
    printf("[SNAPSHOT] %d entrada(s) mapeadas de %s -> %s\n", g_snap_count, dir, out);
    return 0;
}

static int cmd_cleanup(int argc, char **argv) {
    const char *dir = ".";
    const char *file = NULL;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-f") == 0 && i + 1 < argc)
            file = argv[++i];
        else if (argv[i][0] == '-') {
            printf("[ERRO] Opcao desconhecida: %s\n", argv[i]);
            return 1;
        } else {
            dir = argv[i];
        }
    }
    if (!file) { print_usage(); return 1; }

    g_snap_count = 0;
    g_snap = NULL;
    if (load_snapshot_file(file) != 0) {
        printf("[LIMPOU] Snapshot ausente (%s), nada a limpar\n", file);
        return 0;
    }

    set_g_appdir(dir);
    printf("[LIMPOU] Comparando %s com o snapshot de %d entrada(s)...\n", dir, g_snap_count);
    cleanup_and_report();
    return 0;
}

static int cmd_pack(int argc, char **argv) {
    if (argc < 4) { print_usage(); return 1; }
    const char *input_dir = argv[2];
    const char *output = argv[3];

    zip_writer_t *z = zip_open_arapp(output);
    if (!z) {
        printf("[ERRO] Nao foi possivel criar: %s\n", output);
        return 1;
    }

    char path_buf[1024];
    walk_dir(input_dir, NULL, z, path_buf, sizeof(path_buf));

    zip_close(z);
    printf("[OK] %s criado (%s)\n", output, input_dir);
    return 0;
}

static int read_manifest(const char *path, ar_app_manifest_t *m) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *json = (char *)malloc((size_t)len + 1);
    if (!json) { fclose(f); return -1; }
    fread(json, 1, (size_t)len, f);
    fclose(f);
    json[len] = '\0';

    /* skip leading whitespace and ARM header — search for opening { */
    char *data = json;
    while (data < json + len && (*data == ' ' || *data == '\t' || *data == '\n' || *data == '\r'))
        data++;
    if (json + len - data >= 17 && memcmp(data, "ALRIGROUP@APPMAKE", 17) == 0) {
        data += 17;
        while (data < json + len && *data != '{') data++;
    }

    int ret = ar_manifest_parse(data, m);
    free(json);
    return ret;
}

static int has_appmake_header(const char *path) {
    unsigned char hdr[20];
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    int n = (int)fread(hdr, 1, 20, f);
    fclose(f);
    for (int i = 0; i < n; i++) {
        if (hdr[i] == ' ' || hdr[i] == '\t' || hdr[i] == '\n' || hdr[i] == '\r')
            continue;
        if (i + 17 <= n && memcmp(hdr + i, "ALRIGROUP@APPMAKE", 17) == 0) return 1;
        break;
    }
    return 0;
}

static int has_ext(const char *name, const char *ext) {
    const char *e = strrchr(name, '.');
    return e && strcmp(e, ext) == 0;
}

static int find_manifest(const char *dir, char *out, int out_size) {
    /* if it's already a file, use it directly */
    FILE *test = fopen(dir, "rb");
    if (test) {
        fclose(test);
        if (has_appmake_header(dir)) {
            snprintf(out, out_size, "%s", dir);
            return 0;
        }
    }

    walker_t w;
    if (walker_open(&w, dir) != 0) return -1;

    char name[1024];
    int is_dir;

    /* 1. procurar por ALRIGROUP@APPMAKE header em qualquer arquivo */
    while (walker_next(&w, name, &is_dir)) {
        if (!is_dir) {
            char full[1024];
            snprintf(full, sizeof(full), "%s%c%s", dir, SEPARATOR, name);
            if (has_appmake_header(full)) {
                snprintf(out, out_size, "%s", full);
                walker_close(&w);
                return 0;
            }
        }
    }
    walker_close(&w);

    /* 2. procurar por *.arappmake */
    if (walker_open(&w, dir) != 0) return -1;
    while (walker_next(&w, name, &is_dir)) {
        if (!is_dir && has_ext(name, ".arappmake")) {
            snprintf(out, out_size, "%s%c%s", dir, SEPARATOR, name);
            walker_close(&w);
            return 0;
        }
    }
    walker_close(&w);

    /* 3. fallback: *.armake (legacy) */
    if (walker_open(&w, dir) != 0) return -1;
    while (walker_next(&w, name, &is_dir)) {
        if (!is_dir && has_ext(name, ".armake")) {
            snprintf(out, out_size, "%s%c%s", dir, SEPARATOR, name);
            walker_close(&w);
            return 0;
        }
    }
    walker_close(&w);

    return -1;
}

static int pack_file(zip_writer_t *z, const char *dir, const char *file) {
    char fullpath[1024];
    if (file[0] == '/') {
        /* directory entry: walk recursively */
        snprintf(fullpath, sizeof(fullpath), "%s%c%s", dir, SEPARATOR, file + 1);
        char prefix[1024];
        snprintf(prefix, sizeof(prefix), "%s", file + 1);
        walk_dir(fullpath, prefix, z, fullpath, sizeof(fullpath));
        return 0;
    }
    snprintf(fullpath, sizeof(fullpath), "%s%c%s", dir, SEPARATOR, file);
    FILE *f = fopen(fullpath, "rb");
    if (!f) {
        printf("[AVISO] Arquivo nao encontrado: %s\n", fullpath);
        return -1;
    }
    fseek(f, 0, SEEK_END);
    int size = (int)ftell(f);
    fseek(f, 0, SEEK_SET);

    zip_add_entry(z, file, ZIP_METHOD_STORED);
    unsigned char buf[4096];
    while (size > 0) {
        int chunk = (size > 4096) ? 4096 : size;
        fread(buf, 1, chunk, f);
        zip_write(z, buf, chunk);
        size -= chunk;
    }
    fclose(f);
    return 0;
}

static int pack_file_list(zip_writer_t *z, const char *dir,
                          char files[AR_MAX_FILES][AR_FILE_PATH_MAX],
                          int count) {
    int packed = 0;
    for (int i = 0; i < count; i++)
        if (pack_file(z, dir, files[i]) == 0)
            packed++;
    return packed;
}

static int is_platform(const char *target, const char *name) {
    return strcmp(target, name) == 0;
}

/* ------------------------------------------------------------------ */
/* Build steps engine                                                   */
/* ------------------------------------------------------------------ */

/* Expande variáveis simples em 'tpl' -> 'out':
   $APP_NAME, $APP_DIR, $ARCORE, $STAGING, $ARWN_BUILD                  */
static void expand_vars(char *out, int cap,
                        const char *tpl,
                        const char *app_name,
                        const char *app_dir,
                        const char *arcore_dir,
                        const char *staging,
                        const char *arwn_build) {
    int wi = 0;
    const char *p = tpl;
    while (*p && wi < cap - 1) {
        if (*p != '$') { out[wi++] = *p++; continue; }
        p++;
        const char *sub = NULL;
        if      (strncmp(p, "APP_NAME", 8) == 0) { sub = app_name;   p += 8; }
        else if (strncmp(p, "APP_DIR",  7) == 0) { sub = app_dir;    p += 7; }
        else if (strncmp(p, "ARCORE",   6) == 0) { sub = arcore_dir; p += 6; }
        else if (strncmp(p, "STAGING",  7) == 0) { sub = staging;    p += 7; }
        else if (strncmp(p, "ARWN_BUILD", 10) == 0) { sub = arwn_build; p += 10; }
        else { out[wi++] = '$'; continue; }
        if (sub) {
            int sl = (int)strlen(sub);
            if (wi + sl >= cap) sl = cap - wi - 1;
            memcpy(out + wi, sub, sl);
            wi += sl;
        }
    }
    out[wi] = '\0';
}

/* Resolve o path do binário arwn_build. Usa --arwn-build, senão
   <arcore>/.staging/arwn/arwn_build. Retorna 0 em sucesso.         */
static int resolve_arwn_build(const char *arcore_dir, const char *override,
                              char *out, int cap) {
    if (override && override[0]) {
        snprintf(out, cap, "%s", override);
        return 0;
    }
    if (arcore_dir[0]) {
#ifdef _WIN32
        snprintf(out, cap, "%s\\.staging\\arwn\\arwn_build.exe", arcore_dir);
#else
        snprintf(out, cap, "%s/.staging/arwn/arwn_build", arcore_dir);
#endif
        return 0;
    }
    return -1;
}

/* Sobe dirs a partir de 'start' procurando o diretório 'arcore/'.    */
static int find_arcore_dir(const char *start, char *out, int cap) {
    const char *env = getenv("ARCORE_HOME");
    if (env && env[0]) { snprintf(out, cap, "%s", env); return 0; }
    char cur[1024];
    if (is_abs_path(start)) {
        snprintf(cur, sizeof(cur), "%s", start);
    } else {
        char cwd[1024] = {0};
#ifdef _WIN32
        if (!_getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
#else
        if (!getcwd(cwd, sizeof(cwd))) cwd[0] = '\0';
#endif
        if (cwd[0]) {
            snprintf(cur, sizeof(cur), "%s%c%s", cwd, SEPARATOR, start);
        } else {
            snprintf(cur, sizeof(cur), "%s", start);
        }
    }
    for (int depth = 0; depth < 16; depth++) {
        char candidate[1300];
        snprintf(candidate, sizeof(candidate), "%s%carcore", cur, SEPARATOR);
        /* probe two known sub-paths that exist inside arcore/ */
        char probe[1400];
        snprintf(probe, sizeof(probe), "%s%carmake", candidate, SEPARATOR);
        FILE *pf = fopen(probe, "rb");
        if (!pf) {
            snprintf(probe, sizeof(probe), "%s%c.staging", candidate, SEPARATOR);
            pf = fopen(probe, "rb");
        }
        if (pf) { fclose(pf); snprintf(out, cap, "%s", candidate); return 0; }
        char *last = strrchr(cur, SEPARATOR);
        if (!last || last == cur) break;
        *last = '\0';
    }
    return -1;
}

/* Executa o pipeline de steps do manifesto. Retorna 0 em sucesso.    */
static int run_build_steps_from_manifest(ar_app_manifest_t *m,
                                         const char *app_dir,
                                         const char *staging_override,
                                         const char *arwn_build_override) {
    int has_steps  = (m->build.step_count > 0);
    int has_legacy = (m->build.command[0] != '\0');
    if (!has_steps && !has_legacy) return 0;

    char arcore_dir[1024] = {0};
    find_arcore_dir(app_dir, arcore_dir, sizeof(arcore_dir));

    char arwn_build[1024] = {0};
    resolve_arwn_build(arcore_dir, arwn_build_override, arwn_build, sizeof(arwn_build));

    char staging_tpl[AR_BUILD_STAGING_MAX];
    if (staging_override && staging_override[0]) {
        snprintf(staging_tpl, sizeof(staging_tpl), "%s", staging_override);
    } else if (m->build.staging[0]) {
        snprintf(staging_tpl, sizeof(staging_tpl), "%s", m->build.staging);
    } else {
        snprintf(staging_tpl, sizeof(staging_tpl), "$ARCORE/.staging/$APP_NAME");
    }
    char staging[1024] = {0};
    expand_vars(staging, sizeof(staging), staging_tpl,
                m->name, app_dir, arcore_dir, "", arwn_build);

    char saved_cwd[1024] = {0};
#ifdef _WIN32
    if (!_getcwd(saved_cwd, sizeof(saved_cwd))) saved_cwd[0] = '\0';
    if (staging[0]) {
        char mk[1100];
        snprintf(mk, sizeof(mk), "if not exist \"%s\" mkdir \"%s\"", staging, staging);
        system(mk);
    }
#else
    if (!getcwd(saved_cwd, sizeof(saved_cwd))) saved_cwd[0] = '\0';
    if (staging[0]) {
        char mk[1100];
        snprintf(mk, sizeof(mk), "mkdir -p \"%s\"", staging);
        system(mk);
    }
#endif

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    if (has_steps) {
        for (int i = 0; i < m->build.step_count; i++) {
            ar_build_step_t *step = &m->build.steps[i];
            char cmd_exp[AR_BUILD_STEP_CMD_MAX];
            expand_vars(cmd_exp, sizeof(cmd_exp), step->cmd,
                        m->name, app_dir, arcore_dir, staging, arwn_build);
            char step_cwd[1024];
            if (step->cwd[0]) {
                char cwd_exp[AR_BUILD_STEP_CWD_MAX];
                expand_vars(cwd_exp, sizeof(cwd_exp), step->cwd,
                            m->name, app_dir, arcore_dir, staging, arwn_build);
                snprintf(step_cwd, sizeof(step_cwd), "%s%c%s",
                         app_dir, SEPARATOR, cwd_exp);
            } else {
                snprintf(step_cwd, sizeof(step_cwd), "%s", app_dir);
            }
            printf("[BUILD] step %d/%d (%s): %s\n", i + 1, m->build.step_count,
                   step->name[0] ? step->name : "unnamed", cmd_exp);
#ifdef _WIN32
            if (_chdir(step_cwd) != 0) {
#else
            if (chdir(step_cwd) != 0) {
#endif
                printf("[ERRO] step '%s': chdir(%s) falhou\n", step->name, step_cwd);
                if (saved_cwd[0]) {
#ifdef _WIN32
                    _chdir(saved_cwd);
#else
                    chdir(saved_cwd);
#endif
                }
                return 1;
            }
            int ret = system(cmd_exp);
            if (saved_cwd[0]) {
#ifdef _WIN32
                _chdir(saved_cwd);
#else
                chdir(saved_cwd);
#endif
            }
            if (ret != 0) {
                printf("[ERRO] step '%s' falhou (exit %d)\n", step->name, ret);
                return 1;
            }
            printf("[BUILD] step '%s' OK\n", step->name[0] ? step->name : "unnamed");
        }
    } else {
        printf("[BUILD] Running: %s\n", m->build.command);
#ifdef _WIN32
        if (app_dir[0] && _chdir(app_dir) != 0) {
#else
        if (app_dir[0] && chdir(app_dir) != 0) {
#endif
            printf("[ERRO] chdir(%s) falhou\n", app_dir);
            return 1;
        }
        int ret = system(m->build.command);
        if (saved_cwd[0]) {
#ifdef _WIN32
            _chdir(saved_cwd);
#else
            chdir(saved_cwd);
#endif
        }
        if (ret != 0) { printf("[ERRO] Build command falhou (exit %d)\n", ret); return 1; }
        printf("[BUILD] OK\n");
    }

    /* Cleanup de artefatos de src/ */
    for (int i = 0; i < m->build.cleanup_count; i++) {
        char path_exp[1024];
        expand_vars(path_exp, sizeof(path_exp), m->build.cleanup[i],
                    m->name, app_dir, arcore_dir, staging, arwn_build);
        char full_path[1300];
        if (path_exp[0] == '/'
#ifdef _WIN32
            || (path_exp[0] && path_exp[1] == ':')
#endif
        ) {
            snprintf(full_path, sizeof(full_path), "%s", path_exp);
        } else {
            snprintf(full_path, sizeof(full_path), "%s%c%s",
                     app_dir, SEPARATOR, path_exp);
        }
        normalize_path(full_path);
        printf("[BUILD] Removendo: %s\n", full_path);
        rm_tree(full_path);
    }

    return 0;
}

/* Copia um arquivo de origem para destino. Cria o dir pai.          */
static int copy_file_into(const char *src, const char *dst) {
    FILE *in = fopen(src, "rb");
    if (!in) return -1;
    char dirbuf[1300];
    snprintf(dirbuf, sizeof(dirbuf), "%s", dst);
    char *last = strrchr(dirbuf, SEPARATOR);
    if (last) {
        *last = '\0';
        mkdir_p(dirbuf);
        *last = SEPARATOR;
    }
    FILE *out = fopen(dst, "wb");
    if (!out) { fclose(in); return -1; }
    char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), in)) > 0)
        fwrite(buf, 1, n, out);
    fclose(out);
    fclose(in);
    return 0;
}

/* Procura um arquivo estático do manifesto no app_dir. Busca na raiz
   e em subpastas comuns (web/public, public, static).                 */
static int find_static_src(const char *app_dir, const char *file,
                           char *out, int cap) {
    static const char *subdirs[] = { "", "web/public", "public", "static", NULL };
    for (int i = 0; subdirs[i]; i++) {
        char cand[1300];
        if (subdirs[i][0])
            snprintf(cand, sizeof(cand), "%s%c%s%c%s",
                     app_dir, SEPARATOR, subdirs[i], SEPARATOR, file);
        else
            snprintf(cand, sizeof(cand), "%s%c%s", app_dir, SEPARATOR, file);
        normalize_path(cand);
        if (fopen(cand, "rb") != NULL) {
            snprintf(out, cap, "%s", cand);
            return 0;
        }
    }
    return -1;
}

/* True se o manifesto usa ARWN (build.steps referencia $ARWN_BUILD ou
   empacota config.arwn). Apps ARWN-native têm o entry = cópia de
   arwn_build; apps nativos (ex: cdn compilado por cc) não.          */
static int manifest_is_arwn(ar_app_manifest_t *m) {
    if (!m) return 0;
    for (int i = 0; i < m->build.step_count; i++) {
        if (strstr(m->build.steps[i].cmd, "ARWN_BUILD") != NULL) return 1;
    }
    for (int i = 0; i < m->file_count; i++) {
        if (strcmp(m->files[i], "config.arwn") == 0) return 1;
    }
    return 0;
}

/* Popula o staging com os arquivos estáticos do manifesto que ainda
   não existem lá (copiados do app_dir). O binário de entrada, se
   apontado via --arwn-build e o app for ARWN-native, é SEMPRE copiado
   para o staging (sempre reflete o arwn_build atual, mesmo em
   rebuilds). Apps não-ARWN (ex: cdn compilado por cc) NÃO recebem a
   cópia, preservando o binário produzido pelo build.steps.          */
static void prepare_staging(ar_app_manifest_t *m,
                            const char *app_dir,
                            const char *staging,
                            const char *arwn_build) {
    if (!staging || !staging[0]) return;
    mkdir_p(staging);

    /* 1) Copia o binário de entrada (platform entry) do arwn_build */
    if (arwn_build && arwn_build[0] && manifest_is_arwn(m)) {
        char entry[AR_ENTRY_MAX] = {0};
        char platform[32] = {0};
        ar_platform_detect(platform, sizeof(platform));
        if (ar_manifest_get_platform_entry(m, platform, entry, sizeof(entry)) != 0 && m->entry[0])
            snprintf(entry, sizeof(entry), "%s", m->entry);
        if (entry[0]) {
            char dst[1300];
            snprintf(dst, sizeof(dst), "%s%c%s", staging, SEPARATOR, entry);
            normalize_path(dst);
            if (copy_file_into(arwn_build, dst) == 0)
                printf("[STAGING] binário de entrada: %s\n", dst);
            else
                printf("[AVISO] nao foi possivel copiar %s para %s\n", arwn_build, dst);
        }
    }

    /* 2) Copia arquivos estáticos (files[]) que faltam no staging */
    for (int i = 0; i < m->file_count; i++) {
        char dst[1300];
        snprintf(dst, sizeof(dst), "%s%c%s", staging, SEPARATOR, m->files[i]);
        normalize_path(dst);
        if (fopen(dst, "rb") != NULL) continue;  /* já existe no staging */

        char src[1300];
        if (find_static_src(app_dir, m->files[i], src, sizeof(src)) != 0) {
            printf("[AVISO] arquivo do manifesto nao encontrado: %s\n", m->files[i]);
            continue;
        }
        if (copy_file_into(src, dst) == 0)
            printf("[STAGING] %s\n", dst);
    }
}

static int cmd_build(int argc, char **argv) {

    const char *dir = ".";
    const char *output_arg = NULL;
    const char *staging_override = NULL;
    const char *arwn_build_override = NULL;
    char target[32] = {0};
    int universal = 0;

    /* parse flags and positional args */
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--target") == 0 && i + 1 < argc) {
            strncpy(target, argv[++i], sizeof(target) - 1);
        } else if (strcmp(argv[i], "--universal") == 0) {
            universal = 1;
        } else if (strcmp(argv[i], "--staging") == 0 && i + 1 < argc) {
            staging_override = argv[++i];
        } else if (strcmp(argv[i], "--arwn-build") == 0 && i + 1 < argc) {
            arwn_build_override = argv[++i];
        } else if (argv[i][0] == '-') {
            printf("[ERRO] Opcao desconhecida: %s\n", argv[i]);
            return 1;
        } else if (i == 2) {
            dir = argv[i];
        } else if (i == 3 || (!output_arg && i > 2)) {
            output_arg = argv[i];
        }
    }

    /* detect target platform if not specified */
    if (!target[0] && !universal)
        ar_platform_detect(target, sizeof(target));

    /* find manifest file */
    char manifest_path[1024] = {0};
    if (find_manifest(dir, manifest_path, sizeof(manifest_path)) != 0) {
        printf("[ERRO] Nenhum manifesto (.arappmake/.armake) encontrado em: %s\n", dir);
        return 1;
    }

    if (!manifest_path[0]) {
        printf("[ERRO] Nenhum manifesto encontrado em: %s\n", dir);
        return 1;
    }

    ar_app_manifest_t m;
    if (read_manifest(manifest_path, &m) != 0) {
        printf("[ERRO] Nao foi possivel ler: %s\n", manifest_path);
        return 1;
    }

    /* --- Execute build steps / legacy command if specified --- */
    int has_build = (m.build.step_count > 0 || m.build.command[0]);
    if (has_build) {
        char app_dir_buf[1024] = {0};
        snprintf(app_dir_buf, sizeof(app_dir_buf), "%s", manifest_path);
        char *lsep = strrchr(app_dir_buf, SEPARATOR);
        if (lsep) *lsep = '\0';
        else snprintf(app_dir_buf, sizeof(app_dir_buf), "%s", dir);

        set_g_appdir(app_dir_buf);
        g_snap_count = 0;
        g_snap = NULL;
        snap_dir(app_dir_buf, "", &g_snap, &g_snap_count);

        if (run_build_steps_from_manifest(&m, app_dir_buf, staging_override, arwn_build_override) != 0) {
            cleanup_and_report();
            return 1;
        }
    }

    /* determine output path */
    char output[1024];
    if (output_arg) {
        snprintf(output, sizeof(output), "%s", output_arg);
    } else if (m.bin[0]) {
        snprintf(output, sizeof(output), "%s%c%s", dir, SEPARATOR, m.bin);
    } else {
        snprintf(output, sizeof(output), "%s%c%s.arapp", dir, SEPARATOR, m.name);
    }
    normalize_path(output);

    /* create output directory if needed */
    char *last_sep = strrchr(output, SEPARATOR);
    if (last_sep) {
        *last_sep = '\0';
        mkdir_p(output);
        *last_sep = SEPARATOR;
    }

    zip_writer_t *z = zip_open_arapp(output);
    if (!z) {
        printf("[ERRO] Nao foi possivel criar: %s\n", output);
        cleanup_and_report();
        return 1;
    }

    /* always include the manifest with APPMAKE header */
    {
        FILE *af = fopen(manifest_path, "rb");
        if (af) {
            fseek(af, 0, SEEK_END);
            long alen = ftell(af);
            fseek(af, 0, SEEK_SET);
            unsigned char *arm_content = (unsigned char *)malloc((size_t)alen + 1);
            fread(arm_content, 1, (size_t)alen, af);
            fclose(af);

            unsigned char *json_data = arm_content;
            while ((long)(json_data - arm_content) < alen &&
                   (*json_data == ' ' || *json_data == '\t' || *json_data == '\n' || *json_data == '\r'))
                json_data++;
            int has_header = ((long)(alen - (json_data - arm_content)) >= 17 &&
                              memcmp(json_data, "ALRIGROUP@APPMAKE", 17) == 0);
            if (has_header) {
                json_data += 17;
                while ((long)(json_data - arm_content) < alen && *json_data != '{')
                    json_data++;
            }
            long json_len = (long)(alen - (json_data - arm_content));
            if (json_len < 0) json_len = 0;

            const char *manifest_name = strrchr(manifest_path, SEPARATOR);
            if (!manifest_name) manifest_name = manifest_path; else manifest_name++;

            zip_add_entry(z, manifest_name, ZIP_METHOD_STORED);
            unsigned char arm_hdr[20];
            memset(arm_hdr, 0, 20);
            memcpy(arm_hdr, "ALRIGROUP@APPMAKE", 17);
            ar_write_le16(arm_hdr + 18, 0x0001);
            zip_write(z, arm_hdr, 20);
            zip_write(z, json_data, (int)json_len);

            free(arm_content);
        }
    }

    int total_packed = 0;

    if (has_build && m.file_count > 0) {
        /* Resolve staging e tenta empacotar de lá; fallback = dir do manifesto */
        char app_dir_buf2[1024] = {0};
        snprintf(app_dir_buf2, sizeof(app_dir_buf2), "%s", manifest_path);
        char *lsep2 = strrchr(app_dir_buf2, SEPARATOR);
        if (lsep2) *lsep2 = '\0';
        else snprintf(app_dir_buf2, sizeof(app_dir_buf2), "%s", dir);

        char arcore_buf[1024] = {0};
        find_arcore_dir(app_dir_buf2, arcore_buf, sizeof(arcore_buf));

        char arwn_build_buf[1024] = {0};
        resolve_arwn_build(arcore_buf, arwn_build_override, arwn_build_buf, sizeof(arwn_build_buf));

        char stg_tpl[AR_BUILD_STAGING_MAX];
        if (staging_override && staging_override[0])
            snprintf(stg_tpl, sizeof(stg_tpl), "%s", staging_override);
        else if (m.build.staging[0])
            snprintf(stg_tpl, sizeof(stg_tpl), "%s", m.build.staging);
        else
            snprintf(stg_tpl, sizeof(stg_tpl), "$ARCORE/.staging/$APP_NAME");
        char staging_resolved[1024] = {0};
        expand_vars(staging_resolved, sizeof(staging_resolved), stg_tpl,
                    m.name, app_dir_buf2, arcore_buf, "", arwn_build_buf);

        /* Prepara o staging com o binário de entrada + arquivos estáticos */
        prepare_staging(&m, app_dir_buf2, staging_resolved, arwn_build_buf);

        /* Verifica se o primeiro file do manifesto existe no staging */
        int staging_ok = 0;
        if (staging_resolved[0]) {
            char probe[1300];
            snprintf(probe, sizeof(probe), "%s%c%s",
                     staging_resolved, SEPARATOR, m.files[0]);
            FILE *pf = fopen(probe, "rb");
            if (pf) { fclose(pf); staging_ok = 1; }
        }

        const char *pack_dir = staging_ok ? staging_resolved : app_dir_buf2;
        printf("[INFO] Empacotando de: %s\n", pack_dir);
        total_packed += pack_file_list(z, pack_dir, m.files, m.file_count);
    } else if (has_build) {
        /* build.command legado sem files[]: pack dir inteiro */
        char path_buf[1024];
        walk_dir(dir, NULL, z, path_buf, sizeof(path_buf));
        total_packed = 1;
    } else {
        /* Sem build: pack normal via platform entry + files[] */
        {
            char platform_entry[AR_ENTRY_MAX] = {0};
            int is_win = !universal && is_platform(target, "windows");
            int is_lin = !universal && !is_win;

            if (is_win)
                ar_manifest_get_platform_entry(&m, "windows", platform_entry, sizeof(platform_entry));
            else if (is_lin)
                ar_manifest_get_platform_entry(&m, "linux", platform_entry, sizeof(platform_entry));

            if (!platform_entry[0] && m.entry[0])
                strncpy(platform_entry, m.entry, sizeof(platform_entry) - 1);

            if (platform_entry[0]) {
                if (pack_file(z, dir, platform_entry) == 0)
                    total_packed++;
            }

            if (universal) {
                char win_entry[AR_ENTRY_MAX] = {0}, lin_entry[AR_ENTRY_MAX] = {0};
                ar_manifest_get_platform_entry(&m, "windows", win_entry, sizeof(win_entry));
                ar_manifest_get_platform_entry(&m, "linux", lin_entry, sizeof(lin_entry));
                if (win_entry[0] && pack_file(z, dir, win_entry) == 0) total_packed++;
                if (lin_entry[0] && strcmp(lin_entry, win_entry) != 0 &&
                    pack_file(z, dir, lin_entry) == 0) total_packed++;
            }
        }

        /* pack common files */
        total_packed += pack_file_list(z, dir, m.files, m.file_count);

        /* pack platform-specific files */
        if (universal) {
            total_packed += pack_file_list(z, dir, m.files_windows, m.files_windows_count);
            total_packed += pack_file_list(z, dir, m.files_linux, m.files_linux_count);
            printf("[INFO] Alvo: universal (windows + linux)\n");
        } else {
            int is_win = is_platform(target, "windows");
            if (is_win) {
                total_packed += pack_file_list(z, dir, m.files_windows, m.files_windows_count);
                printf("[INFO] Alvo: windows\n");
            } else {
                total_packed += pack_file_list(z, dir, m.files_linux, m.files_linux_count);
                printf("[INFO] Alvo: %s\n", target);
            }
        }
    }

    zip_close(z);
    cleanup_and_report();
    printf("[OK] %s criado (%d files)\n", output, total_packed);
    return 0;
}

static int cmd_extract(int argc, char **argv) {
    if (argc < 4) { print_usage(); return 1; }
    const char *archive = argv[2];
    const char *outdir = argv[3];

    zip_reader_t *z = zip_reader_open(archive);
    if (!z) {
        printf("[ERRO] Nao foi possivel abrir: %s\n", archive);
        return 1;
    }

    int count = zip_reader_count(z);
    for (int i = 0; i < count; i++) {
        int ret = zip_reader_extract(z, i, outdir);
        if (ret == 0) {
            zip_entry_t e;
            zip_reader_entry(z, i, &e);
            printf("  extraido: %s\n", e.name);
        } else if (ret == -2) {
            printf("  ignorado (compressao nao suportada): entry %d\n", i);
        }
    }

    zip_reader_close(z);
    printf("[OK] Extraido para: %s\n", outdir);
    return 0;
}

static int cmd_list(int argc, char **argv) {
    if (argc < 3) { print_usage(); return 1; }
    const char *archive = argv[2];

    zip_reader_t *z = zip_reader_open(archive);
    if (!z) {
        printf("[ERRO] Nao foi possivel abrir: %s\n", archive);
        return 1;
    }

    int count = zip_reader_count(z);
    printf("Arquivo: %s (%d entries)\n", archive, count);
    printf("----------------------------------------\n");
    for (int i = 0; i < count; i++) {
        zip_entry_t e;
        if (zip_reader_entry(z, i, &e) == 0) {
            int is_dir = (e.name[strlen(e.name)-1] == '/');
            printf("  %c  %s\n", is_dir ? 'D' : ' ', e.name);
        }
    }
    printf("----------------------------------------\n");

    zip_reader_close(z);
    return 0;
}

static int cmd_header(int argc, char **argv) {
    if (argc < 3) { print_usage(); return 1; }
    const char *path = argv[2];
    if (ar_write_header_file(path) != 0) {
        printf("[ERRO] Nao foi possivel escrever header em: %s\n", path);
        return 1;
    }
    printf("[OK] Header ALRIGROUP@APP gravado em: %s\n", path);
    return 0;
}

/* Directory containing the armake executable (base do arcore em producao) */
static void get_exe_dir(char *buf, int size) {
#ifdef _WIN32
    GetModuleFileNameA(NULL, buf, size);
#else
    ssize_t n = readlink("/proc/self/exe", buf, size - 1);
    if (n > 0) {
        buf[n] = '\0';
    } else {
        buf[0] = '\0';
        return;
    }
#endif
    char *sep = strrchr(buf, SEPARATOR);
    if (sep) *sep = '\0';
}

/* armake buildapp -s <SRC> -o <OUT>
   -o apps            => <exedir>/apps/<name>.arapp
   -o <dir>           => <dir>/<name>.arapp
   -o <file.arapp>    => <file.arapp> direto */
static int cmd_buildapp(int argc, char **argv) {
    const char *src = NULL;
    const char *out = NULL;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "-s") == 0 && i + 1 < argc) {
            src = argv[++i];
        } else if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out = argv[++i];
        } else if (argv[i][0] == '-') {
            printf("[ERRO] Opcao desconhecida: %s\n", argv[i]);
            return 1;
        } else {
            printf("[ERRO] buildapp nao aceita argumentos posicionais: %s\n", argv[i]);
            return 1;
        }
    }

    if (!src) { printf("[ERRO] buildapp requer -s <SRC>\n"); return 1; }
    if (!out) { printf("[ERRO] buildapp requer -o <OUT>\n"); return 1; }

    char manifest_path[1024] = {0};
    if (find_manifest(src, manifest_path, sizeof(manifest_path)) != 0) {
        printf("[ERRO] Nenhum manifesto (.arappmake/.armake) encontrado em: %s\n", src);
        return 1;
    }

    ar_app_manifest_t m;
    if (read_manifest(manifest_path, &m) != 0) {
        printf("[ERRO] Nao foi possivel ler: %s\n", manifest_path);
        return 1;
    }

    char final_out[1024];
    if (strcmp(out, "apps") == 0) {
        char exedir[1024];
        get_exe_dir(exedir, sizeof(exedir));
        snprintf(final_out, sizeof(final_out), "%s%capps%c%s.arapp",
                 exedir, SEPARATOR, SEPARATOR, m.name);
    } else {
        const char *ext = strrchr(out, '.');
        if (ext && strcmp(ext, ".arapp") == 0) {
            snprintf(final_out, sizeof(final_out), "%s", out);
        } else {
            snprintf(final_out, sizeof(final_out), "%s%c%s.arapp",
                     out, SEPARATOR, m.name);
        }
    }
    normalize_path(final_out);

    char parent[1024];
    strncpy(parent, final_out, sizeof(parent) - 1);
    parent[sizeof(parent) - 1] = '\0';
    char *last_sep = strrchr(parent, SEPARATOR);
    if (last_sep) {
        *last_sep = '\0';
        mkdir_p(parent);
        *last_sep = SEPARATOR;
    }

    printf("[BUILDAPP] %s -> %s\n", src, final_out);
    char *new_argv[5] = { argv[0], "build", (char *)src, final_out, NULL };
    return cmd_build(4, new_argv);
}

int main(int argc, char **argv) {
    /* default: "build" when called with --target or no args */
    if (argc < 2) {
        char *fake[] = { argv[0], "build", NULL };
        return cmd_build(2, fake);
    }
    if (strcmp(argv[1], "--target") == 0) {
        /* armake --target windows  →  build with target, current dir */
        /* rebuild argv: [prog, "build", "--target", <os>] */
        char *new_argv[5] = { argv[0], "build", "--target", argc > 2 ? argv[2] : "windows", NULL };
        int new_argc = (argc > 2 && argv[2][0] != '-') ? 4 : 3;
        return cmd_build(new_argc, new_argv);
    }

    if (strcmp(argv[1], "build") == 0)
        return cmd_build(argc, argv);
    else if (strcmp(argv[1], "buildapp") == 0)
        return cmd_buildapp(argc, argv);
    else if (strcmp(argv[1], "snapshot") == 0)
        return cmd_snapshot(argc, argv);
    else if (strcmp(argv[1], "cleanup") == 0)
        return cmd_cleanup(argc, argv);
    else if (strcmp(argv[1], "pack") == 0)
        return cmd_pack(argc, argv);
    else if (strcmp(argv[1], "extract") == 0)
        return cmd_extract(argc, argv);
    else if (strcmp(argv[1], "list") == 0)
        return cmd_list(argc, argv);
    else if (strcmp(argv[1], "header") == 0)
        return cmd_header(argc, argv);
    else
        print_usage();

    return 0;
}
