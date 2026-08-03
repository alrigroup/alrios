/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ar_kernel.h"
#include "ar_svc.h"
#include "aros_hal.h"
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#endif

static int initialized = 0;

static const char *vm_dirs[] = {
    "run",
    "apps",
    "system",
    "etc",
    "storage"
};
#define VM_DIRS_COUNT (sizeof(vm_dirs) / sizeof(vm_dirs[0]))

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

int ar_init(void) {
    if (initialized) return 0;

    for (int i = 0; i < VM_DIRS_COUNT; i++) {
        if (ar_fs_mkdir(vm_dirs[i]) != 0) {
        }
        printf("[kernel] VM dir: /%s\n", vm_dirs[i]);
    }

    ar_health_init(1000);
    initialized = 1;
    return 0;
}

int ar_shutdown(void) {
    if (!initialized) return 0;

    int count = ar_svc_get_count();
    for (int i = 0; i < count; i++) {
        const char *name = ar_svc_get_name(i);
        if (name) ar_svc_stop(name);
    }

    initialized = 0;
    return 0;
}

int ar_run(void) {
    if (!initialized) return -1;

    while (1) {
        ar_sleep_ms(100);
    }

    return 0;
}

int ar_app_storage_dir(const char *app_name, char *out, int out_size) {
    char base[1024];
    get_base_dir(base, sizeof(base));
    snprintf(out, out_size, "%s/storage/%s", base, app_name);
    ar_fs_mkdir(out);
    return 0;
}

int ar_app_repack(const char *app_name) {
    char base[1024];
    get_base_dir(base, sizeof(base));

    char armake_path[1024];
    snprintf(armake_path, sizeof(armake_path), "%s/armake", base);

    char tmp_dir[1024];
    snprintf(tmp_dir, sizeof(tmp_dir), "%s/tmp/%s", base, app_name);

    char output[1024];
    snprintf(output, sizeof(output), "%s/apps/%s/%s.arapp", base, app_name, app_name);

    /* Primeiro copia arquivos do storage para o tmp (atualiza dados) */
    char storage_dir[1024];
    snprintf(storage_dir, sizeof(storage_dir), "%s/storage/%s", base, app_name);
#ifdef _WIN32
    char pattern[1024];
    snprintf(pattern, sizeof(pattern), "%s\\*", storage_dir);
    WIN32_FIND_DATAA ffd;
    HANDLE hFind = FindFirstFileA(pattern, &ffd);
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (strcmp(ffd.cFileName, ".") == 0 || strcmp(ffd.cFileName, "..") == 0) continue;
            char src[1024], dst[1024];
            snprintf(src, sizeof(src), "%s\\%s", storage_dir, ffd.cFileName);
            snprintf(dst, sizeof(dst), "%s\\%s", tmp_dir, ffd.cFileName);
            FILE *in = fopen(src, "rb");
            if (!in) continue;
            FILE *outf = fopen(dst, "wb");
            if (!outf) { fclose(in); continue; }
            unsigned char buf[4096];
            int n;
            while ((n = (int)fread(buf, 1, sizeof(buf), in)) > 0)
                fwrite(buf, 1, n, outf);
            fclose(outf);
            fclose(in);
        } while (FindNextFileA(hFind, &ffd) != 0);
        FindClose(hFind);
    }
#else
    DIR *sd = opendir(storage_dir);
    if (sd) {
        struct dirent *entry;
        while ((entry = readdir(sd)) != NULL) {
            if (entry->d_name[0] == '.') continue;
            char src[1024], dst[1024];
            snprintf(src, sizeof(src), "%s/%s", storage_dir, entry->d_name);
            snprintf(dst, sizeof(dst), "%s/%s", tmp_dir, entry->d_name);
            FILE *in = fopen(src, "rb");
            if (!in) continue;
            FILE *outf = fopen(dst, "wb");
            if (!outf) { fclose(in); continue; }
            unsigned char buf[4096];
            int n;
            while ((n = (int)fread(buf, 1, sizeof(buf), in)) > 0)
                fwrite(buf, 1, n, outf);
            fclose(outf);
            fclose(in);
        }
        closedir(sd);
    }
#endif

    /* Pack: armake pack tmp/<app> apps/<app>/<app>.arapp */
    char *argv[] = { armake_path, "pack", tmp_dir, output, NULL };
    int pid = ar_process_create(armake_path, argv);
    if (pid <= 0) return -1;
    int ret = ar_process_wait(pid);
    if (ret != 0) return -1;

    return 0;
}
