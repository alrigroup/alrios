/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "aros_hal.h"
#include "home_os.h"
#include "ar_ipc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

#define APP_NAME      "cdn"
#define GATEWAY_HOST  "127.0.0.1"
#define GATEWAY_PORT  9500
#define MAX_ATTEMPTS  30
#define MAX_REQ       65536
#define MAX_ENTRIES   1024
#define MAX_CFG_LINE  2048

static int server_port = 3005;
static int g_cache_ttl = 31536000;

typedef struct {
    char path[256];
    char file[1536];
} CdnEntry;

static CdnEntry g_entries[MAX_ENTRIES];
static int g_entry_count = 0;
static void *g_mutex = NULL;
static char g_cfg_path[2048] = {0};

static const char *g_hosts[] = { "cdn.alrigroup.com", "cdn.localhost" };
static const char *g_route = "*";

/* ------------------------------------------------------------------ */
/* small string/fs helpers                                             */
/* ------------------------------------------------------------------ */

static void trim_copy(char *out, const char *in, int in_len, int out_size) {
    int s = 0, e = in_len;
    while (s < e && (in[s] == ' ' || in[s] == '\t' || in[s] == '\r' || in[s] == '\n')) s++;
    while (e > s && (in[e - 1] == ' ' || in[e - 1] == '\t' || in[e - 1] == '\r' || in[e - 1] == '\n')) e--;
    int n = e - s;
    if (n >= out_size) n = out_size - 1;
    if (n > 0) memcpy(out, in + s, (size_t)n);
    out[n] = '\0';
}

static int ci_cmp_n(const char *a, const char *b, int n) {
    for (int i = 0; i < n; i++) {
        int ca = (unsigned char)a[i], cb = (unsigned char)b[i];
        if (ca >= 'A' && ca <= 'Z') ca += 32;
        if (cb >= 'A' && cb <= 'Z') cb += 32;
        if (ca != cb) return ca - cb;
    }
    return 0;
}

static void lower_ext(char *out, int out_size, const char *path) {
    out[0] = '\0';
    const char *dot = strrchr(path, '.');
    if (!dot) return;
    const char *ext = dot + 1;
    int n = 0;
    while (ext[n] && n < out_size - 1) {
        char ch = ext[n];
        if (ch >= 'A' && ch <= 'Z') ch += 32;
        out[n] = ch;
        n++;
    }
    out[n] = '\0';
}

static void mkdir_p(const char *path) {
    char tmp[1024];
    strncpy(tmp, path, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char save = *p;
            *p = '\0';
#ifdef _WIN32
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = save;
        }
    }
#ifdef _WIN32
    _mkdir(tmp);
#else
    mkdir(tmp, 0755);
#endif
}

static void cfg_dir_of(const char *cfg_path, char *dir, int size) {
    strncpy(dir, cfg_path, size - 1);
    dir[size - 1] = '\0';
    char *sep = strrchr(dir, '/');
#ifdef _WIN32
    char *sep2 = strrchr(dir, '\\');
    if (sep2 && (!sep || sep2 > sep)) sep = sep2;
#endif
    if (sep) *sep = '\0';
}

static int get_cfg_path(void) {
    char exedir[1024];
    if (home_os_get_exe_dir(exedir, sizeof(exedir)) == 0 && exedir[0]) {
#ifdef _WIN32
        snprintf(g_cfg_path, sizeof(g_cfg_path), "%s\\..\\..\\storage\\cdn\\cdn.cfg", exedir);
#else
        snprintf(g_cfg_path, sizeof(g_cfg_path), "%s/../../storage/cdn/cdn.cfg", exedir);
#endif
        return 0;
    }
#ifdef _WIN32
    snprintf(g_cfg_path, sizeof(g_cfg_path), "storage\\cdn\\cdn.cfg");
#else
    snprintf(g_cfg_path, sizeof(g_cfg_path), "storage/cdn/cdn.cfg");
#endif
    return 0;
}

static long long file_size_of(FILE *f) {
    long long sz = 0;
#ifdef _WIN32
    sz = (long long)_ftelli64(f);
#else
    sz = (long long)ftello(f);
#endif
    return sz;
}

static int file_get_size(const char *path, long long *out) {
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long long sz = file_size_of(f);
    fclose(f);
    if (sz < 0) return -1;
    *out = sz;
    return 0;
}

static int file_seek_set(FILE *f, long long off) {
#ifdef _WIN32
    return _fseeki64(f, (__int64)off, SEEK_SET);
#else
    return fseeko(f, (off_t)off, SEEK_SET);
#endif
}

static int normalize_path(const char *in, char *out, int out_size) {
#ifdef _WIN32
    if (_fullpath(out, in, (size_t)out_size) == NULL) return -1;
#else
    if (realpath(in, out) == NULL) return -1;
#endif
    return 0;
}

static int is_valid_route_path(const char *p) {
    if (!p || p[0] != '/') return 0;
    if (strstr(p, "..") != NULL) return 0;
    if (strchr(p, '%') != NULL) return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* cdn.cfg persistence (auto-created on boot if missing)               */
/* ------------------------------------------------------------------ */

static int cfg_save(void) {
    char tmp[2048];
    snprintf(tmp, sizeof(tmp), "%s.tmp", g_cfg_path);

    FILE *f = fopen(tmp, "w");
    if (!f) return -1;

    fprintf(f, "# ALRIOS CDN registry — url_path = local_file\n");
    fprintf(f, "cache_ttl=%d\n", g_cache_ttl);
    fprintf(f, "\n");

    for (int i = 0; i < g_entry_count; i++) {
        fprintf(f, "%s = %s\n", g_entries[i].path, g_entries[i].file);
    }

    fclose(f);
    if (rename(tmp, g_cfg_path) != 0) {
        remove(g_cfg_path);
        if (rename(tmp, g_cfg_path) != 0) {
            remove(tmp);
            return -1;
        }
    }
    return 0;
}

static void cfg_create_default(void) {
    char dir[1024];
    cfg_dir_of(g_cfg_path, dir, sizeof(dir));
    mkdir_p(dir);
    g_cache_ttl = 31536000;
    cfg_save();
}

static void cfg_load(void) {
    FILE *f = fopen(g_cfg_path, "r");
    if (!f) {
        cfg_create_default();
        return;
    }

    char line[MAX_CFG_LINE];
    while (fgets(line, sizeof(line), f)) {
        char *eq = strchr(line, '=');
        if (!eq) continue;

        char key[256], val[1536];
        trim_copy(key, line, (int)(eq - line), sizeof(key));
        const char *vstart = eq + 1;
        trim_copy(val, vstart, (int)strlen(vstart), sizeof(val));

        if (!key[0] || !val[0]) continue;

        if (strcmp(key, "cache_ttl") == 0) {
            int t = atoi(val);
            if (t > 0) g_cache_ttl = t;
            continue;
        }

        if (key[0] != '/') continue;

        if (g_entry_count < MAX_ENTRIES) {
            snprintf(g_entries[g_entry_count].path, sizeof(g_entries[g_entry_count].path), "%s", key);
            snprintf(g_entries[g_entry_count].file, sizeof(g_entries[g_entry_count].file), "%s", val);
            g_entry_count++;
        }
    }
    fclose(f);
}

static CdnEntry *entry_find(const char *path) {
    for (int i = 0; i < g_entry_count; i++) {
        if (strcmp(g_entries[i].path, path) == 0) return &g_entries[i];
    }
    return NULL;
}

/* ------------------------------------------------------------------ */
/* IPC frames (gateway protocol, port 9500)                            */
/* ------------------------------------------------------------------ */

static int ipc_send_frame(int fd, unsigned char type, const char *data, int len) {
    unsigned char header[5];
    header[0] = (unsigned char)((len >> 24) & 0xFF);
    header[1] = (unsigned char)((len >> 16) & 0xFF);
    header[2] = (unsigned char)((len >> 8) & 0xFF);
    header[3] = (unsigned char)(len & 0xFF);
    header[4] = type;

    if (ar_socket_send(fd, header, 5) != 5) return -1;
    if (len > 0 && data) {
        int written = 0;
        while (written < len) {
            int n = ar_socket_send(fd, data + written, (size_t)(len - written));
            if (n <= 0) return -1;
            written += n;
        }
    }
    return 0;
}

static int ipc_recv_frame(int fd, unsigned char *type, char *buf, int buflen) {
    unsigned char header[5];
    int got = 0;
    while (got < 5) {
        int n = ar_socket_recv(fd, header + got, (size_t)(5 - got));
        if (n <= 0) return -1;
        got += n;
    }

    int len = ((int)header[0] << 24) | ((int)header[1] << 16) |
              ((int)header[2] << 8) | (int)header[3];
    *type = header[4];

    if (len > 0) {
        if (len > buflen - 1) len = buflen - 1;
        got = 0;
        while (got < len) {
            int n = ar_socket_recv(fd, buf + got, (size_t)(len - got));
            if (n <= 0) return -1;
            got += n;
        }
        buf[got] = '\0';
    } else {
        buf[0] = '\0';
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* Gateway route registration (proxy mode)                             */
/* ------------------------------------------------------------------ */

static int register_route(int fd, const char *prefix, const char *host, const char *mode) {
    char payload[512];
    int len = snprintf(payload, sizeof(payload),
                       "%s %s %s %s %s proxy=http://%s:%d type=stream rl=300,60",
                       APP_NAME, prefix, "GET", host, mode,
                       GATEWAY_HOST, server_port);
    if (len <= 0 || len >= (int)sizeof(payload)) return -1;

    if (ipc_send_frame(fd, 1, payload, len + 1) != 0) return -1;

    unsigned char type = 0;
    char ack[128];
    if (ipc_recv_frame(fd, &type, ack, sizeof(ack)) != 0) return -1;

    printf("[%s] Registered GET %s host=%s mode=%s (%s)\n", APP_NAME, prefix, host, mode, ack);
    return 0;
}

static int connect_and_register(void) {
    int fd = ar_socket_create(1);
    if (fd < 0) return -1;

    if (home_os_connect_timeout(fd, GATEWAY_HOST, GATEWAY_PORT, 1000) != 0) {
        ar_socket_close(fd);
        return -1;
    }

    const char *mode = "production";
    for (size_t h = 0; h < sizeof(g_hosts) / sizeof(g_hosts[0]); h++) {
        if (register_route(fd, g_route, g_hosts[h], mode) != 0) {
            ar_socket_close(fd);
            return -1;
        }
    }
    return fd;
}

static int build_routes_text(char *out, int size) {
    int used = 0;
    for (size_t h = 0; h < sizeof(g_hosts) / sizeof(g_hosts[0]); h++) {
        int n = snprintf(out + used, size - used,
                         "GET %-8s host=%-28s mode=production proxy=http://%s:%d\n",
                         g_route, g_hosts[h], GATEWAY_HOST, server_port);
        if (n < 0 || used + n >= size) break;
        used += n;
    }
    return used;
}

static int build_list_text(char *out, int size) {
    int used = 0;
    for (int i = 0; i < g_entry_count; i++) {
        int n = snprintf(out + used, size - used, "%s -> %s\n",
                         g_entries[i].path, g_entries[i].file);
        if (n < 0 || used + n >= size) break;
        used += n;
    }
    return used;
}

static int cmd_add(const char *args, char *resp, int size) {
    const char *p = args;
    while (*p == ' ' || *p == '\t') p++;

    char path[256];
    int i = 0;
    while (p[i] && p[i] != ' ' && p[i] != '\t' && p[i] != '\n' && i < (int)sizeof(path) - 1) {
        path[i] = p[i];
        i++;
    }
    path[i] = '\0';
    while (p[i] && (p[i] == ' ' || p[i] == '\t')) i++;
    const char *file = p + i;
    while (*file && (*file == '\r' || *file == '\n')) file++;

    if (!is_valid_route_path(path) || !file[0]) {
        return snprintf(resp, size, "usage: add <path> <file>  (path must start with /, no '..' or '%%')");
    }
    if (strlen(file) >= 1536) {
        return snprintf(resp, size, "file path too long");
    }

    char abs[1536];
    if (normalize_path(file, abs, sizeof(abs)) != 0) {
        return snprintf(resp, size, "file not found or invalid: %s", file);
    }
    long long sz;
    if (file_get_size(abs, &sz) != 0) {
        return snprintf(resp, size, "file not found or not a regular file: %s", abs);
    }

    ar_mutex_lock(g_mutex);
    CdnEntry *ex = entry_find(path);
    int added = 0;
    if (ex) {
        snprintf(ex->file, sizeof(ex->file), "%s", abs);
    } else if (g_entry_count < MAX_ENTRIES) {
        CdnEntry *e = &g_entries[g_entry_count++];
        snprintf(e->path, sizeof(e->path), "%s", path);
        snprintf(e->file, sizeof(e->file), "%s", abs);
        added = 1;
    } else {
        ar_mutex_unlock(g_mutex);
        return snprintf(resp, size, "entry table full (%d)", MAX_ENTRIES);
    }
    int save_rc = cfg_save();
    ar_mutex_unlock(g_mutex);

    if (save_rc != 0) {
        return snprintf(resp, size, "add ok (in memory) but failed to persist cdn.cfg");
    }
    return snprintf(resp, size, "%s %s -> %s (%lld bytes)", added ? "added" : "updated", path, abs, sz);
}

static int cmd_del(const char *args, char *resp, int size) {
    const char *p = args;
    while (*p == ' ' || *p == '\t') p++;
    char path[256];
    int i = 0;
    while (p[i] && p[i] != ' ' && p[i] != '\t' && p[i] != '\n' && i < (int)sizeof(path) - 1) {
        path[i] = p[i];
        i++;
    }
    path[i] = '\0';

    if (!path[0]) {
        return snprintf(resp, size, "usage: del <path>");
    }

    ar_mutex_lock(g_mutex);
    int found = 0;
    for (int j = 0; j < g_entry_count; j++) {
        if (strcmp(g_entries[j].path, path) == 0) {
            if (j < g_entry_count - 1) {
                memmove(&g_entries[j], &g_entries[j + 1],
                        sizeof(CdnEntry) * (size_t)(g_entry_count - j - 1));
            }
            g_entry_count--;
            found = 1;
            break;
        }
    }
    int save_rc = found ? cfg_save() : 0;
    ar_mutex_unlock(g_mutex);

    if (!found) return snprintf(resp, size, "not found: %s", path);
    if (save_rc != 0) return snprintf(resp, size, "deleted (in memory) but failed to persist cdn.cfg");
    return snprintf(resp, size, "deleted: %s", path);
}

static void handle_query(int fd, const char *q, int len) {
    char resp[4096];
    int rlen = 0;
    (void)len;

    if (strncmp(q, "ping", 4) == 0 && (q[4] == '\0' || q[4] == ' ')) {
        rlen = snprintf(resp, sizeof(resp), "pong");
    } else if (strncmp(q, "status", 6) == 0 && (q[6] == '\0' || q[6] == ' ')) {
        rlen = snprintf(resp, sizeof(resp), "%s RUNNING port=%d entries=%d", APP_NAME, server_port, g_entry_count);
    } else if (strncmp(q, "routes", 6) == 0 && (q[6] == '\0' || q[6] == ' ')) {
        rlen = build_routes_text(resp, sizeof(resp));
    } else if (strncmp(q, "list", 4) == 0 && (q[4] == '\0' || q[4] == ' ')) {
        rlen = build_list_text(resp, sizeof(resp));
    } else if (strncmp(q, "add ", 4) == 0) {
        rlen = cmd_add(q + 4, resp, sizeof(resp));
    } else if (strncmp(q, "delete ", 7) == 0) {
        rlen = cmd_del(q + 7, resp, sizeof(resp));
    } else if (strncmp(q, "del ", 4) == 0) {
        rlen = cmd_del(q + 4, resp, sizeof(resp));
    } else {
        rlen = snprintf(resp, sizeof(resp), "unknown command: %s", q);
    }

    if (rlen < 0) rlen = 0;
    if (rlen >= (int)sizeof(resp)) rlen = (int)sizeof(resp) - 1;

    ipc_send_frame(fd, IPC_QUERY_RESP, resp, rlen + 1);
}

static void query_loop(int fd) {
    home_os_set_recv_timeout(fd, 800);

    char buf[MAX_REQ];
    unsigned char type = 0;
    int idle = 0;

    while (1) {
        int r = ipc_recv_frame(fd, &type, buf, sizeof(buf));
        if (r == 0) {
            idle = 0;
            if (type == IPC_QUERY) {
                handle_query(fd, buf, (int)strlen(buf));
            }
            continue;
        }

        idle++;
        if (idle > 60) break;

        if (ipc_send_frame(fd, IPC_HEARTBEAT, NULL, 0) < 0) break;
    }

    ar_socket_close(fd);
    printf("[%s] control channel closed\n", APP_NAME);
}

static void *register_thread(void *arg) {
    (void)arg;
    for (int attempt = 1; attempt <= MAX_ATTEMPTS; attempt++) {
        int fd = connect_and_register();
        if (fd >= 0) {
            printf("[%s] routes registered + control channel open (attempt %d)\n",
                   APP_NAME, attempt);
            query_loop(fd);
            printf("[%s] control channel lost, reconnecting...\n", APP_NAME);
            ar_sleep_ms(1000);
            continue;
        }
        printf("[%s] register retry %d/%d\n", APP_NAME, attempt, MAX_ATTEMPTS);
        ar_sleep_ms(1000);
    }
    fprintf(stderr, "[%s] failed to register routes after %d attempts\n",
            APP_NAME, MAX_ATTEMPTS);
    return NULL;
}

/* ------------------------------------------------------------------ */
/* HTTP server                                                         */
/* ------------------------------------------------------------------ */

static void send_all(int c, const char *data, int len) {
    int written = 0;
    while (written < len) {
        int n = ar_socket_send(c, data + written, (size_t)(len - written));
        if (n <= 0) return;
        written += n;
    }
}

static void send_plain(int c, int status, const char *status_text, const char *ctype,
                       const char *body, int body_len) {
    char header[512];
    if (body_len < 0) body_len = body ? (int)strlen(body) : 0;
    int len = snprintf(header, sizeof(header),
                       "HTTP/1.1 %d %s\r\n"
                       "Content-Type: %s\r\n"
                       "Content-Length: %d\r\n"
                       "Access-Control-Allow-Origin: *\r\n"
                       "Connection: close\r\n\r\n",
                       status, status_text, ctype, body_len);
    if (len > 0) send_all(c, header, len);
    if (body_len > 0) send_all(c, body, body_len);
}

static void send_404(int c) {
    send_plain(c, 404, "Not Found", "application/json", "{\"error\":\"Not found\"}", -1);
}

static const char *get_mime(const char *path) {
    char ext[16];
    lower_ext(ext, sizeof(ext), path);
    if (strcmp(ext, "html") == 0 || strcmp(ext, "htm") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, "css") == 0) return "text/css";
    if (strcmp(ext, "js") == 0) return "application/javascript";
    if (strcmp(ext, "mjs") == 0) return "application/javascript";
    if (strcmp(ext, "json") == 0) return "application/json";
    if (strcmp(ext, "png") == 0) return "image/png";
    if (strcmp(ext, "jpg") == 0 || strcmp(ext, "jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, "gif") == 0) return "image/gif";
    if (strcmp(ext, "svg") == 0) return "image/svg+xml";
    if (strcmp(ext, "ico") == 0) return "image/x-icon";
    if (strcmp(ext, "webp") == 0) return "image/webp";
    if (strcmp(ext, "avif") == 0) return "image/avif";
    if (strcmp(ext, "mp4") == 0) return "video/mp4";
    if (strcmp(ext, "webm") == 0) return "video/webm";
    if (strcmp(ext, "ogg") == 0) return "audio/ogg";
    if (strcmp(ext, "mp3") == 0) return "audio/mpeg";
    if (strcmp(ext, "pdf") == 0) return "application/pdf";
    if (strcmp(ext, "woff") == 0) return "font/woff";
    if (strcmp(ext, "woff2") == 0) return "font/woff2";
    if (strcmp(ext, "ttf") == 0) return "font/ttf";
    if (strcmp(ext, "otf") == 0) return "font/otf";
    if (strcmp(ext, "xml") == 0) return "application/xml";
    if (strcmp(ext, "zip") == 0) return "application/zip";
    if (strcmp(ext, "gz") == 0) return "application/gzip";
    if (strcmp(ext, "txt") == 0) return "text/plain; charset=utf-8";
    if (strcmp(ext, "csv") == 0) return "text/csv";
    if (strcmp(ext, "webmanifest") == 0) return "application/manifest+json";
    return "application/octet-stream";
}

static const char *find_header_value(const char *buf, int total, const char *name) {
    int nlen = (int)strlen(name);
    const char *p = buf;
    const char *end = buf + total;
    while (p < end) {
        const char *nl = memchr(p, '\n', (size_t)(end - p));
        if (!nl) nl = end;
        int linelen = (int)(nl - p);
        if (linelen > nlen && ci_cmp_n(p, name, nlen) == 0 && p[nlen] == ':') {
            const char *v = p + nlen + 1;
            while (*v == ' ' || *v == '\t') v++;
            return v;
        }
        p = nl + 1;
    }
    return NULL;
}

static int parse_range(const char *hdr, long long size, long long *start, long long *end) {
    if (!hdr) return 0;
    char local[96];
    snprintf(local, sizeof(local), "%s", hdr);
    int ll = (int)strlen(local);
    while (ll > 0 && (local[ll - 1] == '\r' || local[ll - 1] == '\n' ||
                      local[ll - 1] == ' ' || local[ll - 1] == '\t')) {
        local[--ll] = '\0';
    }
    const char *p = local;
    while (*p == ' ' || *p == '\t') p++;
    if (ci_cmp_n(p, "bytes=", 6) != 0) return -1;
    p += 6;

    const char *dash = strchr(p, '-');
    if (!dash) return -1;
    if (strchr(dash + 1, ',')) return -1;

    if (dash == p) {
        char *ep = NULL;
        long long n = strtoll(dash + 1, &ep, 10);
        if (ep == dash + 1) return -1;
        if (n <= 0) return -2;
        *start = size - n;
        if (*start < 0) *start = 0;
        *end = size - 1;
        return 1;
    }

    int prelen = (int)(dash - p);
    if (prelen <= 0 || prelen > 18) return -1;
    char num[20];
    memcpy(num, p, (size_t)prelen);
    num[prelen] = '\0';
    for (int i = 0; i < prelen; i++) if (num[i] < '0' || num[i] > '9') return -1;
    long long rstart = strtoll(num, NULL, 10);

    long long rend;
    const char *post = dash + 1;
    if (*post == '\0') {
        rend = size - 1;
    } else {
        int plen = (int)strlen(post);
        if (plen <= 0 || plen > 18) return -1;
        for (int i = 0; i < plen; i++) if (post[i] < '0' || post[i] > '9') return -1;
        rend = strtoll(post, NULL, 10);
    }

    if (rstart > rend || rstart >= size) return -2;
    if (rend >= size) rend = size - 1;
    *start = rstart;
    *end = rend;
    return 1;
}

static void serve_file(int c, const char *file, const char *range_hdr, int is_head) {
    long long size;
    if (file_get_size(file, &size) != 0) {
        send_404(c);
        return;
    }

    long long start = 0, end = size - 1;
    int status = 200;
    const char *status_text = "OK";
    char range_extra[96] = {0};

    if (range_hdr && range_hdr[0] && size > 0) {
        int r = parse_range(range_hdr, size, &start, &end);
        if (r == -2) {
            send_plain(c, 416, "Range Not Satisfiable", "application/json",
                       "{\"error\":\"Range not satisfiable\"}", -1);
            return;
        } else if (r == 1) {
            status = 206;
            status_text = "Partial Content";
            snprintf(range_extra, sizeof(range_extra),
                     "Content-Range: bytes %lld-%lld/%lld\r\n", start, end, size);
        }
    }

    long long clen = end - start + 1;
    char header[1024];
    int hlen = snprintf(header, sizeof(header),
                        "HTTP/1.1 %d %s\r\n"
                        "Content-Type: %s\r\n"
                        "Content-Length: %lld\r\n"
                        "%s"
                        "Accept-Ranges: bytes\r\n"
                        "Access-Control-Allow-Origin: *\r\n"
                        "Cache-Control: public, max-age=%d\r\n"
                        "Connection: close\r\n\r\n",
                        status, status_text, get_mime(file), clen,
                        range_extra, g_cache_ttl);
    if (hlen > 0) send_all(c, header, hlen);

    if (is_head) return;

    FILE *f = fopen(file, "rb");
    if (!f) return;
    if (start > 0) file_seek_set(f, start);

    char buf[32768];
    long long remaining = clen;
    while (remaining > 0) {
        int want = remaining > (long long)sizeof(buf) ? (int)sizeof(buf) : (int)remaining;
        int got = (int)fread(buf, 1, (size_t)want, f);
        if (got <= 0) break;
        send_all(c, buf, got);
        remaining -= got;
    }
    fclose(f);
}

static void serve(int c, const char *path, const char *range_hdr, int is_head) {
    char clean[512];
    snprintf(clean, sizeof(clean), "%s", path);
    char *q = strchr(clean, '?');
    if (q) *q = '\0';
    char *h = strchr(clean, '#');
    if (h) *h = '\0';

    if (strcmp(clean, "/") == 0 || clean[0] == '\0') {
        char body[256];
        int blen = snprintf(body, sizeof(body),
                            "{\"message\":\"ALRI CDN\",\"status\":\"active\",\"entries\":%d}", g_entry_count);
        send_plain(c, 200, "OK", "application/json", body, blen);
        return;
    }

    if (!is_valid_route_path(clean)) {
        send_plain(c, 400, "Bad Request", "application/json", "{\"error\":\"Invalid path\"}", -1);
        return;
    }

    ar_mutex_lock(g_mutex);
    CdnEntry *e = entry_find(clean);
    char file[1536] = {0};
    if (e) snprintf(file, sizeof(file), "%s", e->file);
    ar_mutex_unlock(g_mutex);

    if (!file[0]) {
        send_404(c);
        return;
    }

    serve_file(c, file, range_hdr, is_head);
}

static void handle_client(int c) {
    home_os_set_recv_timeout(c, 5000);

    char buf[MAX_REQ];
    int total = 0;
    int header_end = -1;

    while (total < MAX_REQ - 1) {
        int n = ar_socket_recv(c, buf + total, (size_t)(MAX_REQ - 1 - total));
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';

        char *end = strstr(buf, "\r\n\r\n");
        if (end) {
            header_end = (int)(end - buf) + 4;
            break;
        }
    }

    if (header_end < 0 || total <= 0) {
        ar_socket_close(c);
        return;
    }

    char method[16] = {0};
    char path[512] = {0};
    sscanf(buf, "%15s %511s", method, path);

    const char *range_hdr = find_header_value(buf, header_end, "Range");
    int is_head = (ci_cmp_n(method, "HEAD", 4) == 0);

    serve(c, path, range_hdr, is_head);
    ar_socket_close(c);
}

static void *client_thread(void *arg) {
    int c = (int)(intptr_t)arg;
    handle_client(c);
    return NULL;
}

static int create_server(int port) {
    int s = ar_socket_create(1);
    if (s < 0) {
        printf("[%s] cannot create socket\n", APP_NAME);
        return -1;
    }

    ar_socket_reuseaddr(s, 1);

    if (ar_socket_bind(s, "127.0.0.1", (uint16_t)port) < 0) {
        printf("[%s] cannot bind 127.0.0.1:%d\n", APP_NAME, port);
        ar_socket_close(s);
        return -1;
    }

    if (ar_socket_listen(s, 64) < 0) {
        printf("[%s] listen failed\n", APP_NAME);
        ar_socket_close(s);
        return -1;
    }

    return s;
}

static void run_server(int srv) {
    while (1) {
        int c = ar_socket_accept(srv);
        if (c < 0) continue;

        void *worker = ar_thread_create(client_thread, (void *)(intptr_t)c);
        if (worker) {
            ar_thread_detach(worker);
        } else {
            ar_socket_close(c);
        }
    }
}

/* ------------------------------------------------------------------ */

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);

    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--port=", 7) == 0) {
            server_port = atoi(argv[i] + 7);
            if (server_port <= 0) server_port = 3005;
        }
    }

    g_mutex = ar_mutex_create();

    get_cfg_path();
    cfg_load();
    printf("[%s] cdn.cfg: %s (entries=%d cache_ttl=%d)\n",
           APP_NAME, g_cfg_path, g_entry_count, g_cache_ttl);

    int srv = create_server(server_port);
    if (srv < 0) return 1;

    printf("[%s] CDN server listening on 127.0.0.1:%d\n", APP_NAME, server_port);

    void *rt = ar_thread_create(register_thread, NULL);
    if (rt) ar_thread_detach(rt);

    run_server(srv);
    return 0;
}
