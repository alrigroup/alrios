/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_backend.h"
#include "ardb_pgwire.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#endif

static ArdbBackendConn g_backend_pool[ARDB_BACKEND_POOL_SIZE];
static void *g_backend_mutex = NULL;
static int g_backend_initialized = 0;
static char g_pg_host[64] = ARDB_BACKEND_DEFAULT_HOST;
static int g_pg_port = ARDB_BACKEND_DEFAULT_PORT;
static char g_pg_user[64] = "postgres";
static char g_pg_pass[128] = "postgres";
static char g_pg_db[64] = "postgres";

static void write_uint32_be(unsigned char *buf, uint32_t val) {
    buf[0] = (unsigned char)((val >> 24) & 0xFF);
    buf[1] = (unsigned char)((val >> 16) & 0xFF);
    buf[2] = (unsigned char)((val >> 8) & 0xFF);
    buf[3] = (unsigned char)(val & 0xFF);
}

static uint32_t read_uint32_be(const unsigned char *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           ((uint32_t)buf[3]);
}

/* Establish native PG-Wire handshake with isolated PostgreSQL backend */
static int connect_to_postgres(ArdbBackendConn *conn) {
    if (conn->socket_fd >= 0) {
        ar_socket_close(conn->socket_fd);
        conn->socket_fd = -1;
    }

#ifdef _WIN32
    int fd = ar_socket_create(1);
#else
    int fd = ar_socket_create(SOCK_STREAM);
#endif
    if (fd < 0) return -1;

    if (ar_socket_connect(fd, conn->host, (uint16_t)conn->port) != 0) {
        ar_socket_close(fd);
        return -1;
    }

    /* 1. Send StartupMessage */
    char params[256];
    int plen = snprintf(params, sizeof(params), "user%c%s%cdatabase%c%s%c%c",
                        '\0', conn->user, '\0', '\0', conn->database, '\0', '\0');
    uint32_t pkt_len = 4 + 4 + (uint32_t)plen;

    unsigned char startup[512];
    write_uint32_be(startup, pkt_len);
    write_uint32_be(startup + 4, PG_MSG_STARTUP_V3);
    memcpy(startup + 8, params, plen);

    if (ar_socket_send(fd, (const char*)startup, pkt_len) != (int)pkt_len) {
        ar_socket_close(fd);
        return -1;
    }

    /* 2. Process authentication with PostgreSQL backend */
    unsigned char hdr[5];
    while (1) {
        int r = ar_socket_recv(fd, (char*)hdr, 5);
        if (r < 5) { ar_socket_close(fd); return -1; }

        char type = (char)hdr[0];
        uint32_t len = read_uint32_be(hdr + 1);

        if (type == PG_TYPE_AUTH_REQ) {
            unsigned char auth_code_buf[4];
            if (ar_socket_recv(fd, (char*)auth_code_buf, 4) < 4) { ar_socket_close(fd); return -1; }
            uint32_t auth_code = read_uint32_be(auth_code_buf);

            if (auth_code == 0) {
                /* AuthenticationOk */
            } else if (auth_code == 3) {
                /* CleartextPassword */
                size_t pw_len = strlen(conn->password) + 1;
                uint32_t send_len = 4 + (uint32_t)pw_len;
                unsigned char *pbuf = (unsigned char*)malloc(1 + send_len);
                if (!pbuf) { ar_socket_close(fd); return -1; }
                pbuf[0] = PG_TYPE_PASSWORD;
                write_uint32_be(pbuf + 1, send_len);
                memcpy(pbuf + 5, conn->password, pw_len);
                ar_socket_send(fd, (const char*)pbuf, 1 + send_len);
                free(pbuf);
            }
        } else if (type == PG_TYPE_READY_FOR_QUERY) {
            /* Backend handshake complete */
            break;
        } else if (type == PG_TYPE_ERROR_RESP) {
            ar_socket_close(fd);
            return -1;
        } else {
            /* Drain other parameter/key packets */
            if (len > 4) {
                char *discard = (char*)malloc(len - 4);
                if (discard) {
                    ar_socket_recv(fd, discard, len - 4);
                    free(discard);
                }
            }
        }
    }

    conn->socket_fd = fd;
    conn->is_connected = 1;
    conn->last_used_ms = (uint64_t)ar_time_ms();
    return 0;
}

int ardb_backend_init(const char *host, int port, const char *user, const char *password, const char *database) {
    if (g_backend_initialized) return 0;
    g_backend_mutex = ar_mutex_create();

    if (host && host[0]) strncpy(g_pg_host, host, sizeof(g_pg_host) - 1);
    if (port > 0) g_pg_port = port;
    if (user && user[0]) strncpy(g_pg_user, user, sizeof(g_pg_user) - 1);
    if (password && password[0]) strncpy(g_pg_pass, password, sizeof(g_pg_pass) - 1);
    if (database && database[0]) strncpy(g_pg_db, database, sizeof(g_pg_db) - 1);

    ar_mutex_lock(g_backend_mutex);
    memset(g_backend_pool, 0, sizeof(g_backend_pool));
    for (int i = 0; i < ARDB_BACKEND_POOL_SIZE; i++) {
        g_backend_pool[i].socket_fd = -1;
        strncpy(g_backend_pool[i].host, g_pg_host, sizeof(g_backend_pool[i].host) - 1);
        g_backend_pool[i].port = g_pg_port;
        strncpy(g_backend_pool[i].user, g_pg_user, sizeof(g_backend_pool[i].user) - 1);
        strncpy(g_backend_pool[i].password, g_pg_pass, sizeof(g_backend_pool[i].password) - 1);
        strncpy(g_backend_pool[i].database, g_pg_db, sizeof(g_backend_pool[i].database) - 1);
    }
    g_backend_initialized = 1;
    ar_mutex_unlock(g_backend_mutex);

    return 0;
}

void ardb_backend_cleanup(void) {
    if (!g_backend_initialized) return;
    ar_mutex_lock(g_backend_mutex);
    for (int i = 0; i < ARDB_BACKEND_POOL_SIZE; i++) {
        if (g_backend_pool[i].socket_fd >= 0) {
            ar_socket_close(g_backend_pool[i].socket_fd);
            g_backend_pool[i].socket_fd = -1;
        }
        g_backend_pool[i].is_connected = 0;
        g_backend_pool[i].in_use = 0;
    }
    g_backend_initialized = 0;
    ar_mutex_unlock(g_backend_mutex);

    if (g_backend_mutex) {
        ar_mutex_destroy(g_backend_mutex);
        g_backend_mutex = NULL;
    }
}

ArdbBackendConn* ardb_backend_acquire(void) {
    if (!g_backend_initialized) ardb_backend_init(NULL, 0, NULL, NULL, NULL);

    ar_mutex_lock(g_backend_mutex);
    ArdbBackendConn *selected = NULL;

    for (int i = 0; i < ARDB_BACKEND_POOL_SIZE; i++) {
        if (!g_backend_pool[i].in_use) {
            selected = &g_backend_pool[i];
            selected->in_use = 1;
            break;
        }
    }
    ar_mutex_unlock(g_backend_mutex);

    if (!selected) return NULL;

    if (!selected->is_connected || selected->socket_fd < 0) {
        if (connect_to_postgres(selected) != 0) {
            selected->in_use = 0;
            return NULL;
        }
    }

    return selected;
}

void ardb_backend_release(ArdbBackendConn *conn) {
    if (!conn) return;
    ar_mutex_lock(g_backend_mutex);
    conn->in_use = 0;
    conn->last_used_ms = (uint64_t)ar_time_ms();
    ar_mutex_unlock(g_backend_mutex);
}

int ardb_backend_relay_query(ArdbBackendConn *conn, const char *sql_query, int client_fd) {
    if (!conn || !sql_query || client_fd < 0) return -1;

    /* 1. Send 'Q' (Query) packet to backend PostgreSQL */
    size_t qlen = strlen(sql_query) + 1;
    uint32_t pkt_len = 4 + (uint32_t)qlen;

    unsigned char *qbuf = (unsigned char*)malloc(1 + pkt_len);
    if (!qbuf) return -1;

    qbuf[0] = PG_TYPE_QUERY;
    write_uint32_be(qbuf + 1, pkt_len);
    memcpy(qbuf + 5, sql_query, qlen);

    if (ar_socket_send(conn->socket_fd, (const char*)qbuf, 1 + pkt_len) != (int)(1 + pkt_len)) {
        free(qbuf);
        conn->is_connected = 0;
        return -1;
    }
    free(qbuf);

    /* 2. Stream responses back to client until ReadyForQuery ('Z') */
    unsigned char hdr[5];
    while (1) {
        int r = ar_socket_recv(conn->socket_fd, (char*)hdr, 5);
        if (r < 5) {
            conn->is_connected = 0;
            return -1;
        }

        char type = (char)hdr[0];
        uint32_t len = read_uint32_be(hdr + 1);

        /* Forward packet header */
        ar_socket_send(client_fd, (const char*)hdr, 5);

        /* Forward packet payload */
        if (len > 4) {
            uint32_t remaining = len - 4;
            char chunk[4096];
            while (remaining > 0) {
                uint32_t to_read = remaining > sizeof(chunk) ? sizeof(chunk) : remaining;
                int n = ar_socket_recv(conn->socket_fd, chunk, to_read);
                if (n <= 0) {
                    conn->is_connected = 0;
                    return -1;
                }
                ar_socket_send(client_fd, chunk, n);
                remaining -= (uint32_t)n;
            }
        }

        if (type == PG_TYPE_READY_FOR_QUERY) {
            break; /* Completed */
        }
    }

    return 0;
}
