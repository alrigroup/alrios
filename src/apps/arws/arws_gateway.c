/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_gateway.h"
#include "arws_utils.h"
#include "arws_config.h"
#include "arws_ratelimit.h"
#include "arws_session.h"
#include "arws_cache.h"
#include "arws_proxy.h"
#include "ipc/ipc.h"
#include "ar_ipc.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif

#define ARWS_ADMIN_PORT 9500

extern void arws_route_init(void);
extern void arws_registry_init(void);

static void handler_func(ClientConnection *conn, HttpRequest *req) {
    const char *xff = get_header(req, "X-Forwarded-For");
    const char *client_ip = server_get_client_ip(conn);
    char xff_buf[64];
    if (xff && xff[0]) {
        const char *s = xff;
        while (*s == ' ' || *s == '\t') s++;
        strncpy(xff_buf, s, sizeof(xff_buf) - 1);
        xff_buf[sizeof(xff_buf) - 1] = '\0';
        char *comma = strchr(xff_buf, ',');
        if (comma) *comma = '\0';
        if (xff_buf[0]) client_ip = xff_buf;
    }

    if (!arws_ratelimit_check(client_ip, req->host, req->path)) {
        arws_send_429(conn, "Rate limit exceeded");
        return;
    }

    const char *effective_mode = arws_config_get_effective_mode(req->host, req->path, client_ip);

    if (strcmp(effective_mode, MODE_MAINTENANCE) == 0) {
        arws_send_maintenance(conn);
        return;
    }

    arws_dispatch(conn, req, effective_mode);
}

static int gateway_running = 0;
static int admin_server_fd = -1;
static int admin_client_fds[AR_IPC_MAX_CLIENTS];
static int admin_client_count = 0;
static void *admin_mutex = NULL;
static void *admin_thread = NULL;
static void *accept_thread = NULL;

/* Internal name -> fd table so IPC_QUERY can target apps that register as
   proxy/stream routes (which are not backend-registered). */
static char named_client_names[AR_IPC_MAX_CLIENTS][64];
static int  named_client_fds[AR_IPC_MAX_CLIENTS];
static int  named_client_count = 0;

/* Per-fd query lock: prevents race when forwarding IPC_QUERY.
   Socket handles are arbitrary OS fds (large on Windows), so the
   lock array is grown to fit the fd instead of assuming small fds. */
static int *query_locks = NULL;
static int query_locks_max = 0;
static void *query_mutex = NULL;
static void *query_cond = NULL;

/* Per-backend dispatch lock: prevents interleaved HTTP req/resp on shared FD */
static int dispatch_locks[ARWS_MAX_BACKENDS];
static void *dispatch_mutex = NULL;
static void *dispatch_cond = NULL;

static int query_locks_ensure(int fd) {
    if (fd >= 0 && fd < query_locks_max) return 0;
    int new_max = (fd + 256) & ~255;
    if (new_max < 64) new_max = 64;
    int *nl = (int *)realloc(query_locks, (size_t)new_max * sizeof(int));
    if (!nl) return -1;
    if (new_max > query_locks_max)
        memset(nl + query_locks_max, 0, (size_t)(new_max - query_locks_max) * sizeof(int));
    query_locks = nl;
    query_locks_max = new_max;
    return 0;
}

static void query_lock_fd(int fd) {
    if (fd < 0) return;
    if (!query_mutex) query_mutex = ar_mutex_create();
    if (!query_cond) query_cond = ar_cond_create();

    ar_mutex_lock(query_mutex);
    while (gateway_running) {
        if (query_locks_ensure(fd) == 0 && query_locks[fd] == 0) {
            query_locks[fd] = fd;
            ar_mutex_unlock(query_mutex);
            return;
        }
        ar_cond_wait(query_cond, query_mutex);
    }
    ar_mutex_unlock(query_mutex);
}

static void query_unlock_fd(int fd) {
    if (fd < 0 || fd >= query_locks_max) return;
    if (!query_mutex) return;
    ar_mutex_lock(query_mutex);
    query_locks[fd] = 0;
    if (query_cond) ar_cond_signal(query_cond);
    ar_mutex_unlock(query_mutex);
}

void arws_dispatch_lock(int backend_id) {
    if (backend_id < 0 || backend_id >= ARWS_MAX_BACKENDS) return;
    if (!dispatch_mutex) dispatch_mutex = ar_mutex_create();
    if (!dispatch_cond) dispatch_cond = ar_cond_create();

    ar_mutex_lock(dispatch_mutex);
    while (gateway_running) {
        if (dispatch_locks[backend_id] == 0) {
            dispatch_locks[backend_id] = 1;
            ar_mutex_unlock(dispatch_mutex);
            return;
        }
        ar_cond_wait(dispatch_cond, dispatch_mutex);
    }
    ar_mutex_unlock(dispatch_mutex);
}

void arws_dispatch_unlock(int backend_id) {
    if (backend_id < 0 || backend_id >= ARWS_MAX_BACKENDS) return;
    if (!dispatch_mutex) return;
    ar_mutex_lock(dispatch_mutex);
    dispatch_locks[backend_id] = 0;
    if (dispatch_cond) ar_cond_signal(dispatch_cond);
    ar_mutex_unlock(dispatch_mutex);
}

static int is_query_locked(int fd) {
    int locked;
    ar_mutex_lock(query_mutex);
    locked = (fd >= 0 && fd < query_locks_max) ? query_locks[fd] : 0;
    ar_mutex_unlock(query_mutex);
    return locked;
}

static int add_client_fd(int fd) {
    ar_mutex_lock(admin_mutex);
    if (admin_client_count < AR_IPC_MAX_CLIENTS) {
        admin_client_fds[admin_client_count++] = fd;
        ar_mutex_unlock(admin_mutex);
        return 1;
    }
    ar_mutex_unlock(admin_mutex);
    return 0;
}

static void remove_client_fd(int fd) {
    ar_mutex_lock(admin_mutex);
    for (int i = 0; i < admin_client_count; i++) {
        if (admin_client_fds[i] == fd) {
            admin_client_fds[i] = admin_client_fds[--admin_client_count];
            break;
        }
    }
    ar_mutex_unlock(admin_mutex);
}

static void named_add(int fd, const char *name) {
    ar_mutex_lock(admin_mutex);
    for (int i = 0; i < named_client_count; i++) {
        if (named_client_fds[i] == fd) {
            strncpy(named_client_names[i], name, sizeof(named_client_names[0]) - 1);
            named_client_names[i][sizeof(named_client_names[0]) - 1] = '\0';
            ar_mutex_unlock(admin_mutex);
            return;
        }
    }
    if (named_client_count < AR_IPC_MAX_CLIENTS) {
        named_client_fds[named_client_count] = fd;
        strncpy(named_client_names[named_client_count], name, sizeof(named_client_names[0]) - 1);
        named_client_names[named_client_count][sizeof(named_client_names[0]) - 1] = '\0';
        named_client_count++;
    }
    ar_mutex_unlock(admin_mutex);
}

static int named_find(const char *name) {
    ar_mutex_lock(admin_mutex);
    for (int i = 0; i < named_client_count; i++) {
        if (strcmp(named_client_names[i], name) == 0) {
            int fd = named_client_fds[i];
            ar_mutex_unlock(admin_mutex);
            return fd;
        }
    }
    ar_mutex_unlock(admin_mutex);
    return -1;
}

static void named_remove(int fd) {
    ar_mutex_lock(admin_mutex);
    for (int i = 0; i < named_client_count; i++) {
        if (named_client_fds[i] == fd) {
            named_client_fds[i] = named_client_fds[--named_client_count];
            strncpy(named_client_names[i], named_client_names[named_client_count],
                    sizeof(named_client_names[0]) - 1);
            named_client_names[i][sizeof(named_client_names[0]) - 1] = '\0';
            break;
        }
    }
    ar_mutex_unlock(admin_mutex);
}

static int peek_is_ipc_frame(int fd) {
    fd_set rfds;
    struct timeval tv;
    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 100000;

    int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0) return 0;

    unsigned char peek[5];
    int r;
#ifdef _WIN32
    r = recv((SOCKET)fd, (char*)peek, 5, MSG_PEEK);
#else
    r = recv(fd, peek, 5, MSG_PEEK);
#endif
    if (r < 5) return 0;

    uint32_t frame_len = ((uint32_t)peek[0] << 24) |
                         ((uint32_t)peek[1] << 16) |
                         ((uint32_t)peek[2] << 8) |
                         ((uint32_t)peek[3]);
    int type = peek[4];

    return (frame_len <= AR_IPC_BUF_SIZE && type >= 1 && type <= 10);
}

static void handle_arws_query(int fd, const char *q, int len) {
    char cmd[64] = {0};
    int i = 0;
    while (i < len && i < 63 && q[i] != ' ' && q[i] != '\t' && q[i] != '\n') {
        cmd[i] = q[i];
        i++;
    }

    char resp[4096];
    int rlen = 0;

    if (strcmp(cmd, "cfg") == 0) {
        char sub[32] = {0};
        if (sscanf(q + i, "%31s", sub) == 1 && strcmp(sub, "reload") == 0) {
            arws_config_reload_from_disk();
            rlen = snprintf(resp, sizeof(resp), "arws config reloaded (path=%s)",
                            arws_config_get_path());
        } else {
            rlen = snprintf(resp, sizeof(resp), "usage: cfg reload");
        }
    } else if (strcmp(cmd, "maintenance") == 0 ||
               strcmp(cmd, "production") == 0 ||
               strcmp(cmd, "test") == 0) {
        char host[128] = {0};
        char path[256] = {0};
        int n = sscanf(q + i, "%127s %255s", host, path);
        if (n >= 1 && host[0]) {
            if (!path[0]) strncpy(path, "*", sizeof(path) - 1);
            if (arws_config_add_override(host, path, cmd) == 0) {
                rlen = snprintf(resp, sizeof(resp), "override %s %s -> %s", host, path, cmd);
            } else {
                rlen = snprintf(resp, sizeof(resp), "failed to add override");
            }
        } else {
            if (arws_config_set_global(cmd) == 0) {
                rlen = snprintf(resp, sizeof(resp), "global mode -> %s", cmd);
            } else {
                rlen = snprintf(resp, sizeof(resp), "failed to set global mode");
            }
        }
    } else if (strcmp(cmd, "status") == 0) {
        rlen = snprintf(resp, sizeof(resp),
            "arws RUNNING mode=%s port=%d bind=%s global_mode=%s",
            arws_config_get_mode_name(), arws_config_get_port(),
            arws_config_get_bind_address(), arws_config_get_global_mode());
    } else if (strcmp(cmd, "routes") == 0) {
        rlen = arws_config_dump_routes(resp, sizeof(resp));
        if (rlen <= 0) rlen = snprintf(resp, sizeof(resp), "no routes");
    } else if (strcmp(cmd, "ping") == 0) {
        rlen = snprintf(resp, sizeof(resp), "pong");
    } else {
        rlen = snprintf(resp, sizeof(resp), "unknown arws command: %s", cmd);
    }
    if (rlen < 0) rlen = 0;
    if (rlen >= (int)sizeof(resp)) rlen = (int)sizeof(resp) - 1;

    ar_ipc_send_frame(fd, IPC_QUERY_RESP, resp, rlen + 1);
}

static void *client_handler_loop(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    unsigned char buf[AR_IPC_BUF_SIZE];

    alri_print(CYN "[ARWS]" RST " client_handler_loop started for fd=%d\n", client_fd);

    int idle_rounds = 0;

    while (gateway_running) {
        /* Serialize reads on this fd with cross-queries: hold the same per-fd
           lock the IPC_QUERY forwarder uses, so the response is never stolen
           by this loop while a cross-query is awaiting it. */
        query_lock_fd(client_fd);

        if (!peek_is_ipc_frame(client_fd)) {
            query_unlock_fd(client_fd);
            idle_rounds++;
            /* Keep trying for ~2s (40 rounds x 50ms) before giving up */
            if (idle_rounds > 40) {
                alri_print(CYN "[ARWS]" RST " client_handler_loop: idle timeout on fd=%d, exiting\n", client_fd);
                break;
            }
            ar_sleep_ms(50);
            continue;
        }
        idle_rounds = 0;

        int type;
        uint32_t len = sizeof(buf);
        if (ar_ipc_recv_frame(client_fd, &type, buf, &len) < 0) {
            query_unlock_fd(client_fd);
            break;
        }

        switch (type) {
            case IPC_REGISTER: {
                char name[64] = {0};
                char prefix[256] = {0};
                char method[16] = {0};
                char host[256] = {0};
                char mode[16] = {0};
                char proxy_url[256] = {0};

                alri_print(CYN "[ARWS]" RST " IPC_REGISTER raw='%s' len=%u\n",
                           (const char*)buf, len);

                int parsed = sscanf((const char *)buf,
                    "%63s %255s %15s %255s %15s proxy=%255s",
                    name, prefix, method, host, mode, proxy_url);

                if (parsed < 5) {
                    sscanf((const char *)buf, "%63s %255s %15s %255s %15s",
                           name, prefix, method, host, mode);
                }

                alri_print(CYN "[ARWS]" RST " IPC_REGISTER parsed: name='%s' prefix='%s' method='%s' host='%s' mode='%s' proxy='%s'\n",
                           name, prefix, method, host, mode, proxy_url);

                named_add(client_fd, name);

                arws_config_reload_from_disk();

                /* type= (proxy|stream|redirect|backend) separated from mode
                   (operational). redirect=<url> registers a 302 route. */
                char type_str[16] = {0};
                char redirect_url[256] = {0};
                const char *p_type = strstr((const char *)buf, "type=");
                if (p_type) sscanf(p_type + 5, "%15s", type_str);
                const char *p_redir = strstr((const char *)buf, "redirect=");
                if (p_redir) sscanf(p_redir + 9, "%255s", redirect_url);

                if (redirect_url[0] || strcmp(type_str, "redirect") == 0) {
                    if (!redirect_url[0]) {
                        snprintf(redirect_url, sizeof(redirect_url), "%s", mode[0] ? mode : "/");
                    }
                    arws_add_redirect_route(prefix, method, host, "*", redirect_url);
                    char ack[64];
                    int ack_len = snprintf(ack, sizeof(ack), "ACK REDIRECT");
                    ar_ipc_send_frame(client_fd, IPC_ACK, ack, ack_len + 1);
                    alri_print(CYN "[ARWS]" RST " Redirect: %s %s host='%s' -> %s\n",
                                   method, prefix, host[0] ? host : "*", redirect_url);
                    break;
                }

                /* Rate limit opcional definido pela app: "rl=<max>,<window>".
                   rl=0 ou ausente -> nenhuma regra (default alto do arws). */
                const char *p_rl = strstr((const char *)buf, "rl=");
                if (p_rl) {
                    int rl_max = 0, rl_win = 0;
                    if (sscanf(p_rl + 3, "%d,%d", &rl_max, &rl_win) >= 1 && rl_max > 0) {
                        if (rl_win <= 0) rl_win = 60;
                        char rl_path[128];
                        if (prefix[0] && prefix[0] == '/' && strcmp(prefix, "/") != 0) {
                            snprintf(rl_path, sizeof(rl_path), "%s", prefix);
                        } else {
                            snprintf(rl_path, sizeof(rl_path), "/");
                        }
                        arws_ratelimit_set_rule(host[0] ? host : "*", rl_path, rl_max, rl_win);
                        alri_print(CYN "[ARWS]" RST " RateLimit via route: host='%s' path='%s' max=%d window=%ds\n",
                                   host[0] ? host : "*", rl_path, rl_max, rl_win);
                    }
                }

                if (proxy_url[0] != '\0') {
                    int is_stream = (strncmp(mode, "stream", 6) == 0) ||
                                    (strcmp(type_str, "stream") == 0);
                    const char *route_mode = "*";
                    if (!is_stream && mode[0] && strcmp(mode, "stream") != 0)
                        route_mode = mode;
                    if (is_stream) {
                        arws_add_stream_route(prefix, method, host,
                                              route_mode, proxy_url);
                        if (host[0] && prefix[0]) {
                            char cfg_path[256];
                            const char *p = prefix;
                            while (*p == '/') p++;
                            if (strcmp(p, "*") == 0) {
                                snprintf(cfg_path, sizeof(cfg_path), "*");
                            } else if (!p[0]) {
                                snprintf(cfg_path, sizeof(cfg_path), "/");
                            } else {
                                snprintf(cfg_path, sizeof(cfg_path), "%s", p);
                            }
                            arws_config_add_stream_route(host, cfg_path, proxy_url);
                            if (!arws_config_has_override(host, cfg_path)) {
                                const char *save_mode =
                                    (mode[0] && strcmp(mode, "stream") != 0)
                                        ? mode : arws_config_get_global_mode();
                                arws_config_add_override(host, cfg_path, save_mode);
                            }
                        }
                    } else {
                        arws_add_proxy_route(prefix, method, host,
                                             route_mode, proxy_url);
                        if (host[0] && prefix[0]) {
                            char cfg_path[256];
                            const char *p = prefix;
                            while (*p == '/') p++;
                            if (strcmp(p, "*") == 0) {
                                snprintf(cfg_path, sizeof(cfg_path), "*");
                            } else if (!p[0]) {
                                snprintf(cfg_path, sizeof(cfg_path), "/");
                            } else {
                                snprintf(cfg_path, sizeof(cfg_path), "%s", p);
                            }
                            arws_config_add_proxy_route(host, cfg_path, proxy_url);
                            if (!arws_config_has_override(host, cfg_path)) {
                                const char *save_mode = (route_mode[0] && strcmp(route_mode, "*") != 0) ? route_mode : arws_config_get_global_mode();
                                arws_config_add_override(host, cfg_path, save_mode);
                            }
                        }
                    }
                    char ack[64] = "ACK PROXY";
                    ar_ipc_send_frame(client_fd, IPC_ACK, ack, (uint32_t)strlen(ack) + 1);
                } else {
                    int backend_id = arws_register_backend(name, client_fd);
                    if (backend_id > 0) {
                        arws_add_route(prefix, method, host, mode, backend_id);

                        char ack[64];
                        int ack_len = snprintf(ack, sizeof(ack), "ACK %d", backend_id);
                        ar_ipc_send_frame(client_fd, IPC_ACK, ack, ack_len + 1);

                        alri_print(CYN "[ARWS]" RST " Route: %s %s host='%s' mode=%s -> backend %d\n",
                                   method, prefix,
                                   host[0] ? host : "*",
                                   mode[0] ? mode : MODE_PRODUCTION,
                                   backend_id);

                        if (host[0] && prefix[0]) {
                            const char *save_mode = (mode[0] && strcmp(mode, "*") != 0) ? mode : arws_config_get_global_mode();
                            if (!arws_config_has_override(host, prefix))
                                arws_config_add_override(host, prefix, save_mode);
                        }
                    }
                }
                break;
            }
            case IPC_UNREGISTER: {
                char prefix[256] = {0};
                char method[16] = "*";
                char host[256] = "*";

                alri_print(CYN "[ARWS]" RST " IPC_UNREGISTER raw='%s' len=%u\n",
                           (const char*)buf, len);

                int backend_id = 0;
                if (sscanf((const char *)buf, "%d", &backend_id) == 1 && backend_id > 0) {
                    arws_remove_routes_by_backend(backend_id);
                    arws_unregister_backend(backend_id);
                    alri_print(CYN "[ARWS]" RST " Unregistered backend id=%d and associated routes\n", backend_id);
                } else if (sscanf((const char *)buf, "%255s %15s %255s", prefix, method, host) >= 1) {
                    arws_remove_route(prefix, method[0] ? method : "*", host[0] ? host : "*");
                    alri_print(CYN "[ARWS]" RST " Unregistered route: %s %s host='%s'\n", method, prefix, host);
                }

                char ack[64] = "ACK UNREGISTER";
                ar_ipc_send_frame(client_fd, IPC_ACK, ack, (uint32_t)strlen(ack) + 1);
                break;
            }
            case IPC_RELOAD: {
                alri_print(CYN "[ARWS]" RST " IPC_RELOAD requested\n");
                const char *path = arws_config_get_path();
                if (path && path[0]) {
                    arws_config_load(path);
                } else {
                    arws_config_load("storage/arws/arws.cfg");
                }

                int new_ttl = arws_config_get_cache_ttl();
                arws_cache_set_ttl(new_ttl);
                alri_print(CYN "[ARWS]" RST " cache_ttl updated to %d\n", new_ttl);

                char ack[64] = "ACK RELOAD";
                ar_ipc_send_frame(client_fd, IPC_ACK, ack, (uint32_t)strlen(ack) + 1);
                break;
            }
            case IPC_CACHE_CLEAR: {
                alri_print(CYN "[ARWS]" RST " IPC_CACHE_CLEAR requested\n");
                arws_cache_clear();
                char ack[64] = "ACK CACHE CLEAR";
                ar_ipc_send_frame(client_fd, IPC_ACK, ack, (uint32_t)strlen(ack) + 1);
                break;
            }
            case IPC_QUERY: {
                char target[64] = {0};
                const char *payload = (const char *)buf;
                int payload_len = len;

                /* First line = target app name */
                int ti = 0;
                while (ti < payload_len && ti < (int)sizeof(target) - 1 &&
                       payload[ti] != '\n' && payload[ti] != '\0') {
                    target[ti] = payload[ti];
                    ti++;
                }
                target[ti] = '\0';

                /* Target 'arws' is handled internally by the gateway */
                if (strcmp(target, "arws") == 0) {
                    const char *qdata = payload + ti;
                    int qlen = payload_len - ti;
                    if (qlen > 0 && *qdata == '\n') { qdata++; qlen--; }
                    if (qlen < 0) qlen = 0;
                    handle_arws_query(client_fd, qdata, qlen);
                    break;
                }

                /* If target is ourselves (this backend's name), serve locally */
                const char *my_name = NULL;
                for (int bi = 0; bi < AR_IPC_MAX_CLIENTS; bi++) {
                    ArwsBackend *bk = arws_get_backend(bi);
                    if (bk && bk->fd == client_fd) { my_name = bk->name; break; }
                }

                if (my_name && strcmp(target, my_name) == 0) {
                    /* Query is for THIS app — forward payload as raw data */
                    const char *qdata = payload + ti;
                    int qlen = payload_len - ti;
                    if (qlen > 0 && *qdata == '\n') { qdata++; qlen--; }
                    if (qlen < 0) qlen = 0;

                    if (qlen > 0) {
                        ar_ipc_send_raw(client_fd, (const unsigned char *)qdata, qlen);
                    }

                    unsigned char raw_resp[AR_IPC_BUF_SIZE];
                    int rlen = ar_ipc_recv_raw(client_fd, raw_resp, sizeof(raw_resp));
                    if (rlen > 0) {
                        ar_ipc_send_frame(client_fd, IPC_QUERY_RESP, raw_resp, rlen);
                    } else {
                        ar_ipc_send_frame(client_fd, IPC_ERROR, "no resp from app", 17);
                    }
                    break;
                }

                /* Cross-backend query — forward to target, lock to avoid race */
                int target_fd = arws_find_backend_by_name(target);
                if (target_fd < 0) target_fd = named_find(target);
                if (target_fd < 0) {
                    ar_ipc_send_frame(client_fd, IPC_ERROR, "target not found", 17);
                    break;
                }

                const char *query_data = payload + ti;
                int query_len = payload_len - ti;
                if (query_len > 0 && *query_data == '\n') { query_data++; query_len--; }
                if (query_len < 0) query_len = 0;

                query_lock_fd(target_fd);
                int sent_ok = (ar_ipc_send_frame(target_fd, IPC_QUERY, query_data, query_len) >= 0);

                unsigned char resp_buf[AR_IPC_BUF_SIZE];
                uint32_t resp_len = sizeof(resp_buf);
                int resp_type = 0;

                if (sent_ok) {
                    /* The app's control channel carries heartbeats on the same
                       connection, so a heartbeat can arrive right after the
                       query and get misread as the response. Keep reading and
                       discarding non-response frames until IPC_QUERY_RESP or a
                       ~3s deadline. */
                    uint64_t deadline = ar_time_ms() + 3000;
                    while (ar_time_ms() < deadline) {
                        if (peek_is_ipc_frame(target_fd)) {
                            resp_len = sizeof(resp_buf);
                            if (ar_ipc_recv_frame(target_fd, &resp_type, resp_buf, &resp_len) < 0) {
                                resp_type = 0;
                                break;
                            }
                            if (resp_type == IPC_QUERY_RESP) break;
                        }
                    }
                    if (resp_type != IPC_QUERY_RESP) resp_type = 0;
                }
                query_unlock_fd(target_fd);

                if (resp_type == IPC_QUERY_RESP) {
                    ar_ipc_send_frame(client_fd, IPC_QUERY_RESP, resp_buf, resp_len);
                } else {
                    ar_ipc_send_frame(client_fd, IPC_ERROR, "target error", 13);
                }
                break;
            }
        }

        query_unlock_fd(client_fd);
    }

    /* Clean up backend registration and routes for this fd */
    int bid = arws_find_backend_id_by_fd(client_fd);
    if (bid > 0) {
        arws_remove_routes_by_backend(bid);
        arws_unregister_backend(bid);
        alri_print(CYN "[ARWS]" RST " Cleaned up backend id=%d (fd=%d)\n", bid, client_fd);
    } else {
        ar_socket_close(client_fd);
    }

    named_remove(client_fd);

    ar_mutex_lock(admin_mutex);
    for (int i = 0; i < admin_client_count; i++) {
        if (admin_client_fds[i] == client_fd) {
            admin_client_fds[i] = admin_client_fds[--admin_client_count];
            break;
        }
    }
    ar_mutex_unlock(admin_mutex);

    return NULL;
}

static void *accept_loop(void *arg) {
    (void)arg;
    while (gateway_running) {
        int client_fd = ar_socket_accept(admin_server_fd);
        if (client_fd < 0) {
            if (gateway_running)
                alri_print(RED "[ARWS]" RST " Accept failed\n");
            break;
        }

        if (!add_client_fd(client_fd)) {
            alri_print(RED "[ARWS]" RST " Max clients reached\n");
            ar_socket_close(client_fd);
            continue;
        }

        void *th = ar_thread_create(client_handler_loop, (void*)(intptr_t)client_fd);
        if (th) ar_thread_detach(th);
    }
    return NULL;
}

int arws_init(void) {
    alri_print_force(CYN "[ARWS]" RST " Initializing Gateway...\n");
    arws_route_init();
    arws_registry_init();
    arws_ratelimit_init();
    arws_session_init();
    arws_cache_init();
    arws_proxy_init();
    int cache_ttl = arws_config_get_cache_ttl();
    arws_cache_set_ttl(cache_ttl);
    query_mutex = ar_mutex_create();
    dispatch_mutex = ar_mutex_create();
    memset(dispatch_locks, 0, sizeof(dispatch_locks));
    return 0;
}

int arws_start(int port, int mode) {
    if (gateway_running) return 0;
    gateway_running = 1;

    admin_mutex = ar_mutex_create();
    memset(admin_client_fds, 0, sizeof(admin_client_fds));
    admin_client_count = 0;

    admin_server_fd = ar_ipc_server_start(ARWS_ADMIN_PORT);
    if (admin_server_fd < 0) {
        alri_print_force(RED "[ARWS]" RST " Failed to start admin server on port %d\n",
                         ARWS_ADMIN_PORT);
        gateway_running = 0;
        return -1;
    }

    alri_print_force(CYN "[ARWS]" RST " Admin server at 127.0.0.1:%d\n",
                     ARWS_ADMIN_PORT);

    accept_thread = ar_thread_create(accept_loop, NULL);
    if (accept_thread) ar_thread_detach(accept_thread);

    /* Wait up to 5s for at least one backend to register before accepting HTTP */
    int waited = 0;
    while (waited < 50) {
        if (arws_backend_count() > 0) break;
        ar_sleep_ms(100);
        waited++;
    }
    if (waited >= 50)
        alri_print(CYN "[ARWS]" RST " No backends registered after 5s, starting anyway\n");

    alri_print_force(CYN "[ARWS]" RST " Starting HTTP server on port %d...\n", port);

    if (server_start(port, mode, handler_func) != 0) {
        alri_print_force(RED "[ARWS]" RST " Failed to start HTTP server\n");
        gateway_running = 0;
        return -1;
    }

    return 0;
}

void arws_stop(void) {
    gateway_running = 0;
    server_stop();
    arws_config_watchdog_stop();

    if (admin_server_fd >= 0) {
        ar_ipc_server_stop(admin_server_fd);
        admin_server_fd = -1;
    }

    ar_mutex_lock(admin_mutex);
    for (int i = 0; i < admin_client_count; i++) {
        if (admin_client_fds[i] >= 0)
            ar_socket_close(admin_client_fds[i]);
    }
    admin_client_count = 0;
    ar_mutex_unlock(admin_mutex);

    arws_close_all_backends();

    alri_print(CYN "[ARWS]" RST " Gateway stopped\n");
}
