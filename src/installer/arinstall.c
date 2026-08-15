/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "runtimes.h"
#include "zip.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef _WIN32
#include <sys/stat.h>
#endif
#ifdef HAS_GTK3
#include <gtk/gtk.h>
#endif

#ifdef _WIN32
#include <direct.h>
#include <urlmon.h>
#include <windows.h>
#pragma comment(lib, "urlmon.lib")
#define SEPARATOR '\\'
#define mkdir_p_(p) _mkdir(p)
#define strncasecmp _strnicmp
#define dlopen(a, b) LoadLibraryA(a)
#define dlsym(a, b) GetProcAddress((HMODULE)a, b)
#define dlclose(a) FreeLibrary((HMODULE)a)
#else
#include <dirent.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>
#define SEPARATOR '/'
#define mkdir_p_(p) mkdir(p, 0755)
#endif

/* ─── Progress callback interface ─── */
typedef struct {
  void (*log)(const char *msg, void *user);
  void (*progress)(int step, int total, const char *label, void *user);
  void (*set_runtime)(const char *name, void *user);
  void *user;
} progress_t;

/* ─── File utilities ─── */

static int file_exists(const char *path) {
#ifdef _WIN32
  return GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
#else
  return access(path, F_OK) == 0;
#endif
}

static int file_size(const char *path) {
#ifdef _WIN32
  struct _stat st;
  if (_stat(path, &st) != 0)
    return -1;
#else
  struct stat st;
  if (stat(path, &st) != 0)
    return -1;
#endif
  return (int)st.st_size;
}

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

static int copy_file(const char *src, const char *dst) {
  FILE *fs = fopen(src, "rb");
  if (!fs)
    return -1;
  FILE *fd = fopen(dst, "wb");
  if (!fd) {
    fclose(fs);
    return -1;
  }
  unsigned char buf[8192];
  int n;
  while ((n = (int)fread(buf, 1, sizeof(buf), fs)) > 0)
    fwrite(buf, 1, n, fd);
  fclose(fs);
  fclose(fd);
  return 0;
}

/* ─── Download ─── */

#ifdef _WIN32
static int download_url(const char *url, const char *dest, int expected_mb,
                        progress_t *p) {
  if (p && p->log)
    p->log("Downloading...\n", p->user);
  HRESULT hr = URLDownloadToFileA(NULL, url, dest, 0, NULL);
  if (SUCCEEDED(hr)) {
    int sz = file_size(dest);
    double mb = sz / 1048576.0;
    char buf[128];
    snprintf(buf, sizeof(buf), "  %.1f MB downloaded\n", mb);
    if (p && p->log)
      p->log(buf, p->user);
    return 0;
  }
  if (p && p->log)
    p->log("  Download failed\n", p->user);
  return -1;
}
#else
static int download_url(const char *url, const char *dest, int expected_mb,
                        progress_t *p) {
  char buf[256];
  snprintf(buf, sizeof(buf), "Downloading...\n");
  if (p && p->log)
    p->log(buf, p->user);
  char cmd[4096];
  snprintf(cmd, sizeof(cmd), "curl -L -s -o \"%s\" \"%s\"", dest, url);
  int rc = system(cmd);
  if (rc == 0) {
    int sz = file_size(dest);
    double mb = sz / 1048576.0;
    snprintf(buf, sizeof(buf), "  %.1f MB downloaded\n", mb);
    if (p && p->log)
      p->log(buf, p->user);
    return 0;
  }
  if (p && p->log)
    p->log("  Download failed\n", p->user);
  return -1;
}
#endif

/* ─── Archive extraction ─── */

#ifdef _WIN32
static int extract_archive(const char *archive, const char *outdir,
                           progress_t *p) {
  const char *ext = strrchr(archive, '.');
  if (!ext)
    return -1;
  if (p && p->log)
    p->log("Extracting...\n", p->user);
  if (strcmp(ext, ".exe") == 0) {
    char dest[1024];
    const char *base = strrchr(archive, '\\');
    if (!base)
      base = archive;
    else
      base++;
    snprintf(dest, sizeof(dest), "%s\\%s", outdir, base);
    return copy_file(archive, dest);
  }
  if (strcmp(ext, ".zip") == 0 || strcmp(ext, ".tar") == 0 ||
      strcmp(ext, ".gz") == 0 || strcmp(ext, ".xz") == 0) {
    char cmd[4096];
    snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\"", archive, outdir);
    if (system(cmd) == 0)
      return 0;
    snprintf(cmd, sizeof(cmd),
             "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath "
             "'%s' -Force\"",
             archive, outdir);
    return (system(cmd) == 0) ? 0 : -1;
  }
  return -1;
}
#else
static int extract_archive(const char *archive, const char *outdir,
                           progress_t *p) {
  const char *ext = strrchr(archive, '.');
  if (!ext)
    return -1;
  if (p && p->log)
    p->log("Extracting...\n", p->user);
  char cmd[4096];
  if (strcmp(ext, ".gz") == 0 || strcmp(ext, ".xz") == 0 ||
      strcmp(ext, ".tar") == 0 || strcmp(ext, ".tgz") == 0)
    snprintf(cmd, sizeof(cmd), "tar -xf \"%s\" -C \"%s\"", archive, outdir);
  else if (strcmp(ext, ".zip") == 0)
    snprintf(cmd, sizeof(cmd), "unzip -o \"%s\" -d \"%s\"", archive, outdir);
  else {
    const char *base = strrchr(archive, '/');
    if (!base)
      base = archive;
    else
      base++;
    char dest[1024];
    snprintf(dest, sizeof(dest), "%s/%s", outdir, base);
    return copy_file(archive, dest);
  }
  return (system(cmd) == 0) ? 0 : -1;
}
#endif

/* ─── File locator ─── */

static void normalize_sep(char *p) {
  for (; *p; p++)
    if (*p == '/' || *p == '\\')
      *p = SEPARATOR;
}

static int locate_file(const char *dir, const char *file, char *out,
                       int out_size) {
  char tmp[1024];
  char fnorm[256];
  strncpy(fnorm, file, sizeof(fnorm) - 1);
  fnorm[sizeof(fnorm) - 1] = '\0';
  normalize_sep(fnorm);

#ifdef _WIN32
  snprintf(tmp, sizeof(tmp), "%s\\%s", dir, fnorm);
  if (GetFileAttributesA(tmp) != INVALID_FILE_ATTRIBUTES) {
    strncpy(out, tmp, out_size - 1);
    out[out_size - 1] = '\0';
    return 0;
  }
  char pat[1024];
  snprintf(pat, sizeof(pat), "%s\\*", dir);
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA(pat, &fd);
  if (h == INVALID_HANDLE_VALUE)
    return -1;
  while (FindNextFileA(h, &fd)) {
    if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
      continue;
    if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
      continue;
    snprintf(tmp, sizeof(tmp), "%s\\%s\\%s", dir, fd.cFileName, fnorm);
    if (GetFileAttributesA(tmp) != INVALID_FILE_ATTRIBUTES) {
      strncpy(out, tmp, out_size - 1);
      out[out_size - 1] = '\0';
      FindClose(h);
      return 0;
    }
  }
  FindClose(h);
#else
  snprintf(tmp, sizeof(tmp), "%s/%s", dir, fnorm);
  if (access(tmp, F_OK) == 0) {
    strncpy(out, tmp, out_size - 1);
    out[out_size - 1] = '\0';
    return 0;
  }
  DIR *d = opendir(dir);
  if (!d)
    return -1;
  struct dirent *e;
  while ((e = readdir(d)) != NULL) {
    if (e->d_name[0] == '.' || e->d_type != DT_DIR)
      continue;
    snprintf(tmp, sizeof(tmp), "%s/%s/%s", dir, e->d_name, fnorm);
    if (access(tmp, F_OK) == 0) {
      strncpy(out, tmp, out_size - 1);
      out[out_size - 1] = '\0';
      closedir(d);
      return 0;
    }
  }
  closedir(d);
#endif
  return -1;
}

/* ─── Runtime installer ─── */

static void build_manifest_json(const runtime_entry_t *rt,
                                const char *staging_windows,
                                const char *staging_linux, char *out,
                                int out_size) {
  char win_files[2048] = {0};
  for (int i = 0; i < rt->files_windows_count; i++) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s\"%s\"", (i > 0) ? "," : "",
             rt->files_windows[i]);
    strncat(win_files, tmp, sizeof(win_files) - strlen(win_files) - 1);
  }
  char lin_files[2048] = {0};
  for (int i = 0; i < rt->files_linux_count; i++) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "%s\"%s\"", (i > 0) ? "," : "",
             rt->files_linux[i]);
    strncat(lin_files, tmp, sizeof(lin_files) - strlen(lin_files) - 1);
  }
  snprintf(out, out_size,
           "{\n"
           "  \"name\": \"%s\",\n"
           "  \"version\": \"%s\",\n"
           "  \"is_runtime\": true,\n"
           "  \"entry\": \"%s\",\n"
           "  \"files\": [],\n"
           "  \"files_windows\": [%s],\n"
           "  \"files_linux\": [%s],\n"
           "  \"platforms\": {\n"
           "    \"windows\": { \"entry\": \"%s\" },\n"
           "    \"linux-x64\": { \"entry\": \"%s\" }\n"
           "  }\n"
           "}\n",
           rt->name, rt->version, rt->entry_windows, win_files, lin_files,
           rt->entry_windows, rt->entry_linux);
}

static int pack_runtime(const runtime_entry_t *rt, const char *staging_dir,
                        const char *output_dir, progress_t *p) {
  char manifest_path[1024];
  snprintf(manifest_path, sizeof(manifest_path), "%s%c%s.arappmake",
           staging_dir, SEPARATOR, rt->name);
  char manifest_json[4096];
  build_manifest_json(rt, staging_dir, staging_dir, manifest_json,
                      sizeof(manifest_json));
  FILE *mf = fopen(manifest_path, "wb");
  if (!mf) {
    if (p && p->log)
      p->log("  Cannot create manifest\n", p->user);
    return -1;
  }
  unsigned char hdr[20];
  memset(hdr, 0, 20);
  memcpy(hdr, "ALRIGROUP@APPMAKE", 17);
  ar_write_le16(hdr + 18, 0x0001);
  fwrite(hdr, 1, 20, mf);
  fwrite(manifest_json, 1, strlen(manifest_json), mf);
  fclose(mf);

  char output_path[1024];
  snprintf(output_path, sizeof(output_path), "%s%c%s.arapp", output_dir,
           SEPARATOR, rt->name);
  zip_writer_t *z = zip_open_arapp(output_path);
  if (!z) {
    if (p && p->log)
      p->log("  Cannot create .arapp\n", p->user);
    return -1;
  }
  int total = 0;
  char name_buf[1024];
  /* Add manifest */
  {
    char mname[64];
    snprintf(mname, sizeof(mname), "%s.arappmake", rt->name);
    zip_add_entry(z, mname, ZIP_METHOD_STORED);
    FILE *f = fopen(manifest_path, "rb");
    if (f) {
      fseek(f, 0, SEEK_END);
      int len = (int)ftell(f);
      fseek(f, 0, SEEK_SET);
      unsigned char *buf = (unsigned char *)malloc(len);
      fread(buf, 1, len, f);
      fclose(f);
      zip_write(z, buf, len);
      free(buf);
    }
    total++;
  }
  /* Add files */
  for (int pass = 0; pass < 2; pass++) {
    int count = (pass == 0) ? rt->files_windows_count : rt->files_linux_count;
    const char *const *files =
        (pass == 0) ? rt->files_windows : rt->files_linux;
    for (int i = 0; i < count; i++) {
      const char *file = files[i];
      int is_dir = (file[strlen(file) - 1] == '/');
      snprintf(name_buf, sizeof(name_buf), "%s%c%s", staging_dir, SEPARATOR,
               file);
      if (is_dir) {
        if (!file_exists(name_buf))
          continue;
        /* Recursively add all files under directory */
        char find_cmd[8192];
        snprintf(find_cmd, sizeof(find_cmd),
                 "find \"%s\" -type f -o -type l 2>/dev/null | sort", name_buf);
        FILE *fp = popen(find_cmd, "r");
        if (fp) {
          char line[4096];
          while (fgets(line, sizeof(line), fp)) {
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            /* Compute entry name relative to staging_dir */
            char rel[4096];
            const char *p = line + strlen(staging_dir) + 1;
            if (*p == SEPARATOR) p++;
            snprintf(rel, sizeof(rel), "%s", p);
            if (!file_exists(line)) continue;
            zip_add_entry(z, rel, ZIP_METHOD_STORED);
            FILE *f = fopen(line, "rb");
            if (f) {
              fseek(f, 0, SEEK_END);
              int flen = (int)ftell(f);
              fseek(f, 0, SEEK_SET);
              unsigned char *buf = (unsigned char *)malloc(flen);
              fread(buf, 1, flen, f);
              fclose(f);
              zip_write(z, buf, flen);
              free(buf);
              total++;
            }
          }
          pclose(fp);
        }
        /* Also add the directory entry itself */
        zip_add_entry(z, file, ZIP_METHOD_STORED);
        total++;
        continue;
      }
      if (!file_exists(name_buf))
        continue;
      zip_add_entry(z, file, ZIP_METHOD_STORED);
      FILE *f = fopen(name_buf, "rb");
      if (f) {
        fseek(f, 0, SEEK_END);
        int flen = (int)ftell(f);
        fseek(f, 0, SEEK_SET);
        unsigned char *buf = (unsigned char *)malloc(flen);
        fread(buf, 1, flen, f);
        fclose(f);
        zip_write(z, buf, flen);
        free(buf);
        total++;
      }
    }
  }
  zip_close(z);
  char buf[128];
  snprintf(buf, sizeof(buf), "  Packed %s.arapp (%d entries)\n", rt->name,
           total);
  if (p && p->log)
    p->log(buf, p->user);
  return 0;
}

static int is_runtime_installed(const char *system_root, const char *name,
                                const char *version) {
    char path[1024];
    snprintf(path, sizeof(path), "%s%crun%c%s.arapp", system_root, SEPARATOR, SEPARATOR, name);
    if (!file_exists(path))
        return 0;
    /* version marker: missing or different version -> reinstall */
    char vpath[1024];
    snprintf(vpath, sizeof(vpath), "%s%crun%c.versions%c%s", system_root,
             SEPARATOR, SEPARATOR, SEPARATOR, name);
    FILE *vf = fopen(vpath, "r");
    if (!vf)
        return 0;
    char buf[128];
    if (fgets(buf, sizeof(buf), vf) == NULL) {
        fclose(vf);
        return 0;
    }
    fclose(vf);
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    return (strcmp(buf, version) == 0);
}

static void write_runtime_version(const char *system_root, const char *name,
                                  const char *version) {
    char vdir[1024];
    snprintf(vdir, sizeof(vdir), "%s%crun%c.versions", system_root, SEPARATOR, SEPARATOR);
    mkdir_p(vdir);
    char vpath[1024];
    snprintf(vpath, sizeof(vpath), "%s%c%s", vdir, SEPARATOR, name);
    FILE *vf = fopen(vpath, "w");
    if (vf) {
        fprintf(vf, "%s\n", version);
        fclose(vf);
    }
}

static int install_runtime(const runtime_entry_t *rt, const char *system_root,
                           progress_t *p) {
#ifndef _WIN32
  if (rt->skip_linux) {
    char skip_buf[256];
    snprintf(skip_buf, sizeof(skip_buf),
             "  Runtime %s is provided by the system toolchain on Linux (skipped)\n",
             rt->name);
    if (p && p->log) p->log(skip_buf, p->user);
    return 0;
  }
#endif
  if (is_runtime_installed(system_root, rt->name, rt->version)) {
    char skip_buf[256];
    snprintf(skip_buf, sizeof(skip_buf), "  Runtime %s v%s is already present in %s/run/%s.arapp (skipping download)\n",
             rt->name, rt->version, system_root, rt->name);
    if (p && p->log) p->log(skip_buf, p->user);
    return 0;
  }

  char buf[256];
  snprintf(buf, sizeof(buf), "Installing %s v%s (%d MB)...\n", rt->name,
           rt->version, rt->size_mb);
  if (p && p->log)
    p->log(buf, p->user);

  char staging[1024];
  snprintf(staging, sizeof(staging), "%s%c.staging%carinstall%c%s", system_root,
           SEPARATOR, SEPARATOR, SEPARATOR, rt->name);
  mkdir_p(staging);

  const char *win_fname = strrchr(rt->url_windows, '/');
  win_fname = win_fname ? win_fname + 1 : "download.bin";
  const char *lin_fname = strrchr(rt->url_linux, '/');
  lin_fname = lin_fname ? lin_fname + 1 : "download.bin";
  char win_dest[1024], lin_dest[1024];
  snprintf(win_dest, sizeof(win_dest), "%s%c%s", staging, SEPARATOR, win_fname);
  snprintf(lin_dest, sizeof(lin_dest), "%s%c%s", staging, SEPARATOR, lin_fname);

  /* Download (only the current OS) */
#ifdef _WIN32
  if (p && p->progress)
    p->progress(1, 5, "Downloading Windows binary...", p->user);
  if (download_url(rt->url_windows, win_dest, rt->size_mb, p) != 0)
    return -1;
#else
  if (p && p->progress)
    p->progress(1, 5, "Downloading Linux binary...", p->user);
  if (download_url(rt->url_linux, lin_dest, rt->size_mb, p) != 0)
    return -1;
#endif

  /* Extract */
  if (p && p->progress)
    p->progress(2, 5, "Extracting...", p->user);
  char arch_dir[1024];
#ifdef _WIN32
  snprintf(arch_dir, sizeof(arch_dir), "%s%cwin", staging, SEPARATOR);
#else
  snprintf(arch_dir, sizeof(arch_dir), "%s%clin", staging, SEPARATOR);
#endif
  mkdir_p(arch_dir);
#ifdef _WIN32
  extract_archive(win_dest, arch_dir, p);
#else
  extract_archive(lin_dest, arch_dir, p);
#endif

  /* Find entry */
  if (p && p->progress)
    p->progress(3, 5, "Locating binaries...", p->user);
#ifndef _WIN32
  if (strcmp(rt->name, "lua") == 0) {
    /* lua.org ships source only; build it and place binaries under bin/ */
    char build_dir[1024] = {0};
    DIR *d = opendir(arch_dir);
    if (d) {
      struct dirent *e;
      int count = 0;
      while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;
        count++;
        if (count == 1 && e->d_type == DT_DIR)
          snprintf(build_dir, sizeof(build_dir), "%s/%s", arch_dir, e->d_name);
      }
      closedir(d);
    }
    if (build_dir[0]) {
      char cmd[4096];
      snprintf(cmd, sizeof(cmd), "make -C \"%s\" linux >/dev/null 2>&1", build_dir);
      system(cmd);
      char bin_dir[1024];
      snprintf(bin_dir, sizeof(bin_dir), "%s/bin", arch_dir);
      mkdir_p(bin_dir);
      snprintf(cmd, sizeof(cmd),
               "cp \"%s/src/lua\" \"%s/src/luac\" \"%s/\" 2>/dev/null",
               build_dir, build_dir, bin_dir);
      system(cmd);
      if (p && p->log)
        p->log("  Built lua from source\n", p->user);
    }
  }
#endif
  char exe_path[1024];
  int found = (locate_file(arch_dir,
#ifdef _WIN32
                           rt->entry_windows
#else
                           rt->entry_linux
#endif
                           ,
                           exe_path, sizeof(exe_path)) == 0);
  if (!found) {
    if (p && p->log)
      p->log("  Entry point not found\n", p->user);
    return -1;
  }

  /* Copy files to staging root (recursive, flatten single-wrapper dirs) */
  if (p && p->progress)
    p->progress(4, 5, "Copying files...", p->user);
#ifdef _WIN32
  {
    char pat[1024];
    snprintf(pat, sizeof(pat), "%s\\*", arch_dir);
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pat, &fd);
    if (h != INVALID_HANDLE_VALUE) {
      do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
          continue;
        char src[1024], dst[1024];
        snprintf(src, sizeof(src), "%s\\%s", arch_dir, fd.cFileName);
        snprintf(dst, sizeof(dst), "%s\\%s", staging, fd.cFileName);
        if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
          copy_file(src, dst);
      } while (FindNextFileA(h, &fd) != 0);
      FindClose(h);
    }
  }
#else
  {
    /* Check if extracted content is wrapped in a single top-level dir */
    DIR *d = opendir(arch_dir);
    if (!d) return -1;
    struct dirent *e;
    int entry_count = 0;
    char wrapper_dir[1024] = {0};
    while ((e = readdir(d)) != NULL) {
      if (e->d_name[0] == '.') continue;
      entry_count++;
      if (entry_count == 1 && e->d_type == DT_DIR)
        snprintf(wrapper_dir, sizeof(wrapper_dir), "%s/%s", arch_dir, e->d_name);
    }
    closedir(d);

    const char *src_dir = arch_dir;
    if (entry_count == 1 && wrapper_dir[0]) {
      /* Single top-level dir: use its contents directly (flatten) */
      src_dir = wrapper_dir;
    }

    /* Recursively copy all files (including symlinks) preserving relative paths */
    char cmd[8192];
    snprintf(cmd, sizeof(cmd),
             "cd \"%s\" && find . \\( -type f -o -type l \\) -exec cp -P --parents {} \"%s/\" \\;",
             src_dir, staging);
    system(cmd);
    /* Copy directory entries (for empty dir markers like "lib/") */
    snprintf(cmd, sizeof(cmd),
             "cd \"%s\" && find . -type d -exec mkdir -p \"%s/{}\" \\;",
             src_dir, staging);
    system(cmd);
  }
#endif

  /* Pack */
  if (p && p->progress)
    p->progress(5, 5, "Packaging runtime...", p->user);
  char run_dir[1024];
  snprintf(run_dir, sizeof(run_dir), "%s%crun", system_root, SEPARATOR);
  mkdir_p(run_dir);
  if (pack_runtime(rt, staging, run_dir, p) != 0) {
    if (p && p->log)
      p->log("  Packaging failed\n", p->user);
    return -1;
  }
  write_runtime_version(system_root, rt->name, rt->version);

  snprintf(buf, sizeof(buf), "  Installed -> %s\n", run_dir);
  if (p && p->log)
    p->log(buf, p->user);
  return 0;
}

/* ─── CLI mode ─── */

static const runtime_entry_t *find_runtime_by_name(const char *name) {
  for (int i = 0; i < RUNTIME_COUNT; i++)
    if (strcmp(available_runtimes[i].name, name) == 0)
      return &available_runtimes[i];
  return NULL;
}

static void cli_log(const char *msg, void *user) {
    (void)user;
    if (msg) {
        printf("%s", msg);
        fflush(stdout);
    }
}

static int run_cli(int argc, char **argv, const char *system_root) {
  int installed = 0, failed = 0;
  progress_t p = { .log = cli_log, .progress = NULL, .set_runtime = NULL, .user = NULL };
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "all") == 0) {
      for (int j = 0; j < RUNTIME_COUNT; j++) {
        printf("\n==> Installing %s (%s)...\n", available_runtimes[j].name, available_runtimes[j].version);
        fflush(stdout);
        if (install_runtime(&available_runtimes[j], system_root, &p) == 0)
          installed++;
        else
          failed++;
      }
    } else {
      const runtime_entry_t *rt = find_runtime_by_name(argv[i]);
      if (!rt) {
        printf("  Unknown runtime: %s\n", argv[i]);
        failed++;
        continue;
      }
      printf("\n==> Installing %s (%s)...\n", rt->name, rt->version);
      fflush(stdout);
      if (install_runtime(rt, system_root, &p) == 0)
        installed++;
      else
        failed++;
    }
  }
  printf("\n  %d installed, %d failed\n", installed, failed);
  return (failed > 0) ? 1 : 0;
}

/* ─── Build & setup ─── */

static int find_project_root(char *out, int out_size) {
  char exe_dir[1024] = {0};
#ifdef _WIN32
  GetModuleFileNameA(NULL, exe_dir, sizeof(exe_dir) - 1);
  char *p = strrchr(exe_dir, '\\');
  if (p)
    *p = '\0';
  char test[1024];
  strncpy(test, exe_dir, sizeof(test) - 1);
  while (1) {
    char marker[1024];
    snprintf(marker, sizeof(marker), "%s\\CMakeLists.txt", test);
    if (GetFileAttributesA(marker) != INVALID_FILE_ATTRIBUTES) {
      snprintf(marker, sizeof(marker), "%s\\CMakeLists.txt", test);
      FILE *f = fopen(marker, "r");
      int ok = 0;
      if (f) {
        char line[256];
        while (fgets(line, sizeof(line), f))
          if (strstr(line, "project(ALRIOS")) {
            ok = 1;
            break;
          }
        fclose(f);
      }
      if (ok) {
        strncpy(out, test, out_size - 1);
        return 0;
      }
    }
    char *last = strrchr(test, '\\');
    if (!last || last == test)
      break;
    *last = '\0';
  }
  if (GetCurrentDirectoryA(sizeof(test), test)) {
    char marker[1024];
    snprintf(marker, sizeof(marker), "%s\\CMakeLists.txt", test);
    if (GetFileAttributesA(marker) != INVALID_FILE_ATTRIBUTES) {
      strncpy(out, test, out_size - 1);
      return 0;
    }
  }
#else
  char link[32] = "/proc/self/exe";
  ssize_t len = readlink(link, exe_dir, sizeof(exe_dir) - 1);
  if (len > 0) {
    exe_dir[len] = '\0';
    char *p = strrchr(exe_dir, '/');
    if (p)
      *p = '\0';
  } else
    strncpy(exe_dir, ".", sizeof(exe_dir) - 1);
  char test[1024];
  strncpy(test, exe_dir, sizeof(test) - 1);
  while (1) {
    char marker[1024];
    snprintf(marker, sizeof(marker), "%s/CMakeLists.txt", test);
    if (access(marker, F_OK) == 0) {
      strncpy(out, test, out_size - 1);
      return 0;
    }
    char *last = strrchr(test, '/');
    if (!last || last == test)
      break;
    *last = '\0';
  }
  if (getcwd(test, sizeof(test))) {
    char marker[1024];
    snprintf(marker, sizeof(marker), "%s/CMakeLists.txt", test);
    if (access(marker, F_OK) == 0) {
      strncpy(out, test, out_size - 1);
      return 0;
    }
  }
#endif
  return -1;
}

static int setup_system_root(const char *system_root) {
  const char *dirs[] = {"run", "etc", "storage", "tmp", ".staging", NULL};
  for (int i = 0; dirs[i]; i++) {
    char path[1024];
    snprintf(path, sizeof(path), "%s%c%s", system_root, SEPARATOR, dirs[i]);
    mkdir_p(path);
  }
  return 0;
}

static void cleanup_staging(const char *system_root) {
  char path[1024];
  snprintf(path, sizeof(path), "%s%c.staging%carinstall", system_root,
           SEPARATOR, SEPARATOR);
  if (file_exists(path)) {
    char cmd[1024];
#ifdef _WIN32
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", path);
#else
    snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", path);
#endif
    system(cmd);
  }
}

/* ─── Globals (shared between CLI and GUI) ─── */
static char g_system_root[1024];
static char g_project_root[1024];

/* ─── GTK3 GUI ─── */
#ifdef HAS_GTK3

static GtkWidget *g_window;
static GtkWidget *g_progress;
static GtkWidget *g_log_view;
static GtkTextBuffer *g_log_buf;
static GtkWidget *g_btn_install;
static GtkWidget *g_label_status;
static GtkWidget *g_check[64];
static int g_selected_count;
static int *g_selected_idx;

typedef struct {
  char *msg;
} log_msg_t;
typedef struct {
  int step;
  int total;
  char *label;
} prog_msg_t;
typedef struct {
  char *name;
} rt_msg_t;
typedef struct {
  int done;
} done_msg_t;

static gboolean on_log(gpointer data) {
  log_msg_t *m = data;
  GtkTextIter end;
  gtk_text_buffer_get_end_iter(g_log_buf, &end);
  gtk_text_buffer_insert(g_log_buf, &end, m->msg, -1);
  g_free(m->msg);
  g_free(m);
  return FALSE;
}

static gboolean on_progress(gpointer data) {
  prog_msg_t *m = data;
  gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(g_progress),
                                (double)m->step / m->total);
  gtk_label_set_text(GTK_LABEL(g_label_status), m->label);
  g_free(m->label);
  g_free(m);
  return FALSE;
}

static gboolean on_set_runtime(gpointer data) {
  rt_msg_t *m = data;
  gtk_label_set_text(GTK_LABEL(g_label_status), m->name);
  g_free(m->name);
  g_free(m);
  return FALSE;
}

static gboolean on_install_done(gpointer data) {
  done_msg_t *m = data;
  gtk_button_set_label(GTK_BUTTON(g_btn_install),
                       m->done ? "Done" : "Install Selected");
  gtk_widget_set_sensitive(g_btn_install, TRUE);
  for (int i = 0; i < g_selected_count; i++)
    gtk_widget_set_sensitive(g_check[g_selected_idx[i]], TRUE);
  g_free(m);
  return FALSE;
}

static void gui_log(const char *msg, void *user) {
  (void)user;
  log_msg_t *m = g_new(log_msg_t, 1);
  m->msg = g_strdup(msg);
  g_idle_add(on_log, m);
}

static void gui_progress(int step, int total, const char *label, void *user) {
  (void)user;
  prog_msg_t *m = g_new(prog_msg_t, 1);
  m->step = step;
  m->total = total;
  m->label = g_strdup(label);
  g_idle_add(on_progress, m);
}

static void gui_set_runtime(const char *name, void *user) {
  (void)user;
  rt_msg_t *m = g_new(rt_msg_t, 1);
  m->name = g_strdup(name);
  g_idle_add(on_set_runtime, m);
}

static void *install_runtime_thread(void *arg);
static gboolean auto_install_all(gpointer data);

static void install_selected(void) {
  gtk_button_set_label(GTK_BUTTON(g_btn_install), "Installing...");
  gtk_widget_set_sensitive(g_btn_install, FALSE);
  for (int i = 0; i < g_selected_count; i++)
    gtk_widget_set_sensitive(g_check[g_selected_idx[i]], FALSE);
  gtk_text_buffer_set_text(g_log_buf, "", -1);

  g_thread_new("install",
               (GThreadFunc)(void *(*)(void *)) & install_runtime_thread, NULL);
}

static void *install_runtime_thread(void *arg) {
  (void)arg;
  progress_t p;
  p.log = gui_log;
  p.progress = gui_progress;
  p.set_runtime = gui_set_runtime;
  p.user = NULL;

  for (int i = 0; i < g_selected_count; i++) {
    const runtime_entry_t *rt = &available_runtimes[g_selected_idx[i]];
    char buf[64];
    snprintf(buf, sizeof(buf), "%s (%d MB)", rt->name, rt->size_mb);
    p.set_runtime(buf, NULL);
    install_runtime(rt, g_system_root, &p);
  }

  done_msg_t *m = g_new(done_msg_t, 1);
  m->done = 1;
  g_idle_add(on_install_done, m);
  return NULL;
}

static void on_install_clicked(GtkButton *btn, gpointer user) {
  (void)btn;
  (void)user;
  g_selected_count = 0;
  for (int i = 0; i < RUNTIME_COUNT; i++) {
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_check[i])))
      g_selected_idx[g_selected_count++] = i;
  }
  if (g_selected_count == 0) {
    GtkTextIter end;
    gtk_text_buffer_get_end_iter(g_log_buf, &end);
    gtk_text_buffer_insert(g_log_buf, &end, "Select at least one runtime.\n",
                           -1);
    return;
  }
  install_selected();
}

static void activate(GtkApplication *app, gpointer userdata) {
  (void)userdata;
  GtkWidget *win = gtk_application_window_new(app);
  g_window = win;
  gtk_window_set_title(GTK_WINDOW(win), "ALRI OS Runtime Installer");
  gtk_window_set_default_size(GTK_WINDOW(win), 620, 520);
  gtk_container_set_border_width(GTK_CONTAINER(win), 12);

  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
  gtk_container_add(GTK_CONTAINER(win), vbox);

  /* Header */
  GtkWidget *hdr = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(hdr),
                       "<b><big>ALRI OS — Runtime Installer</big></b>\n"
                       "<span size='small'>Installing all runtimes by default</span>");
  gtk_box_pack_start(GTK_BOX(vbox), hdr, FALSE, FALSE, 0);

  /* Scrolled list of runtimes */
  GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_NEVER,
                                 GTK_POLICY_AUTOMATIC);
  gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

  GtkWidget *list_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
  gtk_container_add(GTK_CONTAINER(scroll), list_box);

  for (int i = 0; i < RUNTIME_COUNT; i++) {
    const runtime_entry_t *rt = &available_runtimes[i];
    char markup[256];
    snprintf(
        markup, sizeof(markup),
        "<b>%-8s</b>  v%s    <span size='small' foreground='#888'>%d MB</span>",
        rt->name, rt->version, rt->size_mb);
    GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_check[i] = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_check[i]), TRUE);
    gtk_box_pack_start(GTK_BOX(row), g_check[i], FALSE, FALSE, 0);
    GtkWidget *lbl = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(lbl), markup);
    gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(list_box), row, FALSE, FALSE, 2);
  }

  /* Status + progress */
  g_label_status = gtk_label_new("Ready");
  gtk_box_pack_start(GTK_BOX(vbox), g_label_status, FALSE, FALSE, 0);
  g_progress = gtk_progress_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox), g_progress, FALSE, FALSE, 0);

  /* Install button */
  g_btn_install = gtk_button_new_with_label("Install All Runtimes");
  g_signal_connect(g_btn_install, "clicked", G_CALLBACK(on_install_clicked),
                   NULL);
  gtk_box_pack_start(GTK_BOX(vbox), g_btn_install, FALSE, FALSE, 4);

  /* Log view */
  GtkWidget *log_sw = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_min_content_height(GTK_SCROLLED_WINDOW(log_sw), 160);
  g_log_view = gtk_text_view_new();
  gtk_text_view_set_editable(GTK_TEXT_VIEW(g_log_view), FALSE);
  g_log_buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(g_log_view));
  gtk_container_add(GTK_CONTAINER(log_sw), g_log_view);
  gtk_box_pack_start(GTK_BOX(vbox), log_sw, TRUE, TRUE, 0);

  g_selected_idx = malloc(RUNTIME_COUNT * sizeof(int));

  gtk_widget_show_all(win);
  cleanup_staging(g_system_root);

  /* Auto-start installation of all runtimes */
  g_timeout_add(300, auto_install_all, NULL);
}

static gboolean auto_install_all(gpointer data) {
  (void)data;
  g_selected_count = RUNTIME_COUNT;
  for (int i = 0; i < RUNTIME_COUNT; i++)
    g_selected_idx[i] = i;
  install_selected();
  return FALSE;
}

#endif /* HAS_GTK3 */

/* ─── Print usage ─── */

static void print_usage(void) {
  printf("\n");
  printf("  ALRI OS Runtime Installer\n");
  printf("\n");
  printf("  Usage:\n");
  printf("    arinstall                     GUI (auto-installs all runtimes)\n");
  printf("    arinstall all                 Install all runtimes (CLI)\n");
  printf("    arinstall node python3 ...    Install specific runtimes (CLI)\n");
  printf("    arinstall --help              Show this help\n");
  printf("\n");
  printf("  Available:\n");
  for (int i = 0; i < RUNTIME_COUNT; i++)
    printf("    %-12s v%s  (%d MB)\n", available_runtimes[i].name,
           available_runtimes[i].version, available_runtimes[i].size_mb);
  printf("\n");
}

/* ─── Entry point ─── */

int main(int argc, char **argv) {
  /* Find project root and system root */
  if (find_project_root(g_project_root, sizeof(g_project_root)) != 0) {
    fprintf(stderr, "Could not find project root\n");
    return 1;
  }
  snprintf(g_system_root, sizeof(g_system_root), "%s%carcore", g_project_root,
           SEPARATOR);

  /* Handle --help */
  if (argc > 1 &&
      (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
    print_usage();
    return 0;
  }

  /* Check for CLI mode: runtime names passed as args */
  int has_runtime_args = 0;
  int skip_build = 0;
  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--skip-build") == 0)
      skip_build = 1;
    else if (argv[i][0] != '-')
      has_runtime_args = 1;
  }

  if (!skip_build) {
    printf("Building project...\n");
    char cmd[4096];
    snprintf(cmd, sizeof(cmd),
             "cmake -B \"%s/build\" -S \"%s\" && cmake --build \"%s/build\" "
             "--target armake --target arcore --target arinstall",
             g_project_root, g_project_root, g_project_root);
    int rc = system(cmd);
    if (rc != 0) {
      fprintf(stderr, "Build failed\n");
      return 1;
    }
  }

  setup_system_root(g_system_root);

  if (has_runtime_args) {
    /* CLI mode */
    char *filtered[64];
    int fc = 0;
    filtered[fc++] = argv[0];
    for (int i = 1; i < argc; i++)
      if (argv[i][0] != '-')
        filtered[fc++] = argv[i];
    int result = run_cli(fc, filtered, g_system_root);
    cleanup_staging(g_system_root);
    return result;
  }

  /* GUI mode */
#ifdef HAS_GTK3
  GtkApplication *app =
      gtk_application_new("alrios.installer", G_APPLICATION_DEFAULT_FLAGS);
  g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
  int status = g_application_run(G_APPLICATION(app), argc, argv);
  g_object_unref(app);
  return status;
#else
  fprintf(stderr, "GTK3 not available. Use CLI mode: arinstall all\n");
  print_usage();
  return 1;
#endif
}
