/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_pgwire.h"
#include "ardb_backend.h"
#include "ardb_auth.h"
#include "ardb_firewall.h"
#include "ardb_audit.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#endif

static volatile int g_pgwire_running = 0;
static void *g_pgwire_listen_thread = NULL;
static int g_pgwire_listen_fd = -1;

/* Big-Endian helper functions for PG-Wire protocol */
static uint32_t read_uint32_be(const unsigned char *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8)  |
           ((uint32_t)buf[3]);
}

static void write_uint32_be(unsigned char *buf, uint32_t val) {
    buf[0] = (unsigned char)((val >> 24) & 0xFF);
    buf[1] = (unsigned char)((val >> 16) & 0xFF);
    buf[2] = (unsigned char)((val >> 8) & 0xFF);
    buf[3] = (unsigned char)(val & 0xFF);
}

int ardb_pgwire_send_auth_ok(int fd) {
    unsigned char buf[9];
    buf[0] = PG_TYPE_AUTH_REQ;
    write_uint32_be(buf + 1, 8);
    write_uint32_be(buf + 5, 0); /* 0 = AuthenticationOk */
    return ar_socket_send(fd, (const char*)buf, 9);
}

int ardb_pgwire_send_auth_cleartext_req(int fd) {
    unsigned char buf[9];
    buf[0] = PG_TYPE_AUTH_REQ;
    write_uint32_be(buf + 1, 8);
    write_uint32_be(buf + 5, 3); /* 3 = CleartextPassword */
    return ar_socket_send(fd, (const char*)buf, 9);
}

int ardb_pgwire_send_auth_md5_req(int fd, const char salt[4]) {
    unsigned char buf[13];
    buf[0] = PG_TYPE_AUTH_REQ;
    write_uint32_be(buf + 1, 12);
    write_uint32_be(buf + 5, 5); /* 5 = MD5Password */
    memcpy(buf + 9, salt, 4);
    return ar_socket_send(fd, (const char*)buf, 13);
}

int ardb_pgwire_send_param_status(int fd, const char *param, const char *value) {
    size_t plen = strlen(param) + 1;
    size_t vlen = strlen(value) + 1;
    uint32_t len = 4 + (uint32_t)(plen + vlen);

    unsigned char *buf = (unsigned char*)malloc(1 + len);
    if (!buf) return -1;

    buf[0] = PG_TYPE_PARAM_STATUS;
    write_uint32_be(buf + 1, len);
    memcpy(buf + 5, param, plen);
    memcpy(buf + 5 + plen, value, vlen);

    int r = ar_socket_send(fd, (const char*)buf, 1 + len);
    free(buf);
    return r;
}

int ardb_pgwire_send_backend_key_data(int fd, uint32_t pid, uint32_t key) {
    unsigned char buf[13];
    buf[0] = PG_TYPE_KEY_DATA;
    write_uint32_be(buf + 1, 12);
    write_uint32_be(buf + 5, pid);
    write_uint32_be(buf + 9, key);
    return ar_socket_send(fd, (const char*)buf, 13);
}

int ardb_pgwire_send_ready_for_query(int fd, char tx_status) {
    unsigned char buf[6];
    buf[0] = PG_TYPE_READY_FOR_QUERY;
    write_uint32_be(buf + 1, 5);
    buf[5] = (unsigned char)tx_status;
    return ar_socket_send(fd, (const char*)buf, 6);
}

int ardb_pgwire_send_error(int fd, const char *severity, const char *code, const char *message) {
    char payload[1024];
    int off = 0;

    payload[off++] = 'S';
    size_t slen = strlen(severity) + 1;
    memcpy(payload + off, severity, slen); off += (int)slen;

    payload[off++] = 'C';
    size_t clen = strlen(code) + 1;
    memcpy(payload + off, code, clen); off += (int)clen;

    payload[off++] = 'M';
    size_t mlen = strlen(message) + 1;
    memcpy(payload + off, message, mlen); off += (int)mlen;

    payload[off++] = '\0';

    uint32_t len = 4 + (uint32_t)off;
    unsigned char *buf = (unsigned char*)malloc(1 + len);
    if (!buf) return -1;

    buf[0] = PG_TYPE_ERROR_RESP;
    write_uint32_be(buf + 1, len);
    memcpy(buf + 5, payload, off);

    int r = ar_socket_send(fd, (const char*)buf, 1 + len);
    free(buf);
    return r;
}

int ardb_pgwire_send_command_complete(int fd, const char *tag) {
    if (!tag) tag = "SELECT 1";
    size_t tlen = strlen(tag) + 1;
    uint32_t len = 4 + (uint32_t)tlen;

    unsigned char *buf = (unsigned char*)malloc(1 + len);
    if (!buf) return -1;

    buf[0] = PG_TYPE_CMD_COMPLETE;
    write_uint32_be(buf + 1, len);
    memcpy(buf + 5, tag, tlen);

    int r = ar_socket_send(fd, (const char*)buf, 1 + len);
    free(buf);
    return r;
}

int ardb_pgwire_send_single_row(int fd, const char *col_name, const char *val) {
    /* 1. RowDescription ('T') */
    size_t clen = strlen(col_name) + 1;
    uint32_t rd_len = 4 + 2 + (uint32_t)clen + 4 + 2 + 4 + 2 + 4 + 2;
    unsigned char *rd_buf = (unsigned char*)malloc(1 + rd_len);
    if (!rd_buf) return -1;

    rd_buf[0] = PG_TYPE_ROW_DESC;
    write_uint32_be(rd_buf + 1, rd_len);
    rd_buf[5] = 0; rd_buf[6] = 1; /* 1 column */
    size_t off = 7;
    memcpy(rd_buf + off, col_name, clen); off += clen;
    write_uint32_be(rd_buf + off, 0); off += 4; /* table OID */
    rd_buf[off++] = 0; rd_buf[off++] = 0;       /* col attr */
    write_uint32_be(rd_buf + off, 25); off += 4;/* type OID: 25 (TEXT) */
    rd_buf[off++] = 0xFF; rd_buf[off++] = 0xFE; /* type size: -1 */
    write_uint32_be(rd_buf + off, 0xFFFFFFFF); off += 4; /* type mod */
    rd_buf[off++] = 0; rd_buf[off++] = 0;       /* format: 0 (text) */
    ar_socket_send(fd, (const char*)rd_buf, 1 + rd_len);
    free(rd_buf);

    /* 2. DataRow ('D') */
    size_t vlen = val ? strlen(val) : 0;
    uint32_t dr_len = 4 + 2 + 4 + (uint32_t)vlen;
    unsigned char *dr_buf = (unsigned char*)malloc(1 + dr_len);
    if (!dr_buf) return -1;

    dr_buf[0] = PG_TYPE_DATA_ROW;
    write_uint32_be(dr_buf + 1, dr_len);
    dr_buf[5] = 0; dr_buf[6] = 1; /* 1 col */
    write_uint32_be(dr_buf + 7, (uint32_t)vlen);
    if (vlen > 0) memcpy(dr_buf + 11, val, vlen);
    ar_socket_send(fd, (const char*)dr_buf, 1 + dr_len);
    free(dr_buf);

    /* 3. CommandComplete ('C') */
    ardb_pgwire_send_command_complete(fd, "SELECT 1");
    return 0;
}

/* Client connection handler loop (DBeaver / App) */
static void* pgwire_client_handler(void *arg) {
    int fd = (int)(intptr_t)arg;
    ArdbClientSession session;
    memset(&session, 0, sizeof(session));
    session.client_fd = fd;
    session.tx_status = PG_TX_IDLE;
    session.session_start_ms = (uint64_t)ar_time_ms();

    unsigned char hdr[5];

    /* 1. StartupMessage or SSLRequest */
    int r = ar_socket_recv(fd, (char*)hdr, 4);
    if (r < 4) {
        ar_socket_close(fd);
        return NULL;
    }

    uint32_t pkt_len = read_uint32_be(hdr);

    /* Buffer Overflow protection on oversized packets */
    if (pkt_len > ARDB_PGWIRE_MAX_BUF || pkt_len < 4) {
        alri_print(RED "[ARDB-SEC]" RST " Buffer Overflow attempt blocked: pkt_len=%u -> Dropping connection\n", pkt_len);
        ar_socket_close(fd);
        return NULL;
    }

    unsigned char *startup_payload = (unsigned char*)malloc(pkt_len - 4);
    if (!startup_payload) {
        ar_socket_close(fd);
        return NULL;
    }

    int received = 0;
    while (received < (int)(pkt_len - 4)) {
        int n = ar_socket_recv(fd, (char*)startup_payload + received, (pkt_len - 4) - received);
        if (n <= 0) break;
        received += n;
    }

    if (received < (int)(pkt_len - 4)) {
        free(startup_payload);
        ar_socket_close(fd);
        return NULL;
    }

    uint32_t proto_ver = read_uint32_be(startup_payload);

    /* Handle SSLRequest */
    if (proto_ver == PG_MSG_SSL_REQUEST) {
        free(startup_payload);
        /* Respond 'N' (Direct plain / TLS handled at gateway level) */
        char ssl_n = 'N';
        ar_socket_send(fd, &ssl_n, 1);

        /* Read real StartupMessage */
        r = ar_socket_recv(fd, (char*)hdr, 4);
        if (r < 4) { ar_socket_close(fd); return NULL; }
        pkt_len = read_uint32_be(hdr);
        if (pkt_len > ARDB_PGWIRE_MAX_BUF || pkt_len < 4) { ar_socket_close(fd); return NULL; }

        startup_payload = (unsigned char*)malloc(pkt_len - 4);
        if (!startup_payload) { ar_socket_close(fd); return NULL; }
        received = 0;
        while (received < (int)(pkt_len - 4)) {
            int n = ar_socket_recv(fd, (char*)startup_payload + received, (pkt_len - 4) - received);
            if (n <= 0) break;
            received += n;
        }
        proto_ver = read_uint32_be(startup_payload);
    }

    /* Parse StartupMessage parameters (user, database, etc.) */
    const char *p = (const char*)(startup_payload + 4);
    const char *end = (const char*)(startup_payload + (pkt_len - 4));

    while (p < end && *p != '\0') {
        const char *key = p;
        p += strlen(key) + 1;
        if (p >= end) break;
        const char *val = p;
        p += strlen(val) + 1;

        if (strcmp(key, "user") == 0) {
            strncpy(session.user, val, sizeof(session.user) - 1);
        } else if (strcmp(key, "database") == 0) {
            strncpy(session.database, val, sizeof(session.database) - 1);
        }
    }
    free(startup_payload);

    /* 2. Request authentication (Cleartext Password / Ephemeral Token) */
    ardb_pgwire_send_auth_cleartext_req(fd);

    /* 3. Read PasswordMessage */
    r = ar_socket_recv(fd, (char*)hdr, 5);
    if (r < 5 || hdr[0] != PG_TYPE_PASSWORD) {
        ar_socket_close(fd);
        return NULL;
    }

    uint32_t pass_len = read_uint32_be(hdr + 1);
    if (pass_len < 4 || pass_len > 1024) {
        ar_socket_close(fd);
        return NULL;
    }

    char *pass_buf = (char*)malloc(pass_len - 4 + 1);
    if (!pass_buf) { ar_socket_close(fd); return NULL; }

    received = 0;
    while (received < (int)(pass_len - 4)) {
        int n = ar_socket_recv(fd, pass_buf + received, (pass_len - 4) - received);
        if (n <= 0) break;
        received += n;
    }
    pass_buf[received] = '\0';
    /* Remove null-terminator or trailing whitespace */
    while (received > 0 && (pass_buf[received - 1] == '\0' || pass_buf[received - 1] == '\n' || pass_buf[received - 1] == '\r')) {
        pass_buf[--received] = '\0';
    }

    /* Validate Ephemeral Token or User Password */
    char validated_user[64] = {0};
    char validated_tenant[64] = {0};
    char validated_role[32] = {0};

    int auth_ok = 0;
    if (ardb_auth_verify_token(pass_buf, validated_user, validated_tenant, validated_role) == 0) {
        auth_ok = 1;
    } else {
        char gen_token[128];
        if (ardb_auth_generate_token(session.user, pass_buf, NULL, 3600, gen_token, sizeof(gen_token)) == 0) {
            ardb_auth_verify_token(gen_token, validated_user, validated_tenant, validated_role);
            auth_ok = 1;
        }
    }
    free(pass_buf);

    if (!auth_ok) {
        ardb_pgwire_send_error(fd, "FATAL", "28P01", "password authentication failed for user or expired token");
        ar_socket_close(fd);
        return NULL;
    }

    session.is_authenticated = 1;
    strncpy(session.tenant_id, validated_tenant, sizeof(session.tenant_id) - 1);
    char session_role[32] = {0};
    strncpy(session_role, validated_role, sizeof(session_role) - 1);

    /* 4. Send AuthenticationOk and Session Parameters */
    ardb_pgwire_send_auth_ok(fd);
    ardb_pgwire_send_param_status(fd, "server_version", "15.4 (ALRI OS Sovereign Data Guardian)");
    ardb_pgwire_send_param_status(fd, "server_encoding", "UTF8");
    ardb_pgwire_send_param_status(fd, "client_encoding", "UTF8");
    ardb_pgwire_send_param_status(fd, "DateStyle", "ISO, MDY");
    ardb_pgwire_send_param_status(fd, "is_superuser", strcmp(session_role, "admin") == 0 ? "on" : "off");
    ardb_pgwire_send_param_status(fd, "session_authorization", session.user);

    ardb_pgwire_send_backend_key_data(fd, (uint32_t)rand(), (uint32_t)rand());
    ardb_pgwire_send_ready_for_query(fd, session.tx_status);

    alri_print(GRN "[ARDB-AUTH]" RST " Client '%s' (tenant='%s', role='%s') connected via PG-Wire.\n",
               session.user, session.tenant_id, session_role);

    /* 5. Main Command Loop (Query, Parse, Sync, Terminate) */
    while (g_pgwire_running) {
        r = ar_socket_recv(fd, (char*)hdr, 5);
        if (r < 5) break;

        char msg_type = (char)hdr[0];
        uint32_t msg_len = read_uint32_be(hdr + 1);

        if (msg_type == PG_TYPE_TERMINATE) {
            break;
        }

        if (msg_len > ARDB_PGWIRE_MAX_BUF || msg_len < 4) {
            alri_print(RED "[ARDB-SEC]" RST " Oversized query packet dropped: len=%u\n", msg_len);
            break;
        }

        char *msg_payload = (char*)malloc(msg_len - 4 + 1);
        if (!msg_payload) break;

        received = 0;
        while (received < (int)(msg_len - 4)) {
            int n = ar_socket_recv(fd, msg_payload + received, (msg_len - 4) - received);
            if (n <= 0) break;
            received += n;
        }
        msg_payload[received] = '\0';

        if (msg_type == PG_TYPE_QUERY) {
            uint64_t start_us = (uint64_t)ar_time_ms() * 1000;
            char rewritten[2048];
            char reason[512] = {0};

            ArdbFwAction act = ardb_firewall_inspect(msg_payload, session.tenant_id, session_role,
                                                     rewritten, sizeof(rewritten),
                                                     reason, sizeof(reason));

            if (act != ARDB_FW_OK) {
                ardb_pgwire_send_error(fd, "ERROR", "42501", reason);
                ardb_audit_log_query(session.user, session.tenant_id, session.client_ip,
                                     msg_payload, 403, (uint64_t)ar_time_ms() * 1000 - start_us);
                ardb_pgwire_send_ready_for_query(fd, session.tx_status);
            } else {
                /* Relay to isolated PostgreSQL via Backend Proxy */
                ArdbBackendConn *bconn = ardb_backend_acquire();
                if (bconn) {
                    int relay_res = ardb_backend_relay_query(bconn, rewritten, fd);
                    ardb_backend_release(bconn);

                    if (relay_res == 0) {
                        ardb_audit_log_query(session.user, session.tenant_id, session.client_ip,
                                             rewritten, 200, (uint64_t)ar_time_ms() * 1000 - start_us);
                    } else {
                        ardb_pgwire_send_error(fd, "FATAL", "08006", "Connection failure to isolated PostgreSQL");
                        ardb_pgwire_send_ready_for_query(fd, session.tx_status);
                    }
                } else {
                    /* Standalone Mock mode for environments without physical PostgreSQL */
                    if (strstr(msg_payload, "current_schema") || strstr(msg_payload, "current_user")) {
                        ardb_pgwire_send_single_row(fd, "current_schema", "public");
                    } else if (strstr(msg_payload, "version()")) {
                        ardb_pgwire_send_single_row(fd, "version", "PostgreSQL 15.4 on x86_64-pc-linux-gnu, compiled by ALRIOS ARDB Sovereign Guardian");
                    } else if (strstr(msg_payload, "search_path")) {
                        ardb_pgwire_send_single_row(fd, "search_path", "public, \"$user\"");
                    } else if (strstr(msg_payload, "SELECT") || strstr(msg_payload, "select")) {
                        ardb_pgwire_send_single_row(fd, "result", "1");
                    } else {
                        ardb_pgwire_send_command_complete(fd, "OK");
                    }
                    ardb_pgwire_send_ready_for_query(fd, session.tx_status);
                    ardb_audit_log_query(session.user, session.tenant_id, session.client_ip,
                                         rewritten, 200, (uint64_t)ar_time_ms() * 1000 - start_us);
                }
            }
        } else if (msg_type == PG_TYPE_SYNC) {
            ardb_pgwire_send_ready_for_query(fd, session.tx_status);
        }

        free(msg_payload);
    }

    ar_socket_close(fd);
    alri_print(CYN "[ARDB]" RST " Client '%s' disconnected.\n", session.user);
    return NULL;
}

static void* pgwire_listen_worker(void *arg) {
    int port = (int)(intptr_t)arg;
#ifdef _WIN32
    g_pgwire_listen_fd = ar_socket_create(1); /* SOCK_STREAM */
#else
    g_pgwire_listen_fd = ar_socket_create(SOCK_STREAM);
#endif
    if (g_pgwire_listen_fd < 0) {
        alri_print(RED "[ARDB]" RST " Failed to create socket for PG-Wire (err=%d)\n", g_pgwire_listen_fd);
        g_pgwire_running = 0;
        return NULL;
    }

    ar_socket_reuseaddr(g_pgwire_listen_fd, 1);
    if (ar_socket_bind(g_pgwire_listen_fd, "0.0.0.0", (uint16_t)port) != 0) {
        alri_print(RED "[ARDB]" RST " Failed to bind PG-Wire on port %d\n", port);
        ar_socket_close(g_pgwire_listen_fd);
        g_pgwire_listen_fd = -1;
        g_pgwire_running = 0;
        return NULL;
    }

    if (ar_socket_listen(g_pgwire_listen_fd, 128) != 0) {
        alri_print(RED "[ARDB]" RST " Failed to listen on PG-Wire port %d\n", port);
        ar_socket_close(g_pgwire_listen_fd);
        g_pgwire_listen_fd = -1;
        g_pgwire_running = 0;
        return NULL;
    }

    alri_print_force(CYN "[ARDB]" RST " Sovereign Data Guardian listening on PG-Wire port %d...\n", port);

    while (g_pgwire_running) {
        int client_fd = ar_socket_accept(g_pgwire_listen_fd);
        if (client_fd < 0) {
            if (g_pgwire_running) ar_sleep_ms(50);
            continue;
        }

        ar_thread_create(pgwire_client_handler, (void*)(intptr_t)client_fd);
    }

    if (g_pgwire_listen_fd >= 0) {
        ar_socket_close(g_pgwire_listen_fd);
        g_pgwire_listen_fd = -1;
    }
    return NULL;
}

int ardb_pgwire_server_start(int port) {
    if (g_pgwire_running) return 0;
    g_pgwire_running = 1;
    g_pgwire_listen_thread = ar_thread_create(pgwire_listen_worker, (void*)(intptr_t)port);
    return 0;
}

void ardb_pgwire_server_stop(void) {
    if (!g_pgwire_running) return;
    g_pgwire_running = 0;
    if (g_pgwire_listen_fd >= 0) {
        ar_socket_close(g_pgwire_listen_fd);
        g_pgwire_listen_fd = -1;
    }
}
