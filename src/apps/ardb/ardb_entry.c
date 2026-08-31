/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "ardb_config.h"
#include "ardb_http.h"
#include "ardb_pgwire.h"
#include "ardb_backend.h"
#include "ardb_auth.h"
#include "ardb_firewall.h"
#include "ardb_audit.h"
#include "aros_hal.h"
#include "ar_ipc.h"
#include "log.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>

static volatile int g_app_running = 1;
static int g_ipc_fd = -1;

static void handle_sig(int s) {
    (void)s;
    g_app_running = 0;
    ardb_http_server_stop();
    ardb_pgwire_server_stop();
}

/* Handle IPC queries received from CLI 'alrios ardb <cmd>' */
static void handle_ardb_ipc_query(int fd, const char *payload, int payload_len) {
    char resp[AR_IPC_BUF_SIZE];
    memset(resp, 0, sizeof(resp));
    int rlen = 0;

    char q[512] = {0};
    int qlen = payload_len < (int)sizeof(q) - 1 ? payload_len : (int)sizeof(q) - 1;
    memcpy(q, payload, qlen);
    q[qlen] = '\0';

    const char *p = q;
    while (*p == ' ' || *p == '\r' || *p == '\n' || *p == '\t') p++;

    char cmd[64] = {0};
    int i = 0;
    while (p[i] && p[i] != ' ' && p[i] != '\n' && p[i] != '\r' && i < 63) {
        cmd[i] = p[i];
        i++;
    }
    cmd[i] = '\0';
    const char *args = p + i;
    while (*args == ' ') args++;

    ArdbConfig *cfg = ardb_config_get();

    if (strcmp(cmd, "status") == 0) {
        rlen = snprintf(resp, sizeof(resp),
            "[ALRI DB] Sovereign Database Guardian Status:\n"
            "  State: RUNNING\n"
            "  PG-Wire Port: %d (Active)\n"
            "  HTTP REST API: %s (Port %d, Route: %s)\n"
            "  SQL Firewall: %s\n"
            "  Forensic Audit Log: %s\n",
            cfg->server_port,
            ardb_http_is_running() ? "ENABLED" : "DISABLED",
            cfg->http_port, cfg->http_route_prefix,
            cfg->sql_firewall,
            cfg->audit_log);
    } else if (strcmp(cmd, "cfg") == 0) {
        char sub[32] = {0};
        sscanf(args, "%31s", sub);
        if (strcmp(sub, "reload") == 0) {
            ardb_config_reload(NULL);
            ArdbConfig *new_cfg = ardb_config_get();
            if (new_cfg->http_enabled && !ardb_http_is_running()) {
                ardb_http_server_start(new_cfg->http_bind, new_cfg->http_port);
            } else if (!new_cfg->http_enabled && ardb_http_is_running()) {
                ardb_http_server_stop();
            }
            rlen = snprintf(resp, sizeof(resp),
                "[ALRI DB] Configuration reloaded successfully.\n"
                "  HTTP REST API: %s (Port %d)\n",
                ardb_http_is_running() ? "ENABLED" : "DISABLED", new_cfg->http_port);
        } else {
            rlen = snprintf(resp, sizeof(resp), "usage: alrios ardb cfg reload");
        }
    } else if (strcmp(cmd, "auth") == 0) {
        char sub[32] = {0};
        char target[128] = {0};
        int parsed = sscanf(args, "%31s %127s", sub, target);

        if (strcmp(sub, "login") == 0 && parsed >= 2) {
            char tok[128] = {0};
            int ok = ardb_auth_generate_token(target, NULL, NULL, 14400, tok, sizeof(tok));
            if (ok == 0) {
                rlen = snprintf(resp, sizeof(resp),
                    "[ALRI DB] Authenticated user '%s' successfully!\n"
                    "  Session Token: %s\n"
                    "  TTL: 4 hours (Zero-Knowledge Session)\n"
                    "  Host: 127.0.0.1 | Port: %d | Database: alrios_db\n\n"
                    "Use this token as the password in DBeaver, or in X-ARDB-Token HTTP Header.\n",
                    target, tok, cfg->server_port);
            } else {
                rlen = snprintf(resp, sizeof(resp), "[ALRI DB] Error: User '%s' not found or inactive.\n", target);
            }
        } else if (strcmp(sub, "revoke") == 0 && parsed >= 2) {
            int ok = ardb_auth_revoke_token(target);
            if (ok == 0) {
                rlen = snprintf(resp, sizeof(resp), "[ALRI DB] Session token revoked successfully.\n");
            } else {
                rlen = snprintf(resp, sizeof(resp), "[ALRI DB] Error: Token not found or already expired.\n");
            }
        } else {
            rlen = snprintf(resp, sizeof(resp), "usage: auth <login|revoke> <user|token>");
        }
    } else if (strcmp(cmd, "user") == 0) {
        char sub[32] = {0};
        char u[64] = {0}, p[64] = {0}, t[64] = {0}, r[32] = "operator";
        int parsed = sscanf(args, "%31s %63s %63s %63s %31s", sub, u, p, t, r);
        if (strcmp(sub, "add") == 0 && parsed >= 4) {
            ardb_auth_add_user(u, p, t, r);
            rlen = snprintf(resp, sizeof(resp), "[ALRI DB] User '%s' created for tenant '%s' with role '%s'.\n", u, t, r);
        } else {
            rlen = snprintf(resp, sizeof(resp), "usage: user add <user> <pass> <tenant> [role]");
        }
    } else if (strcmp(cmd, "app") == 0) {
        char sub[32] = {0};
        char app_name[64] = {0}, token[128] = {0}, grp[64] = {0}, tables[256] = {0};
        int parsed = sscanf(args, "%31s %63s %127s %63s %255s", sub, app_name, token, grp, tables);
        if (strcmp(sub, "add") == 0 && parsed >= 3) {
            ardb_auth_add_app(app_name, token, (parsed >= 4 && strcmp(grp, "-") != 0) ? grp : NULL, (parsed >= 5) ? tables : NULL);
            rlen = snprintf(resp, sizeof(resp),
                "[ALRI DB] App '%s' credentials provisioned.\n"
                "  Assigned Group: %s\n"
                "  Table Scope: %s\n",
                app_name, (parsed >= 4 && strcmp(grp, "-") != 0) ? grp : "none", (parsed >= 5) ? tables : "app_isolated (*)");
        } else {
            rlen = snprintf(resp, sizeof(resp), "usage: app add <app_name> <token/hash> [group|-] [allowed_tables_csv]");
        }
    } else if (strcmp(cmd, "group") == 0) {
        char sub[32] = {0};
        char gname[64] = {0}, extra[256] = {0};
        int parsed = sscanf(args, "%31s %63s %255s", sub, gname, extra);
        if (strcmp(sub, "create") == 0 && parsed >= 2) {
            ardb_auth_create_group(gname, (parsed >= 3) ? extra : NULL);
            rlen = snprintf(resp, sizeof(resp), "[ALRI DB] App Group '%s' created with shared tables: %s\n", gname, (parsed >= 3) ? extra : "none");
        } else if (strcmp(sub, "add-app") == 0 && parsed >= 3) {
            ardb_auth_add_app_to_group(gname, extra);
            rlen = snprintf(resp, sizeof(resp), "[ALRI DB] Added app '%s' to App Group '%s'.\n", extra, gname);
        } else {
            rlen = snprintf(resp, sizeof(resp), "usage: group <create|add-app> <group_name> <tables_csv|app_name>");
        }
    } else if (strcmp(cmd, "audit") == 0) {
        char sub[32] = {0};
        sscanf(args, "%31s", sub);
        if (strcmp(sub, "verify") == 0) {
            char err[256] = {0};
            int ok = ardb_audit_verify_integrity(cfg->audit_log, err, sizeof(err));
            if (ok == 0) {
                rlen = snprintf(resp, sizeof(resp),
                    "[ALRI DB] Auditing forensic integrity chain...\n"
                    "  [PASS] 100%% of cryptographic log hashes verified. Zero tampering detected.\n");
            } else {
                rlen = snprintf(resp, sizeof(resp),
                    "[ALRI DB] Auditing forensic integrity chain...\n"
                    "  [FAIL] Cryptographic log hash chain error: %s\n", err);
            }
        } else {
            rlen = snprintf(resp, sizeof(resp),
                "[ALRI DB] Streaming forensic audit logs (tail):\n"
                "  [LIVE] Listening for PG-Wire connections...\n");
        }
    } else if (strcmp(cmd, "ping") == 0) {
        rlen = snprintf(resp, sizeof(resp), "pong");
    } else if (strcmp(cmd, "help") == 0 || cmd[0] == '\0') {
        rlen = snprintf(resp, sizeof(resp),
            "ALRI DB Sovereign Guardian commands:\n"
            "  alrios ardb status\n"
            "  alrios ardb auth login <user>\n"
            "  alrios ardb auth revoke <token>\n"
            "  alrios ardb user add <user> <pass> <tenant> [role]\n"
            "  alrios ardb app add <app_name> <token> [group|-] [tables_csv]\n"
            "  alrios ardb group create <group_name> <tables_csv>\n"
            "  alrios ardb group add-app <group_name> <app_name>\n"
            "  alrios ardb audit verify\n"
            "  alrios ardb cfg reload\n");
    } else {
        rlen = snprintf(resp, sizeof(resp), "unknown ardb command: %s (run 'alrios ardb help')", cmd);
    }

    if (rlen < 0) rlen = 0;
    if (rlen >= (int)sizeof(resp)) rlen = (int)sizeof(resp) - 1;

    ar_ipc_send_frame(fd, IPC_QUERY_RESP, resp, (uint32_t)rlen + 1);
}

static void *ardb_ipc_thread(void *arg) {
    (void)arg;
    while (g_app_running) {
        g_ipc_fd = ar_ipc_client_connect("127.0.0.1", AR_IPC_DEFAULT_PORT);
        if (g_ipc_fd < 0) {
            ar_sleep_ms(1000);
            continue;
        }

        const char *reg_frame = "ardb /ardb-internal * * production";
        ar_ipc_send_frame(g_ipc_fd, IPC_REGISTER, reg_frame, (uint32_t)strlen(reg_frame));

        char buf[AR_IPC_BUF_SIZE];
        while (g_app_running) {
            int type = 0;
            uint32_t len = sizeof(buf);
            if (ar_ipc_recv_frame(g_ipc_fd, &type, buf, &len) < 0) {
                break;
            }

            if (type == IPC_QUERY) {
                handle_ardb_ipc_query(g_ipc_fd, buf, (int)len);
            } else if (type == IPC_HEARTBEAT) {
                ar_ipc_send_frame(g_ipc_fd, IPC_ACK, "ACK", 4);
            }
        }

        ar_socket_close(g_ipc_fd);
        g_ipc_fd = -1;
        ar_sleep_ms(1000);
    }
    return NULL;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    signal(SIGINT, handle_sig);
    signal(SIGTERM, handle_sig);

    alri_print_force(CYN "[ALRI DB]" RST " Starting Sovereign Database Guardian Service...\n");

    /* Load ardb.cfg */
    ardb_config_load(NULL, ardb_config_get());
    ArdbConfig *cfg = ardb_config_get();

    /* Initialize security modules */
    ardb_auth_init();
    ardb_audit_init(cfg->audit_log);
    ardb_backend_init(cfg->backend_host, cfg->backend_port, cfg->backend_user, cfg->backend_password, cfg->backend_database);

    /* Start PG-Wire server */
    if (ardb_pgwire_server_start(cfg->server_port) != 0) {
        alri_print(RED "[ALRI DB]" RST " Fatal: Unable to bind PG-Wire server on port %d\n", cfg->server_port);
        return 1;
    }

    /* Start optional HTTP REST server if configured */
    if (cfg->http_enabled) {
        ardb_http_server_start(cfg->http_bind, cfg->http_port);
    }

    /* Start IPC control channel thread */
    ar_thread_create(ardb_ipc_thread, NULL);

    alri_print_force(GRN "[ALRI DB]" RST " Sovereign Data Guardian is ACTIVE and PROTECTED.\n");

    while (g_app_running) {
        ar_sleep_ms(250);
    }

    alri_print_force("[ALRI DB] Stopping Sovereign Database Guardian gracefully...\n");
    ardb_http_server_stop();
    ardb_pgwire_server_stop();
    ardb_audit_cleanup();
    return 0;
}
