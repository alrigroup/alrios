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
#include "arws_upstream.h"
#include "arws_health.h"
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
    /* client_ip is already resolved in handle_client: headers like
       X-Forwarded-For / CF-Connecting-IP are only honored when the real
       peer is in ARWS_TRUSTED_PROXY. */
    const char *client_ip = server_get_client_ip(conn);

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
typedef struct {
    int has_response;
    int resp_type;
    uint32_t resp_len;
    char resp_buf[AR_IPC_BUF_SIZE];
} QueryRespSlot;

static QueryRespSlot *query_resp_slots = NULL;
static int query_resp_slots_max = 0;
static int *query_locks = NULL;
static int query_locks_max = 0;
static void *query_mutex = NULL;
static void *query_cond = NULL;

/* Per-backend dispatch lock: prevents interleaved HTTP req/resp on shared FD */
static int dispatch_locks[ARWS_MAX_BACKENDS];
static void *dispatch_mutex = NULL;
static void *dispatch_cond = NULL;

static int query_locks_ensure(int fd) {
    if (fd >= 0 && fd < query_locks_max && fd < query_resp_slots_max) return 0;
    int new_max = (fd + 256) & ~255;
    if (new_max < 64) new_max = 64;
    int *nl = (int *)realloc(query_locks, (size_t)new_max * sizeof(int));
    if (!nl) return -1;
    if (new_max > query_locks_max)
        memset(nl + query_locks_max, 0, (size_t)(new_max - query_locks_max) * sizeof(int));
    query_locks = nl;
    query_locks_max = new_max;

    QueryRespSlot *ns = (QueryRespSlot *)realloc(query_resp_slots, (size_t)new_max * sizeof(QueryRespSlot));
    if (!ns) return -1;
    if (new_max > query_resp_slots_max)
        memset(ns + query_resp_slots_max, 0, (size_t)(new_max - query_resp_slots_max) * sizeof(QueryRespSlot));
    query_resp_slots = ns;
    query_resp_slots_max = new_max;
    return 0;
}

static void query_lock_fd(int fd) {
    if (fd < 0) return;
    if (!query_mutex) query_mutex = ar_mutex_create();
    if (!query_cond) query_cond = ar_cond_create();

    ar_mutex_lock(query_mutex);
    while (gateway_running) {
        if (query_locks_ensure(fd) == 0 && query_locks[fd] == 0) {
            query_locks[fd] = 1;
            query_resp_slots[fd].has_response = 0;
            query_resp_slots[fd].resp_type = 0;
            query_resp_slots[fd].resp_len = 0;
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
    query_resp_slots[fd].has_response = 0;
    query_resp_slots[fd].resp_type = 0;
    query_resp_slots[fd].resp_len = 0;
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
    if (fd < 0 || !query_mutex) return 0;
    int locked;
    ar_mutex_lock(query_mutex);
    locked = (fd < query_locks_max && query_locks) ? query_locks[fd] : 0;
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

static int match_app_alias(const char *registered, const char *target) {
    if (strcmp(registered, target) == 0) return 1;
    if ((strcmp(registered, "ardb") == 0 || strcmp(registered, "db") == 0) &&
        (strcmp(target, "ardb") == 0 || strcmp(target, "db") == 0)) return 1;
    if ((strcmp(registered, "arauth") == 0 || strcmp(registered, "auth") == 0) &&
        (strcmp(target, "arauth") == 0 || strcmp(target, "auth") == 0)) return 1;
    if ((strcmp(registered, "arcdn") == 0 || strcmp(registered, "cdn") == 0) &&
        (strcmp(target, "arcdn") == 0 || strcmp(target, "cdn") == 0)) return 1;
    if ((strcmp(registered, "home.web") == 0 || strcmp(registered, "home-web") == 0) &&
        (strcmp(target, "home.web") == 0 || strcmp(target, "home-web") == 0)) return 1;
    if ((strcmp(registered, "detroit.web") == 0 || strcmp(registered, "detroit-web") == 0) &&
        (strcmp(target, "detroit.web") == 0 || strcmp(target, "detroit-web") == 0)) return 1;
    return 0;
}

static void named_add(int fd, const char *name) {
    ar_mutex_lock(admin_mutex);
    for (int i = 0; i < named_client_count; i++) {
        if (named_client_fds[i] == fd || match_app_alias(named_client_names[i], name)) {
            named_client_fds[i] = fd;
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
        if (match_app_alias(named_client_names[i], name)) {
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
    tv.tv_usec = 0;

    int ret = select(fd + 1, &rfds, NULL, NULL, &tv);
    if (ret <= 0) return 0;

    unsigned char peek[5];
    int r;
#ifdef _WIN32
    r = recv((SOCKET)fd, (char*)peek, 5, MSG_PEEK);
#else
    r = recv(fd, peek, 5, MSG_PEEK);
#endif
    if (r <= 0) return -1;
    if (r < 5) return 0;

    uint32_t frame_len = ((uint32_t)peek[0] << 24) |
                         ((uint32_t)peek[1] << 16) |
                         ((uint32_t)peek[2] << 8) |
                         ((uint32_t)peek[3]);
    int type = peek[4];

    return (frame_len <= AR_IPC_BUF_SIZE && type >= 1 && type <= 25);
}

static void handle_arws_query(int fd, const char *q, int len) {
    char cmd[64] = {0};
    int i = 0;
    while (i < len && i < 63 && q[i] != ' ' && q[i] != '\t' && q[i] != '\n') {
        cmd[i] = q[i];
        i++;
    }

    char *resp = (char *)malloc(AR_IPC_BUF_SIZE);
    if (!resp) return;
    int rlen = 0;

    if (strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0 || cmd[0] == '\0') {
        rlen = snprintf(resp, AR_IPC_BUF_SIZE,
            "ARWS Gateway & Layer 7 Load Balancer v3.0.0 (Self-Registered)\n\n"
            "Supported Commands:\n"
            "  status                                      - View runtime status, active mode and bind port\n"
            "  cfg reload                                  - Reload arws.cfg live from disk without downtime\n"
            "  routes                                      - List all active dynamic and static routes\n"
            "  <production|test|maintenance>               - Switch global gateway operational mode\n"
            "  <production|test|maintenance> <host> [path] - Set domain/route specific operational override\n"
            "  upstream list                               - List all backend upstream pools and node health\n"
            "  upstream add <pool> <host> <port> [w] [b]   - Register backend node to load balancer pool\n"
            "  upstream drain <pool> <host> <port> [1|0]   - Enable/disable graceful connection draining\n"
            "  ping                                        - Check gateway control channel connectivity\n");
    } else if (strcmp(cmd, "cfg") == 0) {
        char sub[32] = {0};
        if (sscanf(q + i, "%31s", sub) == 1 && strcmp(sub, "reload") == 0) {
            arws_config_reload_from_disk();
            rlen = snprintf(resp, AR_IPC_BUF_SIZE, "arws config reloaded (path=%s)",
                            arws_config_get_path());
        } else {
            rlen = snprintf(resp, AR_IPC_BUF_SIZE, "usage: cfg reload");
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
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "override %s %s -> %s", host, path, cmd);
            } else {
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "failed to add override");
            }
        } else {
            if (arws_config_set_global(cmd) == 0) {
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "global mode -> %s", cmd);
            } else {
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "failed to set global mode");
            }
        }
    } else if (strcmp(cmd, "status") == 0) {
        rlen = snprintf(resp, AR_IPC_BUF_SIZE,
            "arws RUNNING mode=%s port=%d bind=%s global_mode=%s",
            arws_config_get_mode_name(), arws_config_get_port(),
            arws_config_get_bind_address(), arws_config_get_global_mode());
    } else if (strcmp(cmd, "routes") == 0) {
        rlen = arws_config_dump_routes(resp, sizeof(resp));
        if (rlen <= 0) rlen = snprintf(resp, sizeof(resp), "no routes");
    } else if (strcmp(cmd, "upstream") == 0) {
        char subcmd[32] = {0};
        int off = 0;
        sscanf(q + i, "%31s%n", subcmd, &off);
        const char *args = q + i + off;

        if (strcmp(subcmd, "list") == 0) {
            ArwsUpstreamPool pools[ARWS_MAX_POOLS];
            int pcount = arws_upstream_get_all_pools(pools, ARWS_MAX_POOLS);
            if (pcount == 0) {
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "No upstream pools registered.\n");
            } else {
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "UPSTREAM POOLS (%d):\n", pcount);
                for (int p = 0; p < pcount; p++) {
                    ArwsUpstreamPool *up = &pools[p];
                    const char *algo_str = (up->algo == ARWS_LB_ROUND_ROBIN) ? "round_robin" :
                                           (up->algo == ARWS_LB_LEAST_CONN) ? "least_conn" :
                                           (up->algo == ARWS_LB_IP_HASH) ? "ip_hash" : "weighted_round_robin";
                    rlen += snprintf(resp + rlen, sizeof(resp) - rlen,
                                     "  Pool '@%s' [algo=%s, nodes=%d]:\n",
                                     up->name, algo_str, up->node_count);
                    for (int n = 0; n < up->node_count; n++) {
                        ArwsBackendNode *bn = &up->nodes[n];
                        rlen += snprintf(resp + rlen, sizeof(resp) - rlen,
                                         "    - %s:%d weight=%d conns=%d alive=%s backup=%s drain=%s reqs=%llu errs=%llu\n",
                                         bn->host, bn->port, bn->weight, bn->active_conns,
                                         bn->is_alive ? "UP" : "DOWN",
                                         bn->is_backup ? "YES" : "NO",
                                         bn->is_draining ? "YES" : "NO",
                                         (unsigned long long)bn->total_requests,
                                         (unsigned long long)bn->total_errors);
                    }
                }
            }
        } else if (strcmp(subcmd, "add") == 0) {
            char pool_name[64] = {0};
            char host[128] = {0};
            int port = 0, weight = 1, backup = 0;
            if (sscanf(args, "%63s %127s %d %d %d", pool_name, host, &port, &weight, &backup) >= 3) {
                if (arws_upstream_add_node(pool_name, host, port, weight, backup) == 0) {
                    rlen = snprintf(resp, AR_IPC_BUF_SIZE, "Node %s:%d added to pool '@%s' (weight=%d, backup=%d)",
                                    host, port, pool_name, weight, backup);
                } else {
                    rlen = snprintf(resp, AR_IPC_BUF_SIZE, "Failed to add node to pool '@%s'", pool_name);
                }
            } else {
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "usage: upstream add <pool> <host> <port> [weight=1] [backup=0]");
            }
        } else if (strcmp(subcmd, "drain") == 0) {
            char pool_name[64] = {0};
            char host[128] = {0};
            int port = 0, drain = 1;
            if (sscanf(args, "%63s %127s %d %d", pool_name, host, &port, &drain) >= 3) {
                if (arws_upstream_set_node_drain(pool_name, host, port, drain) == 0) {
                    rlen = snprintf(resp, AR_IPC_BUF_SIZE, "Node %s:%d drain=%d in pool '@%s'",
                                    host, port, drain, pool_name);
                } else {
                    rlen = snprintf(resp, AR_IPC_BUF_SIZE, "Failed to set drain for %s:%d in pool '@%s'",
                                    host, port, pool_name);
                }
            } else {
                rlen = snprintf(resp, AR_IPC_BUF_SIZE, "usage: upstream drain <pool> <host> <port> [drain=1|0]");
            }
        } else {
            rlen = snprintf(resp, AR_IPC_BUF_SIZE, "usage: upstream list | add | drain");
        }
    } else if (strcmp(cmd, "ping") == 0) {
        rlen = snprintf(resp, AR_IPC_BUF_SIZE, "pong");
    } else {
        rlen = snprintf(resp, AR_IPC_BUF_SIZE, "unknown arws command: %s", cmd);
    }
    if (rlen < 0) rlen = 0;
    if (rlen >= AR_IPC_BUF_SIZE) rlen = AR_IPC_BUF_SIZE - 1;
    resp[rlen] = '\0';

    ar_ipc_send_frame(fd, IPC_QUERY_RESP, resp, (uint32_t)rlen);
    free(resp);
}

static void *client_handler_loop(void *arg) {
    int client_fd = (int)(intptr_t)arg;
    unsigned char *buf = (unsigned char *)malloc(AR_IPC_BUF_SIZE);
    if (!buf) { ar_socket_close(client_fd); return NULL; }

    alri_print(CYN "[ARWS]" RST " client_handler_loop started for fd=%d\n", client_fd);

    int idle_rounds = 0;

    while (gateway_running) {
        int peek_res = peek_is_ipc_frame(client_fd);
        if (peek_res < 0) {
            break; /* Peer disconnected */
        }
        if (peek_res == 0) {
            idle_rounds++;
            ar_sleep_ms(10);
            continue;
        }
        idle_rounds = 0;

        int type;
        uint32_t len = AR_IPC_BUF_SIZE;
        if (ar_ipc_recv_frame(client_fd, &type, buf, &len) < 0) {
            break;
        }
        if (len < AR_IPC_BUF_SIZE) buf[len] = '\0';
        else buf[AR_IPC_BUF_SIZE - 1] = '\0';

        switch (type) {
            case IPC_QUERY_RESP:
            case IPC_RESPONSE: {
                if (query_locks_ensure(client_fd) == 0) {
                    ar_mutex_lock(query_mutex);
                    query_resp_slots[client_fd].has_response = 1;
                    query_resp_slots[client_fd].resp_type = type;
                    query_resp_slots[client_fd].resp_len = (len < AR_IPC_BUF_SIZE) ? len : AR_IPC_BUF_SIZE - 1;
                    memcpy(query_resp_slots[client_fd].resp_buf, buf, query_resp_slots[client_fd].resp_len);
                    query_resp_slots[client_fd].resp_buf[query_resp_slots[client_fd].resp_len] = '\0';
                    ar_mutex_unlock(query_mutex);
                }
                break;
            }
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

                    char hosts_buf[256];
                    strncpy(hosts_buf, host[0] ? host : "*", sizeof(hosts_buf) - 1);
                    hosts_buf[sizeof(hosts_buf) - 1] = '\0';
                    char *saveptr = NULL;
                    char *h_tok = strtok_r(hosts_buf, ",", &saveptr);

                    while (h_tok) {
                        while (*h_tok == ' ' || *h_tok == '\t') h_tok++;
                        char *end = h_tok + strlen(h_tok) - 1;
                        while (end > h_tok && (*end == ' ' || *end == '\t')) *end-- = '\0';

                        if (h_tok[0]) {
                            int is_local = 0;
                            size_t hlen = strlen(h_tok);
                            if (strcasecmp(h_tok, "localhost") == 0 || strcmp(h_tok, "127.0.0.1") == 0 ||
                                (hlen > 10 && strcasecmp(h_tok + hlen - 10, ".localhost") == 0)) {
                                is_local = 1;
                            }
                            const char *route_mode = "*";
                            if (is_local) {
                                route_mode = MODE_TEST;
                            } else if (!is_stream && mode[0] && strcmp(mode, "stream") != 0) {
                                route_mode = mode;
                            }

                            if (is_stream) {
                                arws_add_stream_route(prefix, method, h_tok, route_mode, proxy_url);
                            } else {
                                arws_add_proxy_route(prefix, method, h_tok, route_mode, proxy_url);
                            }

                            if (prefix[0]) {
                                char cfg_path[256];
                                const char *p = prefix;
                                while (*p == '/') p++;
                                if (strcmp(p, "*") == 0 || strcmp(prefix, "/*") == 0 || strcmp(prefix, "*") == 0) {
                                    snprintf(cfg_path, sizeof(cfg_path), "*");
                                } else if (!p[0] || strcmp(prefix, "/") == 0) {
                                    snprintf(cfg_path, sizeof(cfg_path), "*");
                                } else {
                                    snprintf(cfg_path, sizeof(cfg_path), "%s", p);
                                }
                                if (is_stream) {
                                    arws_config_add_stream_route(h_tok, cfg_path, proxy_url);
                                } else {
                                    arws_config_add_proxy_route(h_tok, cfg_path, proxy_url);
                                }
                                if (!arws_config_has_override(h_tok, cfg_path)) {
                                    const char *save_mode = (route_mode[0] && strcmp(route_mode, "*") != 0) ? route_mode : arws_config_get_global_mode();
                                    arws_config_add_override(h_tok, cfg_path, save_mode);
                                }
                            }
                        }
                        h_tok = strtok_r(NULL, ",", &saveptr);
                    }
                    char ack[64] = "ACK PROXY";
                    ar_ipc_send_frame(client_fd, IPC_ACK, ack, (uint32_t)strlen(ack) + 1);
                } else {
                    int backend_id = arws_register_backend(name, client_fd);
                    if (backend_id > 0) {
                        char hosts_buf[256];
                        strncpy(hosts_buf, host[0] ? host : "*", sizeof(hosts_buf) - 1);
                        hosts_buf[sizeof(hosts_buf) - 1] = '\0';
                        char *saveptr = NULL;
                        char *h_tok = strtok_r(hosts_buf, ",", &saveptr);
                        while (h_tok) {
                            while (*h_tok == ' ' || *h_tok == '\t') h_tok++;
                            char *end = h_tok + strlen(h_tok) - 1;
                            while (end > h_tok && (*end == ' ' || *end == '\t')) *end-- = '\0';
                            if (h_tok[0]) {
                                arws_add_route(prefix, method, h_tok, mode, backend_id);
                            }
                            h_tok = strtok_r(NULL, ",", &saveptr);
                        }

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
                uint32_t resp_len = 0;
                int resp_type = 0;

                if (sent_ok) {
                    ar_mutex_lock(query_mutex);
                    uint64_t deadline = ar_time_ms() + 3000;
                    while (gateway_running && ar_time_ms() < deadline && !query_resp_slots[target_fd].has_response) {
                        ar_mutex_unlock(query_mutex);
                        ar_sleep_ms(5);
                        ar_mutex_lock(query_mutex);
                    }
                    if (query_resp_slots[target_fd].has_response) {
                        resp_type = query_resp_slots[target_fd].resp_type;
                        resp_len = query_resp_slots[target_fd].resp_len;
                        memcpy(resp_buf, query_resp_slots[target_fd].resp_buf, resp_len);
                    }
                    ar_mutex_unlock(query_mutex);
                }
                query_unlock_fd(target_fd);

                if (resp_type == IPC_QUERY_RESP || resp_type == IPC_RESPONSE) {
                    ar_ipc_send_frame(client_fd, IPC_QUERY_RESP, resp_buf, resp_len);
                } else {
                    ar_ipc_send_frame(client_fd, IPC_ERROR, "target error", 13);
                }
                break;
            }
        }
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

    free(buf);
    return NULL;
}

static void *accept_loop(void *arg) {
    (void)arg;
    while (gateway_running) {
        int client_fd = ar_socket_accept(admin_server_fd);
        if (client_fd < 0) {
            if (gateway_running) {
                ar_sleep_ms(10);
                continue;
            }
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
    arws_upstream_init();
    arws_route_init();
    arws_registry_init();
    arws_ratelimit_init();
    arws_session_init();
    arws_cache_init();
    arws_proxy_init();
    arws_health_start();
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

    if (!admin_mutex) admin_mutex = ar_mutex_create();
    ar_mutex_lock(admin_mutex);
    memset(admin_client_fds, 0, sizeof(admin_client_fds));
    admin_client_count = 0;
    ar_mutex_unlock(admin_mutex);

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
    arws_health_stop();
    arws_upstream_cleanup();
    server_stop();
    arws_config_watchdog_stop();

    if (admin_server_fd >= 0) {
        int sfd = admin_server_fd;
        admin_server_fd = -1;
        ar_ipc_server_stop(sfd);
    }

    if (accept_thread) {
        ar_thread_join(accept_thread);
        accept_thread = NULL;
    }

    if (admin_mutex) {
        ar_mutex_lock(admin_mutex);
        for (int i = 0; i < admin_client_count; i++) {
            if (admin_client_fds[i] >= 0)
                ar_socket_close(admin_client_fds[i]);
        }
        admin_client_count = 0;
        ar_mutex_unlock(admin_mutex);
    }

    arws_close_all_backends();

    alri_print(CYN "[ARWS]" RST " Gateway stopped\n");
}
