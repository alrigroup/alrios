/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_config.h"
#include "arws_gateway.h"
#include "server.h"
#include "cJSON.h"
#include "log.h"
#include "aros_hal.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

#ifdef _WIN32
#include <direct.h>
#include <windows.h>
#include <sys/stat.h>
#include <sys/types.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

#define MAX_LINE 512

typedef struct {
    char host[128];
    char path[128];
    char mode[32];
    int no_cache;
} ModeOverride;

static ArwsMode cached_mode = -1;
static char config_bind[64] = "";
static int config_port = 0;
static int config_loaded = 0;

static char config_path[256] = "";
static char global_mode[16] = "production";
static int cache_ttl_config = 60;
static ModeOverride overrides[MAX_OVERRIDES];
static int override_count = 0;

typedef struct {
    char host[128];
    char path[128];
    char target_url[256];
} ProxyRouteEntry;

static ProxyRouteEntry proxy_routes[MAX_OVERRIDES];
static int proxy_route_count = 0;
static ProxyRouteEntry stream_routes[MAX_OVERRIDES];
static int stream_route_count = 0;
static char maintenance_ips[MAX_MAINTENANCE_IPS][64];
static int maintenance_ip_count = 0;
static void *config_mutex = NULL;

static char cfg_header[1024] = "# ALRI Web Services Config\n";
static long last_config_mtime = 0;
static volatile int config_watchdog_running = 0;
static int config_watchdog_started = 0;

#define CONFIG_POLL_MS 2000

static void arws_config_watchdog_start(void);

static void ensure_parent_dir(const char *path) {
    if (!path || !path[0]) return;

    char dir[512];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (!slash) return;
    *slash = '\0';
    if (dir[0] == '\0') return;

    for (char *p = dir + 1; *p; ++p) {
        if (*p == '/') {
            *p = '\0';
#ifdef _WIN32
            _mkdir(dir);
#else
            mkdir(dir, 0755);
#endif
            *p = '/';
        }
    }

#ifdef _WIN32
    _mkdir(dir);
#else
    mkdir(dir, 0755);
#endif
}

static void resolve_config_path(const char *requested_path, char *out, size_t out_size) {
    const char *candidates[3] = {0};
    int count = 0;

    if (requested_path && requested_path[0]) {
        candidates[count++] = requested_path;
    }
    candidates[count++] = "arcore/storage/arws/arws.cfg";
    candidates[count++] = "storage/arws/arws.cfg";

    for (int i = 0; i < count; i++) {
        const char *candidate = candidates[i];
        if (!candidate || !candidate[0]) continue;
        FILE *f = fopen(candidate, "r");
        if (f) {
            fclose(f);
            snprintf(out, out_size, "%s", candidate);
            return;
        }
    }

    snprintf(out, out_size, "%s", requested_path && requested_path[0] ? requested_path : "arcore/storage/arws/arws.cfg");
}

static long file_mtime(const char *path) {
    if (!path || !path[0]) return 0;
#ifdef _WIN32
    struct _stat st;
    if (_stat(path, &st) != 0)
        return 0;
#else
    struct stat st;
    if (stat(path, &st) != 0)
        return 0;
#endif
    return (long)st.st_mtime;
}

static void fix_owner(const char *path) {
#ifdef _WIN32
    (void)path;
#else
    if (geteuid() != 0) return;
    const char *su = getenv("SUDO_UID");
    const char *sg = getenv("SUDO_GID");
    if (!su || !sg) return;
    uid_t uid = (uid_t)atol(su);
    gid_t gid = (gid_t)atol(sg);
    chown(path, uid, gid);
    char dir[512];
    snprintf(dir, sizeof(dir), "%s", path);
    char *slash = strrchr(dir, '/');
    if (slash) {
        *slash = '\0';
        if (dir[0]) chown(dir, uid, gid);
    }
#endif
}

static void trim(char *s) {
    if (!s || !*s) return;
    char *e = s + strlen(s) - 1;
    while (e >= s && (*e == ' ' || *e == '\t' || *e == '\r' || *e == '\n')) *e-- = '\0';
    char *start = s;
    while (*start && (*start == ' ' || *start == '\t')) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
}

static int path_matches_pattern(const char *path, const char *pattern) {
    if (!path || !pattern) return 0;
    int plen = (int)strlen(pattern);
    if (plen > 0 && pattern[plen - 1] == '*') {
        return strncmp(path, pattern, plen - 1) == 0;
    }
    return strcmp(path, pattern) == 0;
}

static int is_loopback_addr(const char *h) {
    if (!h || !h[0]) return 0;
    if (strcasecmp(h, "localhost") == 0) return 1;
    if (strcasecmp(h, "0.0.0.0") == 0) return 1;
    if (strncmp(h, "127.", 4) == 0) return 1;
    if (strcasecmp(h, "::1") == 0 || strcasecmp(h, "[::1]") == 0) return 1;
    return 0;
}

static int host_matches(const char *pattern, const char *host) {
    if (!pattern || !host) return 0;
    if (pattern[0] == '\0' || strcmp(pattern, "*") == 0) return 1;
    if (strcasecmp(pattern, host) == 0) return 1;
    if (is_loopback_addr(pattern) && is_loopback_addr(host)) return 1;

    const char *p_dot = strchr(pattern, '.');
    const char *h_dot = strchr(host, '.');
    if (p_dot && h_dot) {
        int p_len = (int)(p_dot - pattern);
        int h_len = (int)(h_dot - host);
        if (p_len == h_len && strncasecmp(pattern, host, p_len) == 0) {
            if (is_loopback_addr(p_dot + 1) && is_loopback_addr(h_dot + 1)) {
                return 1;
            }
        }
    }

    if (strncasecmp(host, "www.", 4) == 0 && strcasecmp(pattern, host + 4) == 0) return 1;
    return 0;
}

static void write_guide_comments(FILE *f) {
    fprintf(f, "# ALRI Web Services Config\n");
    fprintf(f, "#\n");
    fprintf(f, "# mode: server binding mode\n");
    fprintf(f, "#   test       - HTTP on 127.0.0.1:8080 (development)\n");
    fprintf(f, "#   production - HTTPS on 0.0.0.0:443 (live)\n");
    fprintf(f, "#\n");
    fprintf(f, "# port: TCP port to listen on\n");
    fprintf(f, "#   default: 8080 (test), 443 (production)\n");
    fprintf(f, "#\n");
    fprintf(f, "# bind: IP address to bind to\n");
    fprintf(f, "#   default: 127.0.0.1 (test), 0.0.0.0 (production)\n");
    fprintf(f, "#\n");
    fprintf(f, "# global_mode: default response mode for all endpoints\n");
    fprintf(f, "#   test        - normal operation (development)\n");
    fprintf(f, "#   production  - normal operation (live)\n");
    fprintf(f, "#   maintenance - returns 503 for all requests\n");
    fprintf(f, "#\n");
    fprintf(f, "# maintenance_ips: IPs allowed through during maintenance\n");
    fprintf(f, "#   maintenance_ips = [\"127.0.0.1\", \"192.168.1.100\"]\n");
    fprintf(f, "#\n");
    fprintf(f, "# Per-endpoint mode overrides:\n");
    fprintf(f, "#   Format: \"host/path\" = mode\n");
    fprintf(f, "#   \"example.com/api/admin\" = maintenance\n");
    fprintf(f, "#   \"example.com/health\" = production\n");
    fprintf(f, "#   \"*//public\" = test\n");
    fprintf(f, "#\n");
    fprintf(f, "# cache_ttl: cache lifetime in seconds for static/GET responses\n");
    fprintf(f, "#   0       - disable caching\n");
    fprintf(f, "#   N       - cache responses for N seconds (default: 60)\n");
    fprintf(f, "#\n");
    fprintf(f, "# Cache control per endpoint (append no-cache to override):\n");
    fprintf(f, "#   \"detroitgg.alrigroup.com/minha-conta\" = production no-cache\n");
    fprintf(f, "#   \"detroitgg.alrigroup.com/dynamic\" = no-cache\n");
    fprintf(f, "#\n");
    fprintf(f, "# By default, static files (.css/.js/.png/.jpg/etc) are always cached.\n");
    fprintf(f, "# Other routes are cached only if cache_ttl > 0 and no \"no-cache\" flag.\n");
    fprintf(f, "#\n");
    fprintf(f, "# Reverse proxy: forward requests directly to an HTTP backend\n");
    fprintf(f, "#   \"detroitgg.alrigroup.com/*\" = \"http://127.0.0.1:3001\"\n");
    fprintf(f, "#   \"home.alrigroup.com/*\" = \"http://127.0.0.1:3002\"\n");
    fprintf(f, "#\n");
    fprintf(f, "# Stream proxy: real-time bidirectional pipe (WebSocket, SSE, chunked, media)\n");
    fprintf(f, "#   \"site.com/live/*\" = stream \"http://192.168.1.100:8080\"\n");
    fprintf(f, "#   \"site.com/ws/*\" = stream \"ws://192.168.1.100:9000\"\n");
    fprintf(f, "#\n");
    fprintf(f, "# Mode overrides also apply to proxy and stream routes: add a line\n");
    fprintf(f, "#   \"site.com/live/*\" = maintenance\n");
    fprintf(f, "# to force that route to production/test/maintenance. Apps auto-register\n");
    fprintf(f, "# an override (global_mode) next to their stream/proxy routes on startup.\n");
}

static int replace_file(const char *tmp, const char *dest) {
#ifdef _WIN32
    if (MoveFileExA(tmp, dest, MOVEFILE_REPLACE_EXISTING))
        return 0;
    return -1;
#else
    if (rename(tmp, dest) != 0)
        return -1;
    return 0;
#endif
}

static int save_config(void) {
    if (config_path[0] == '\0') return -1;

    char tmp_path[512];
    snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", config_path);

    FILE *f = fopen(tmp_path, "w");
    if (!f) return -1;

    write_guide_comments(f);

    if (cached_mode == ARWS_MODE_PRODUCTION)
        fprintf(f, "mode=production\n");
    else if (cached_mode == ARWS_MODE_TEST)
        fprintf(f, "mode=test\n");
    else
        fprintf(f, "mode=test\n");

    if (config_port > 0)
        fprintf(f, "port=%d\n", config_port);

    if (config_bind[0])
        fprintf(f, "bind=%s\n", config_bind);

    if (maintenance_ip_count > 0) {
        fprintf(f, "maintenance_ips = [");
        for (int i = 0; i < maintenance_ip_count; i++) {
            if (i > 0) fprintf(f, ",");
            fprintf(f, "\"%s\"", maintenance_ips[i]);
        }
        fprintf(f, "]\n");
    }

    fprintf(f, "global_mode=%s\n", global_mode);
    fprintf(f, "cache_ttl=%d\n", cache_ttl_config);

    for (int i = 0; i < override_count; i++) {
        if (overrides[i].mode[0]) {
            fprintf(f, "\"%s/%s\" = %s%s\n",
                    overrides[i].host, overrides[i].path, overrides[i].mode,
                    overrides[i].no_cache ? " no-cache" : "");
        } else if (overrides[i].no_cache) {
            fprintf(f, "\"%s/%s\" = no-cache\n",
                    overrides[i].host, overrides[i].path);
        }
    }

    for (int i = 0; i < proxy_route_count; i++) {
        fprintf(f, "\"%s/%s\" = %s\n",
                proxy_routes[i].host, proxy_routes[i].path,
                proxy_routes[i].target_url);
    }

    for (int i = 0; i < stream_route_count; i++) {
        fprintf(f, "\"%s/%s\" = stream %s\n",
                stream_routes[i].host, stream_routes[i].path,
                stream_routes[i].target_url);
    }

    fclose(f);

    if (replace_file(tmp_path, config_path) != 0) {
        remove(tmp_path);
        return -1;
    }
    fix_owner(config_path);
    return 0;
}

int arws_config_load(const char *path) {
    if (!config_mutex)
        config_mutex = ar_mutex_create();

    ar_mutex_lock(config_mutex);

    char resolved_path[512];
    resolve_config_path(path, resolved_path, sizeof(resolved_path));
    strncpy(config_path, resolved_path, sizeof(config_path) - 1);

    FILE *f = fopen(resolved_path, "r");
    if (!f) {
        ensure_parent_dir(resolved_path);
        f = fopen(resolved_path, "w");
        if (f) {
            write_guide_comments(f);
            fprintf(f, "mode=production\n");
            fprintf(f, "port=443\n");
            fprintf(f, "bind=0.0.0.0\n");
            fprintf(f, "global_mode=production\n");
            fclose(f);
        }
        f = fopen(resolved_path, "r");
        if (!f) { ar_mutex_unlock(config_mutex); return -1; }
    }

    fix_owner(resolved_path);

    cached_mode = (ArwsMode)-1;
    config_port = 0;
    config_bind[0] = '\0';
    override_count = 0;
    proxy_route_count = 0;
    stream_route_count = 0;
    maintenance_ip_count = 0;
    cache_ttl_config = 60;
    strncpy(global_mode, "production", sizeof(global_mode) - 1);

    char line[MAX_LINE];
    int first_line = 1;
    while (fgets(line, sizeof(line), f)) {
        if (first_line && line[0] == '#') {
            strncpy(cfg_header, line, sizeof(cfg_header) - 1);
            char *nl = strchr(cfg_header, '\n');
            if (nl) *(nl + 1) = '\0';
            first_line = 0;
        }

        trim(line);
        if (line[0] == '#' || line[0] == '\0') continue;

        if (line[0] == '"') {
            char *close = strchr(line + 1, '"');
            if (!close) continue;
            *close = '\0';
            char *key = line + 1;

            char *eq = strchr(close + 1, '=');
            if (!eq) continue;
            eq++;
            trim(eq);

            char val[512];
            strncpy(val, eq, sizeof(val) - 1);
            val[sizeof(val) - 1] = '\0';
            trim(val);
            int vlen = (int)strlen(val);
            if (vlen >= 2 && val[0] == '"' && val[vlen - 1] == '"') {
                val[vlen - 1] = '\0';
                memmove(val, val + 1, strlen(val));
            }

            if (override_count < MAX_OVERRIDES) {
                char *slash = strchr(key, '/');
                if (slash) {
                    *slash = '\0';
                    char *host = key;
                    char *path = slash + 1;

                    int is_stream = 0;
                    char stream_url[512] = "";
                    if (strncmp(val, "stream ", 7) == 0) {
                        is_stream = 1;
                        strncpy(stream_url, val + 7, sizeof(stream_url) - 1);
                        stream_url[sizeof(stream_url) - 1] = '\0';
                        char *nl = strchr(stream_url, '\n');
                        if (nl) *nl = '\0';
                    }

                    const char *target = is_stream ? stream_url : val;

                    if (strncmp(target, "http://", 7) == 0 || strncmp(target, "https://", 8) == 0
                        || strncmp(target, "ws://", 5) == 0 || strncmp(target, "wss://", 6) == 0) {

                        if (is_stream) {
                            arws_add_stream_route(path, "*", host,
                                                  arws_config_get_global_mode(), target);
                            if (stream_route_count < MAX_OVERRIDES) {
                                strncpy(stream_routes[stream_route_count].host, host,
                                        sizeof(stream_routes[0].host) - 1);
                                strncpy(stream_routes[stream_route_count].path, path,
                                        sizeof(stream_routes[0].path) - 1);
                                strncpy(stream_routes[stream_route_count].target_url, target,
                                        sizeof(stream_routes[0].target_url) - 1);
                                stream_route_count++;
                            }
                        } else {
                            arws_add_proxy_route(path, "*", host,
                                                 arws_config_get_global_mode(), target);
                            if (proxy_route_count < MAX_OVERRIDES) {
                                strncpy(proxy_routes[proxy_route_count].host, host,
                                        sizeof(proxy_routes[0].host) - 1);
                                strncpy(proxy_routes[proxy_route_count].path, path,
                                        sizeof(proxy_routes[0].path) - 1);
                                strncpy(proxy_routes[proxy_route_count].target_url, target,
                                        sizeof(proxy_routes[0].target_url) - 1);
                                proxy_route_count++;
                            }
                        }
                    } else {
                        int idx = override_count;
                        for (int i = 0; i < override_count; i++) {
                            if (strcmp(overrides[i].host, host) == 0 &&
                                strcmp(overrides[i].path, path) == 0) {
                                idx = i;
                                break;
                            }
                        }

                        strncpy(overrides[idx].host, host,
                                sizeof(overrides[0].host) - 1);
                        strncpy(overrides[idx].path, path,
                                sizeof(overrides[0].path) - 1);
                        overrides[idx].no_cache = 0;

                        char value[256];
                        strncpy(value, val, sizeof(value) - 1);
                        char *flags = strchr(value, ' ');
                        if (flags) {
                            *flags++ = '\0';
                            trim(flags);
                            if (strcasecmp(flags, "no-cache") == 0 || strcasecmp(flags, "no_cache") == 0)
                                overrides[idx].no_cache = 1;
                        }
                        strncpy(overrides[idx].mode, value, sizeof(overrides[0].mode) - 1);

                        if (strcasecmp(value, "no-cache") == 0 || strcasecmp(value, "no_cache") == 0) {
                            overrides[idx].mode[0] = '\0';
                            overrides[idx].no_cache = 1;
                        }

                        if (idx == override_count)
                            override_count++;
                    }
                }
            }
            continue;
        }

        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq++ = '\0';
        trim(line);
        trim(eq);

        if (strcasecmp(line, "mode") == 0) {
            if (strcasecmp(eq, "production") == 0)
                cached_mode = ARWS_MODE_PRODUCTION;
            else if (strcasecmp(eq, "test") == 0)
                cached_mode = ARWS_MODE_TEST;
        } else if (strcasecmp(line, "global_mode") == 0) {
            strncpy(global_mode, eq, sizeof(global_mode) - 1);
            global_mode[sizeof(global_mode) - 1] = '\0';
        } else if (strcasecmp(line, "port") == 0) {
            int p = atoi(eq);
            if (p > 0 && p <= 65535) config_port = p;
        } else if (strcasecmp(line, "bind") == 0) {
            strncpy(config_bind, eq, sizeof(config_bind) - 1);
        } else if (strcasecmp(line, "cache_ttl") == 0) {
            int v = atoi(eq);
            if (v >= 0) cache_ttl_config = v;
        } else if (strcasecmp(line, "maintenance_ips") == 0) {
            cJSON *arr = cJSON_Parse(eq);
            if (cJSON_IsArray(arr)) {
                maintenance_ip_count = 0;
                cJSON *item;
                cJSON_ArrayForEach(item, arr) {
                    if (cJSON_IsString(item) && maintenance_ip_count < MAX_MAINTENANCE_IPS) {
                        strncpy(maintenance_ips[maintenance_ip_count], item->valuestring,
                                sizeof(maintenance_ips[0]) - 1);
                        maintenance_ip_count++;
                    }
                }
            }
            if (arr) cJSON_Delete(arr);
        }
    }
    fclose(f);
    config_loaded = 1;
    last_config_mtime = file_mtime(resolved_path);

    alri_print(CYN "[ARWS]" RST " Config loaded: path=%s mode=%s port=%d bind=%s global=%s overrides=%d\n",
               config_path,
               cached_mode == ARWS_MODE_PRODUCTION ? "PRODUCTION" : "TEST",
               config_port, config_bind, global_mode, override_count);

    ar_mutex_unlock(config_mutex);

    arws_config_watchdog_start();
    return 0;
}

const char* arws_config_get_path(void) {
    if (!config_mutex) return config_path[0] != '\0' ? config_path : "storage/arws/arws.cfg";
    ar_mutex_lock(config_mutex);
    const char *ret = config_path[0] != '\0' ? config_path : "storage/arws/arws.cfg";
    ar_mutex_unlock(config_mutex);
    return ret;
}

int arws_config_reload_from_disk(void) {
    if (config_path[0] == '\0') return -1;
    return arws_config_load(config_path);
}

static void *config_watchdog_loop(void *arg) {
    (void)arg;
    while (config_watchdog_running) {
        ar_sleep_ms(CONFIG_POLL_MS);

        if (config_path[0] == '\0') continue;

        long mtime = file_mtime(config_path);
        if (mtime != 0 && mtime != last_config_mtime) {
            alri_print(CYN "[ARWS]" RST " Config changed on disk, reloading...\n");
            arws_config_load(config_path);
        }
    }
    return NULL;
}

static void arws_config_watchdog_start(void) {
    if (config_watchdog_started) return;
    config_watchdog_started = 1;
    config_watchdog_running = 1;

    void *th = ar_thread_create(config_watchdog_loop, NULL);
    if (th) ar_thread_detach(th);
    alri_print_force(CYN "[ARWS]" RST " Config watchdog started (poll every %d ms)\n",
                     CONFIG_POLL_MS);
}

void arws_config_watchdog_stop(void) {
    config_watchdog_running = 0;
}

static ArwsMode detect_mode(void) {
    if (cached_mode != (ArwsMode)-1)
        return cached_mode;

    cached_mode = ARWS_MODE_TEST;

    if (!config_loaded)
        arws_config_load("storage/arws/arws.cfg");

    if (cached_mode != (ArwsMode)-1 && cached_mode != ARWS_MODE_TEST)
        return cached_mode;

    const char *env = getenv("ARWS_MODE");
    if (env) {
        if (strcasecmp(env, "production") == 0)
            return ARWS_MODE_PRODUCTION;
        if (strcasecmp(env, "test") == 0)
            return ARWS_MODE_TEST;
    }

    return cached_mode != (ArwsMode)-1 ? cached_mode : ARWS_MODE_TEST;
}

ArwsMode arws_config_get_mode(void) {
    if (cached_mode == (ArwsMode)-1)
        cached_mode = detect_mode();
    return cached_mode;
}

int arws_config_get_port(void) {
    if (config_port > 0) return config_port;

    const char *env = getenv("ARWS_PORT");
    if (env) {
        int p = atoi(env);
        if (p > 0 && p <= 65535) return p;
    }

    if (arws_config_get_mode() == ARWS_MODE_PRODUCTION)
        return 443;
    return 80;
}

void arws_config_set_port(int p) {
    config_port = p;
}

int arws_config_get_operation_mode(void) {
    if (arws_config_get_mode() == ARWS_MODE_PRODUCTION) {
        if (config_port > 0 && config_port != 443)
            return MODE_INSECURE;
        return MODE_SECURE;
    }
    return MODE_INSECURE;
}

const char* arws_config_get_mode_name(void) {
    return arws_config_get_mode() == ARWS_MODE_PRODUCTION
        ? "PRODUCTION"
        : "TEST";
}

const char* arws_config_get_bind_address(void) {
    if (!config_mutex) {
        if (config_bind[0] != '\0') return config_bind;
    } else {
        ar_mutex_lock(config_mutex);
        int has_bind = (config_bind[0] != '\0');
        const char *ret = has_bind ? config_bind : NULL;
        ar_mutex_unlock(config_mutex);
        if (ret) return ret;
    }
    if (arws_config_get_mode() == ARWS_MODE_PRODUCTION)
        return "0.0.0.0";
    return "127.0.0.1";
}

const char* arws_config_get_effective_mode(const char *host, const char *path, const char *client_ip) {
    ar_mutex_lock(config_mutex);

    if (strcmp(global_mode, MODE_MAINTENANCE) == 0) {
        if (client_ip) {
            for (int j = 0; j < maintenance_ip_count; j++) {
                if (strcmp(maintenance_ips[j], client_ip) == 0) {
                    ar_mutex_unlock(config_mutex);
                    return MODE_PRODUCTION;
                }
            }
        }
        ar_mutex_unlock(config_mutex);
        return MODE_MAINTENANCE;
    }

    for (int i = 0; i < override_count; i++) {
        if (host_matches(overrides[i].host, host) &&
            path_matches_pattern(path, overrides[i].path)) {
            const char *ret = overrides[i].mode;
            if (strcmp(ret, MODE_MAINTENANCE) == 0 && client_ip) {
                for (int j = 0; j < maintenance_ip_count; j++) {
                    if (strcmp(maintenance_ips[j], client_ip) == 0) {
                        ar_mutex_unlock(config_mutex);
                        return MODE_PRODUCTION;
                    }
                }
            }
            ar_mutex_unlock(config_mutex);
            return ret;
        }
    }

    ar_mutex_unlock(config_mutex);

    return global_mode;
}

int arws_config_is_maintenance_ip(const char *client_ip) {
    if (!client_ip) return 0;
    ar_mutex_lock(config_mutex);
    for (int i = 0; i < maintenance_ip_count; i++) {
        if (strcmp(maintenance_ips[i], client_ip) == 0) {
            ar_mutex_unlock(config_mutex);
            return 1;
        }
    }
    ar_mutex_unlock(config_mutex);
    return 0;
}

int arws_config_has_override(const char *host, const char *path) {
    if (!host || !path) return 0;
    for (int i = 0; i < override_count; i++) {
        if (strcmp(overrides[i].host, host) == 0 &&
            strcmp(overrides[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

int arws_config_add_override(const char *host, const char *path, const char *mode) {
    if (!host || !path || !mode) return -1;

    ar_mutex_lock(config_mutex);

    if (arws_config_has_override(host, path)) {
        for (int i = 0; i < override_count; i++) {
            if (strcmp(overrides[i].host, host) == 0 &&
                strcmp(overrides[i].path, path) == 0) {
                strncpy(overrides[i].mode, mode, sizeof(overrides[i].mode) - 1);
                save_config();
                ar_mutex_unlock(config_mutex);
                return 0;
            }
        }
    }

    if (override_count >= MAX_OVERRIDES) {
        ar_mutex_unlock(config_mutex);
        return -1;
    }

    strncpy(overrides[override_count].host, host, sizeof(overrides[0].host) - 1);
    strncpy(overrides[override_count].path, path, sizeof(overrides[0].path) - 1);
    strncpy(overrides[override_count].mode, mode, sizeof(overrides[0].mode) - 1);
    override_count++;

    save_config();
    ar_mutex_unlock(config_mutex);
    return 0;
}

int arws_config_remove_override(const char *host, const char *path) {
    if (!host || !path) return -1;

    ar_mutex_lock(config_mutex);
    for (int i = 0; i < override_count; i++) {
        if (strcmp(overrides[i].host, host) == 0 &&
            strcmp(overrides[i].path, path) == 0) {
            overrides[i] = overrides[--override_count];
            save_config();
            ar_mutex_unlock(config_mutex);
            return 0;
        }
    }
    ar_mutex_unlock(config_mutex);
    return -1;
}

int arws_config_set_global(const char *mode) {
    if (!mode) return -1;
    if (strcmp(mode, MODE_PRODUCTION) != 0 &&
        strcmp(mode, MODE_TEST) != 0 &&
        strcmp(mode, MODE_MAINTENANCE) != 0) return -1;

    ar_mutex_lock(config_mutex);
    strncpy(global_mode, mode, sizeof(global_mode) - 1);
    global_mode[sizeof(global_mode) - 1] = '\0';
    save_config();
    ar_mutex_unlock(config_mutex);
    return 0;
}

const char* arws_config_get_global_mode(void) {
    return global_mode;
}

int arws_config_get_cache_ttl(void) {
    int ttl;
    ar_mutex_lock(config_mutex);
    ttl = cache_ttl_config;
    ar_mutex_unlock(config_mutex);
    return ttl;
}

int arws_config_set_cache_ttl(int seconds) {
    if (seconds < 0) return -1;
    ar_mutex_lock(config_mutex);
    cache_ttl_config = seconds;
    save_config();
    ar_mutex_unlock(config_mutex);
    return 0;
}

int arws_config_add_proxy_route(const char *host, const char *path, const char *target_url) {
    if (!host || !path || !target_url) return -1;

    ar_mutex_lock(config_mutex);

    if (arws_config_has_proxy_route(host, path)) {
        for (int i = 0; i < proxy_route_count; i++) {
            if (strcmp(proxy_routes[i].host, host) == 0 &&
                strcmp(proxy_routes[i].path, path) == 0) {
                strncpy(proxy_routes[i].target_url, target_url,
                        sizeof(proxy_routes[0].target_url) - 1);
                save_config();
                ar_mutex_unlock(config_mutex);
                return 0;
            }
        }
    }

    if (proxy_route_count >= MAX_OVERRIDES) {
        ar_mutex_unlock(config_mutex);
        return -1;
    }

    strncpy(proxy_routes[proxy_route_count].host, host,
            sizeof(proxy_routes[0].host) - 1);
    strncpy(proxy_routes[proxy_route_count].path, path,
            sizeof(proxy_routes[0].path) - 1);
    strncpy(proxy_routes[proxy_route_count].target_url, target_url,
            sizeof(proxy_routes[0].target_url) - 1);
    proxy_route_count++;

    save_config();
    ar_mutex_unlock(config_mutex);
    return 0;
}

int arws_config_has_proxy_route(const char *host, const char *path) {
    if (!host || !path) return 0;
    for (int i = 0; i < proxy_route_count; i++) {
        if (strcmp(proxy_routes[i].host, host) == 0 &&
            strcmp(proxy_routes[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

int arws_config_add_stream_route(const char *host, const char *path, const char *target_url) {
    if (!host || !path || !target_url) return -1;

    ar_mutex_lock(config_mutex);

    if (arws_config_has_stream_route(host, path)) {
        for (int i = 0; i < stream_route_count; i++) {
            if (strcmp(stream_routes[i].host, host) == 0 &&
                strcmp(stream_routes[i].path, path) == 0) {
                strncpy(stream_routes[i].target_url, target_url,
                        sizeof(stream_routes[0].target_url) - 1);
                save_config();
                ar_mutex_unlock(config_mutex);
                return 0;
            }
        }
    }

    if (stream_route_count >= MAX_OVERRIDES) {
        ar_mutex_unlock(config_mutex);
        return -1;
    }

    strncpy(stream_routes[stream_route_count].host, host,
            sizeof(stream_routes[0].host) - 1);
    strncpy(stream_routes[stream_route_count].path, path,
            sizeof(stream_routes[0].path) - 1);
    strncpy(stream_routes[stream_route_count].target_url, target_url,
            sizeof(stream_routes[0].target_url) - 1);
    stream_route_count++;

    save_config();
    ar_mutex_unlock(config_mutex);
    return 0;
}

int arws_config_has_stream_route(const char *host, const char *path) {
    if (!host || !path) return 0;
    for (int i = 0; i < stream_route_count; i++) {
        if (strcmp(stream_routes[i].host, host) == 0 &&
            strcmp(stream_routes[i].path, path) == 0) {
            return 1;
        }
    }
    return 0;
}

int arws_config_is_no_cache(const char *host, const char *path) {
    if (!host || !path) return 0;
    ar_mutex_lock(config_mutex);
    for (int i = 0; i < override_count; i++) {
        if (host_matches(overrides[i].host, host) &&
            path_matches_pattern(path, overrides[i].path)) {
            int nc = overrides[i].no_cache;
            ar_mutex_unlock(config_mutex);
            return nc;
        }
    }
    ar_mutex_unlock(config_mutex);
    return 0;
}

int arws_config_dump_routes(char *out, int size) {
    if (!out || size <= 0) return 0;
    int used = 0;
    used += snprintf(out + used, size - used, "global_mode=%s\n", global_mode);

    for (int i = 0; i < proxy_route_count; i++) {
        int n = snprintf(out + used, size - used, "proxy   host=%-28s path=%-28s -> %s\n",
                         proxy_routes[i].host, proxy_routes[i].path, proxy_routes[i].target_url);
        if (n < 0 || used + n >= size) break;
        used += n;
    }
    for (int i = 0; i < stream_route_count; i++) {
        int n = snprintf(out + used, size - used, "stream  host=%-28s path=%-28s -> %s\n",
                         stream_routes[i].host, stream_routes[i].path, stream_routes[i].target_url);
        if (n < 0 || used + n >= size) break;
        used += n;
    }
    for (int i = 0; i < override_count; i++) {
        int n = snprintf(out + used, size - used, "override host=%-28s path=%-28s mode=%s%s\n",
                         overrides[i].host, overrides[i].path,
                         overrides[i].mode[0] ? overrides[i].mode : "-",
                         overrides[i].no_cache ? " no-cache" : "");
        if (n < 0 || used + n >= size) break;
        used += n;
    }
    return used;
}
