/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_ratelimit.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>

#define ARWS_RL_MAX_IPS 2000
#define ARWS_RL_MAX_RULES 64
/* Default (rede de seguranca): arws nunca decide sozinho o teto das apps;
   sem regra declarada via IPC_REGISTER rl=, aplica este limite alto. */
#define ARWS_RL_DEFAULT_MAX    5000
#define ARWS_RL_DEFAULT_WINDOW 60

typedef struct {
    char ip[64];
    int count;
    uint64_t window_start;
    int blocked;
    uint64_t blocked_until;
} RlEntry;

typedef struct {
    char host[256];
    char path_pattern[128];
    int max_requests;
    int window_seconds;
} RlRule;

static RlEntry rl_entries[ARWS_RL_MAX_IPS];
static RlRule rl_rules[ARWS_RL_MAX_RULES];
static int rl_rule_count = 0;
static void *rl_mutex = NULL;

void arws_ratelimit_init(void) {
    rl_mutex = ar_mutex_create();
    ar_mutex_lock(rl_mutex);
    memset(rl_entries, 0, sizeof(rl_entries));
    memset(rl_rules, 0, sizeof(rl_rules));
    rl_rule_count = 0;
    ar_mutex_unlock(rl_mutex);
}

void arws_ratelimit_set_rule(const char *host, const char *path_pattern,
                             int max_req, int window_sec) {
    if (!path_pattern) return;

    ar_mutex_lock(rl_mutex);

    for (int i = 0; i < rl_rule_count; i++) {
        if (strcmp(rl_rules[i].host, host ? host : "*") == 0 &&
            strcmp(rl_rules[i].path_pattern, path_pattern) == 0) {
            rl_rules[i].max_requests = max_req;
            rl_rules[i].window_seconds = window_sec;
            ar_mutex_unlock(rl_mutex);
            return;
        }
    }

    if (rl_rule_count >= ARWS_RL_MAX_RULES) {
        ar_mutex_unlock(rl_mutex);
        return;
    }

    RlRule *r = &rl_rules[rl_rule_count++];
    strncpy(r->host, host ? host : "*", sizeof(r->host) - 1);
    r->host[sizeof(r->host) - 1] = '\0';
    strncpy(r->path_pattern, path_pattern, sizeof(r->path_pattern) - 1);
    r->path_pattern[sizeof(r->path_pattern) - 1] = '\0';
    r->max_requests = max_req;
    r->window_seconds = window_sec;

    ar_mutex_unlock(rl_mutex);
}

/* prefix match; trailing '/*' behaves as wildcard ('/api/*' also matches
   '/api/v1'); '*' or '/' match every path */
static int path_matches(const char *pattern, const char *path) {
    size_t plen = strlen(pattern);
    if (plen == 0 || strcmp(pattern, "*") == 0) return 1;
    if (plen >= 2 && pattern[plen - 2] == '/' && pattern[plen - 1] == '*')
        plen -= 2;
    if (plen == 0) return 1;
    return strncmp(pattern, path, plen) == 0;
}

static int rule_matches(const RlRule *r, const char *host, const char *path) {
    if (r->host[0] != '\0' && strcmp(r->host, "*") != 0) {
        if (!host || strcmp(r->host, host) != 0) return 0;
    }
    return path_matches(r->path_pattern, path);
}

int arws_ratelimit_check(const char *ip, const char *host, const char *path) {
    if (!ip || !path) return 1;

    ar_mutex_lock(rl_mutex);

    if (arws_ratelimit_is_blocked(ip)) {
        ar_mutex_unlock(rl_mutex);
        return 0;
    }

    uint64_t now = ar_time_ms() / 1000;
    int entry_idx = -1;
    int empty_idx = -1;
    int oldest_idx = 0;
    uint64_t oldest_time = now;

    for (int i = 0; i < ARWS_RL_MAX_IPS; i++) {
        if (rl_entries[i].ip[0] == '\0') {
            if (empty_idx == -1) empty_idx = i;
        } else if (strcmp(rl_entries[i].ip, ip) == 0) {
            entry_idx = i;
            break;
        } else if (rl_entries[i].window_start < oldest_time) {
            oldest_time = rl_entries[i].window_start;
            oldest_idx = i;
        }
    }

    if (entry_idx == -1) {
        entry_idx = (empty_idx != -1) ? empty_idx : oldest_idx;
        strncpy(rl_entries[entry_idx].ip, ip, 63);
        rl_entries[entry_idx].ip[63] = '\0';
        rl_entries[entry_idx].count = 0;
        rl_entries[entry_idx].window_start = now;
        rl_entries[entry_idx].blocked = 0;
        rl_entries[entry_idx].blocked_until = 0;
    }

    int max_req = ARWS_RL_DEFAULT_MAX;
    int window = ARWS_RL_DEFAULT_WINDOW;

    for (int i = 0; i < rl_rule_count; i++) {
        if (rule_matches(&rl_rules[i], host, path)) {
            max_req = rl_rules[i].max_requests;
            window = rl_rules[i].window_seconds;
            break;
        }
    }

    if (now - rl_entries[entry_idx].window_start >= window) {
        rl_entries[entry_idx].window_start = now;
        rl_entries[entry_idx].count = 1;
    } else if (++rl_entries[entry_idx].count > max_req) {
        ar_mutex_unlock(rl_mutex);
        return 0;
    }

    ar_mutex_unlock(rl_mutex);
    return 1;
}

int arws_ratelimit_check_ip(const char *ip) {
    return arws_ratelimit_check(ip, NULL, "/");
}

void arws_ratelimit_block_ip(const char *ip, int seconds) {
    ar_mutex_lock(rl_mutex);
    uint64_t now = ar_time_ms() / 1000;

    for (int i = 0; i < ARWS_RL_MAX_IPS; i++) {
        if (rl_entries[i].ip[0] == '\0') continue;
        if (strcmp(rl_entries[i].ip, ip) == 0) {
            rl_entries[i].blocked = 1;
            rl_entries[i].blocked_until = now + seconds;
            ar_mutex_unlock(rl_mutex);
            return;
        }
    }

    for (int i = 0; i < ARWS_RL_MAX_IPS; i++) {
        if (rl_entries[i].ip[0] == '\0') {
            strncpy(rl_entries[i].ip, ip, 63);
            rl_entries[i].ip[63] = '\0';
            rl_entries[i].blocked = 1;
            rl_entries[i].blocked_until = now + seconds;
            rl_entries[i].count = 0;
            rl_entries[i].window_start = now;
            break;
        }
    }
    ar_mutex_unlock(rl_mutex);
}

int arws_ratelimit_is_blocked(const char *ip) {
    uint64_t now = ar_time_ms() / 1000;

    for (int i = 0; i < ARWS_RL_MAX_IPS; i++) {
        if (rl_entries[i].ip[0] == '\0') continue;
        if (strcmp(rl_entries[i].ip, ip) == 0) {
            if (!rl_entries[i].blocked) return 0;
            if (now >= rl_entries[i].blocked_until) {
                rl_entries[i].blocked = 0;
                rl_entries[i].count = 0;
                return 0;
            }
            return 1;
        }
    }
    return 0;
}
