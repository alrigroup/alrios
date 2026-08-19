/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_upstream.h"
#include "log.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static ArwsUpstreamPool g_pools[ARWS_MAX_POOLS];
static void *g_upstream_global_mutex = NULL;
static int g_upstream_initialized = 0;

void arws_upstream_init(void) {
    if (g_upstream_initialized) return;
    g_upstream_global_mutex = ar_mutex_create();
    ar_mutex_lock(g_upstream_global_mutex);
    memset(g_pools, 0, sizeof(g_pools));
    for (int i = 0; i < ARWS_MAX_POOLS; i++) {
        g_pools[i].mutex = ar_mutex_create();
        g_pools[i].active = 0;
    }
    g_upstream_initialized = 1;
    ar_mutex_unlock(g_upstream_global_mutex);
}

void arws_upstream_cleanup(void) {
    if (!g_upstream_initialized) return;
    ar_mutex_lock(g_upstream_global_mutex);
    for (int i = 0; i < ARWS_MAX_POOLS; i++) {
        if (g_pools[i].mutex) {
            ar_mutex_destroy(g_pools[i].mutex);
            g_pools[i].mutex = NULL;
        }
        g_pools[i].active = 0;
    }
    g_upstream_initialized = 0;
    ar_mutex_unlock(g_upstream_global_mutex);
    if (g_upstream_global_mutex) {
        ar_mutex_destroy(g_upstream_global_mutex);
        g_upstream_global_mutex = NULL;
    }
}

int arws_upstream_create_pool(const char *name, ArwsLbAlgo algo) {
    if (!name || name[0] == '\0') return -1;
    arws_upstream_init();

    ar_mutex_lock(g_upstream_global_mutex);
    /* Verificar se já existe */
    for (int i = 0; i < ARWS_MAX_POOLS; i++) {
        if (g_pools[i].active && strcmp(g_pools[i].name, name) == 0) {
            g_pools[i].algo = algo;
            ar_mutex_unlock(g_upstream_global_mutex);
            return 0;
        }
    }

    /* Encontrar slot livre */
    for (int i = 0; i < ARWS_MAX_POOLS; i++) {
        if (!g_pools[i].active) {
            ArwsUpstreamPool *p = &g_pools[i];
            ar_mutex_lock(p->mutex);
            p->active = 1;
            strncpy(p->name, name, sizeof(p->name) - 1);
            p->algo = algo;
            p->node_count = 0;
            p->rr_index = 0;
            strncpy(p->health_path, "/healthz", sizeof(p->health_path) - 1);
            p->check_interval_ms = 5000;
            p->check_timeout_ms = 2000;
            p->fall_threshold = 3;
            p->rise_threshold = 2;
            ar_mutex_unlock(p->mutex);
            ar_mutex_unlock(g_upstream_global_mutex);
            return 0;
        }
    }

    ar_mutex_unlock(g_upstream_global_mutex);
    return -1; /* Limite de pools atingido */
}

int arws_upstream_set_health_params(const char *pool_name, const char *health_path,
                                    int interval_ms, int timeout_ms,
                                    int fall_thresh, int rise_thresh) {
    ArwsUpstreamPool *pool = arws_upstream_get_pool(pool_name);
    if (!pool) return -1;

    ar_mutex_lock(pool->mutex);
    if (health_path && health_path[0]) {
        strncpy(pool->health_path, health_path, sizeof(pool->health_path) - 1);
    }
    if (interval_ms > 0) pool->check_interval_ms = interval_ms;
    if (timeout_ms > 0) pool->check_timeout_ms = timeout_ms;
    if (fall_thresh > 0) pool->fall_threshold = fall_thresh;
    if (rise_thresh > 0) pool->rise_threshold = rise_thresh;
    ar_mutex_unlock(pool->mutex);
    return 0;
}

int arws_upstream_add_node(const char *pool_name, const char *host, int port,
                           int weight, int is_backup) {
    if (!pool_name || !host || port <= 0) return -1;
    ArwsUpstreamPool *pool = arws_upstream_get_pool(pool_name);
    if (!pool) {
        if (arws_upstream_create_pool(pool_name, ARWS_LB_WEIGHTED_ROUND_ROBIN) != 0) {
            return -1;
        }
        pool = arws_upstream_get_pool(pool_name);
        if (!pool) return -1;
    }

    if (weight <= 0) weight = 1;
    if (weight > 100) weight = 100;

    ar_mutex_lock(pool->mutex);
    /* Verificar se já existe */
    for (int i = 0; i < pool->node_count; i++) {
        if (strcmp(pool->nodes[i].host, host) == 0 && pool->nodes[i].port == port) {
            pool->nodes[i].weight = weight;
            pool->nodes[i].effective_weight = weight;
            pool->nodes[i].is_backup = is_backup;
            ar_mutex_unlock(pool->mutex);
            return 0;
        }
    }

    if (pool->node_count >= ARWS_MAX_NODES_PER_POOL) {
        ar_mutex_unlock(pool->mutex);
        return -1; /* Capacidade máxima por pool atingida */
    }

    ArwsBackendNode *n = &pool->nodes[pool->node_count++];
    memset(n, 0, sizeof(ArwsBackendNode));
    snprintf(n->id, sizeof(n->id), "%s:%d", host, port);
    strncpy(n->host, host, sizeof(n->host) - 1);
    n->port = port;
    n->weight = weight;
    n->effective_weight = weight;
    n->current_weight = 0;
    n->active_conns = 0;
    n->is_alive = 1; /* Inicia como ativo */
    n->is_backup = is_backup;
    n->is_draining = 0;

    ar_mutex_unlock(pool->mutex);
    return 0;
}

int arws_upstream_remove_node(const char *pool_name, const char *host, int port) {
    ArwsUpstreamPool *pool = arws_upstream_get_pool(pool_name);
    if (!pool) return -1;

    ar_mutex_lock(pool->mutex);
    int idx = -1;
    for (int i = 0; i < pool->node_count; i++) {
        if (strcmp(pool->nodes[i].host, host) == 0 && pool->nodes[i].port == port) {
            idx = i;
            break;
        }
    }

    if (idx != -1) {
        for (int i = idx; i < pool->node_count - 1; i++) {
            pool->nodes[i] = pool->nodes[i + 1];
        }
        pool->node_count--;
        ar_mutex_unlock(pool->mutex);
        return 0;
    }

    ar_mutex_unlock(pool->mutex);
    return -1;
}

int arws_upstream_set_node_drain(const char *pool_name, const char *host, int port, int drain) {
    ArwsUpstreamPool *pool = arws_upstream_get_pool(pool_name);
    if (!pool) return -1;

    ar_mutex_lock(pool->mutex);
    for (int i = 0; i < pool->node_count; i++) {
        if (strcmp(pool->nodes[i].host, host) == 0 && pool->nodes[i].port == port) {
            pool->nodes[i].is_draining = drain ? 1 : 0;
            ar_mutex_unlock(pool->mutex);
            return 0;
        }
    }
    ar_mutex_unlock(pool->mutex);
    return -1;
}

ArwsUpstreamPool* arws_upstream_get_pool(const char *pool_name) {
    if (!pool_name || pool_name[0] == '\0') return NULL;
    arws_upstream_init();

    for (int i = 0; i < ARWS_MAX_POOLS; i++) {
        if (g_pools[i].active && strcmp(g_pools[i].name, pool_name) == 0) {
            return &g_pools[i];
        }
    }
    return NULL;
}

int arws_upstream_get_all_pools(ArwsUpstreamPool *out_pools, int max_pools) {
    if (!out_pools || max_pools <= 0) return 0;
    arws_upstream_init();

    int count = 0;
    ar_mutex_lock(g_upstream_global_mutex);
    for (int i = 0; i < ARWS_MAX_POOLS && count < max_pools; i++) {
        if (g_pools[i].active) {
            ar_mutex_lock(g_pools[i].mutex);
            out_pools[count++] = g_pools[i];
            ar_mutex_unlock(g_pools[i].mutex);
        }
    }
    ar_mutex_unlock(g_upstream_global_mutex);
    return count;
}

/* ========================================================================= */
/* ALGORITMOS DE LOAD BALANCING                                              */
/* ========================================================================= */

/* 1. Smooth Weighted Round-Robin (Padrão Nginx / RFC Alta Concorrência) */
static ArwsBackendNode* select_wrr_locked(ArwsUpstreamPool *pool, int use_backup) {
    ArwsBackendNode *best = NULL;
    int total_weight = 0;

    for (int i = 0; i < pool->node_count; i++) {
        ArwsBackendNode *n = &pool->nodes[i];
        if (!n->is_alive || n->is_draining) continue;
        if (n->is_backup != use_backup) continue;

        n->current_weight += n->effective_weight;
        total_weight += n->effective_weight;

        if (n->effective_weight < n->weight) {
            n->effective_weight++;
        }

        if (best == NULL || n->current_weight > best->current_weight) {
            best = n;
        }
    }

    if (best != NULL) {
        best->current_weight -= total_weight;
        best->active_conns++;
        best->total_requests++;
    }

    return best;
}

/* 2. Least Connections (Direciona para menor número de conexões ativas) */
static ArwsBackendNode* select_least_conn_locked(ArwsUpstreamPool *pool, int use_backup) {
    ArwsBackendNode *best = NULL;

    for (int i = 0; i < pool->node_count; i++) {
        ArwsBackendNode *n = &pool->nodes[i];
        if (!n->is_alive || n->is_draining) continue;
        if (n->is_backup != use_backup) continue;

        if (best == NULL || n->active_conns < best->active_conns) {
            best = n;
        }
    }

    if (best != NULL) {
        best->active_conns++;
        best->total_requests++;
    }
    return best;
}

/* 3. IP Consistent Hashing */
static unsigned int hash_ip(const char *ip) {
    unsigned int hash = 5381;
    int c;
    if (ip) {
        while ((c = *ip++)) hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

static ArwsBackendNode* select_ip_hash_locked(ArwsUpstreamPool *pool, const char *client_ip, int use_backup) {
    ArwsBackendNode *candidates[ARWS_MAX_NODES_PER_POOL];
    int cand_count = 0;

    for (int i = 0; i < pool->node_count; i++) {
        ArwsBackendNode *n = &pool->nodes[i];
        if (!n->is_alive || n->is_draining) continue;
        if (n->is_backup != use_backup) continue;
        candidates[cand_count++] = n;
    }

    if (cand_count == 0) return NULL;

    unsigned int h = hash_ip(client_ip);
    ArwsBackendNode *selected = candidates[h % cand_count];
    selected->active_conns++;
    selected->total_requests++;
    return selected;
}

/* 4. Round Robin Simples */
static ArwsBackendNode* select_rr_locked(ArwsUpstreamPool *pool, int use_backup) {
    for (int attempts = 0; attempts < pool->node_count; attempts++) {
        int idx = (pool->rr_index++) % pool->node_count;
        ArwsBackendNode *n = &pool->nodes[idx];
        if (n->is_alive && !n->is_draining && n->is_backup == use_backup) {
            n->active_conns++;
            n->total_requests++;
            return n;
        }
    }
    return NULL;
}

ArwsBackendNode* arws_upstream_select(const char *pool_name, const char *client_ip) {
    ArwsUpstreamPool *pool = arws_upstream_get_pool(pool_name);
    if (!pool || pool->node_count == 0) return NULL;

    ar_mutex_lock(pool->mutex);

    ArwsBackendNode *selected = NULL;

    /* Tentar nós primários (use_backup = 0) */
    switch (pool->algo) {
        case ARWS_LB_ROUND_ROBIN:
            selected = select_rr_locked(pool, 0);
            break;
        case ARWS_LB_LEAST_CONN:
            selected = select_least_conn_locked(pool, 0);
            break;
        case ARWS_LB_IP_HASH:
            selected = select_ip_hash_locked(pool, client_ip, 0);
            break;
        case ARWS_LB_WEIGHTED_ROUND_ROBIN:
        default:
            selected = select_wrr_locked(pool, 0);
            break;
    }

    /* Se todos primários estiverem fora, tentar nós de backup (use_backup = 1) */
    if (!selected) {
        switch (pool->algo) {
            case ARWS_LB_ROUND_ROBIN:
                selected = select_rr_locked(pool, 1);
                break;
            case ARWS_LB_LEAST_CONN:
                selected = select_least_conn_locked(pool, 1);
                break;
            case ARWS_LB_IP_HASH:
                selected = select_ip_hash_locked(pool, client_ip, 1);
                break;
            case ARWS_LB_WEIGHTED_ROUND_ROBIN:
            default:
                selected = select_wrr_locked(pool, 1);
                break;
        }
    }

    ar_mutex_unlock(pool->mutex);
    return selected;
}

void arws_upstream_release(const char *pool_name, ArwsBackendNode *node, int status_code) {
    if (!node) return;
    ArwsUpstreamPool *pool = arws_upstream_get_pool(pool_name);
    if (!pool) return;

    ar_mutex_lock(pool->mutex);
    if (node->active_conns > 0) {
        node->active_conns--;
    }

    /* Circuit Breaker Passivo: se status_code for 502, 503, 504 ou timeout (código < 0) */
    if (status_code >= 500 || status_code < 0) {
        node->total_errors++;
        node->fail_count++;
        if (node->effective_weight > 1) {
            node->effective_weight /= 2; /* Reduz peso temporariamente sob estresse */
        }
        if (node->fail_count >= pool->fall_threshold) {
            node->is_alive = 0; /* Isola o nó automaticamente */
            alri_print(RED "[ARWS-LB]" RST " Node %s:%d marked DOWN (Circuit Breaker triggered, fails=%d)\n",
                       node->host, node->port, node->fail_count);
        }
    } else {
        /* Sucesso */
        node->fail_count = 0;
        node->pass_count++;
        if (node->pass_count >= pool->rise_threshold) {
            node->is_alive = 1;
            node->effective_weight = node->weight; /* Restaura peso */
        }
    }

    ar_mutex_unlock(pool->mutex);
}
