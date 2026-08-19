/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_UPSTREAM_H
#define ARWS_UPSTREAM_H

#include "aros_hal.h"
#include <stdint.h>

#define ARWS_MAX_NODES_PER_POOL 32
#define ARWS_MAX_POOLS 64

typedef enum {
    ARWS_LB_ROUND_ROBIN,
    ARWS_LB_WEIGHTED_ROUND_ROBIN,
    ARWS_LB_LEAST_CONN,
    ARWS_LB_IP_HASH
} ArwsLbAlgo;

typedef struct {
    char id[64];                  /* Ex: "srv-01" */
    char host[128];               /* IP ou Hostname */
    int port;                     /* Porta TCP */
    int weight;                   /* Peso configurado (1..100) */
    int effective_weight;         /* Peso dinâmico para Smooth WRR */
    int current_weight;           /* Peso acumulado para seleção */
    int active_conns;             /* Conexões ativas em andamento */

    /* Estado e Resiliência */
    int is_alive;                 /* 1 = Saudável, 0 = Down */
    int is_backup;                /* 1 = Contingência (usado se todos primários caírem) */
    int is_draining;              /* 1 = Em manutenção/drenagem (não recebe novas conns) */
    int fail_count;               /* Falhas consecutivas */
    int pass_count;               /* Sucessos consecutivos */
    uint64_t last_check_ms;       /* Timestamp da última checagem */

    /* Métricas / Auditoria */
    uint64_t total_requests;
    uint64_t total_errors;
} ArwsBackendNode;

typedef struct {
    char name[64];                /* Ex: "backend_gov" ou "srv_auth" */
    ArwsLbAlgo algo;              /* Algoritmo de Load Balancing */
    ArwsBackendNode nodes[ARWS_MAX_NODES_PER_POOL];
    int node_count;
    int rr_index;                 /* Para Round-Robin simples */

    /* Configuração de Health Check Ativo */
    char health_path[128];        /* Ex: "/healthz" */
    int check_interval_ms;        /* Intervalo entre checagens (ex: 5000) */
    int check_timeout_ms;         /* Timeout da sonda (ex: 2000) */
    int fall_threshold;           /* Falhas para considerar DOWN (ex: 3) */
    int rise_threshold;           /* Sucessos para recuperar UP (ex: 2) */

    void *mutex;                  /* Mutex para acesso concorrente seguro */
    int active;
} ArwsUpstreamPool;

/* Inicialização e Gestão de Ciclo de Vida */
void arws_upstream_init(void);
void arws_upstream_cleanup(void);

/* Gestão de Pools e Nós */
int arws_upstream_create_pool(const char *name, ArwsLbAlgo algo);
int arws_upstream_set_health_params(const char *pool_name, const char *health_path,
                                    int interval_ms, int timeout_ms,
                                    int fall_thresh, int rise_thresh);
int arws_upstream_add_node(const char *pool_name, const char *host, int port,
                           int weight, int is_backup);
int arws_upstream_remove_node(const char *pool_name, const char *host, int port);
int arws_upstream_set_node_drain(const char *pool_name, const char *host, int port, int drain);

/* Seleção de Backend e Liberação com Feedback */
ArwsBackendNode* arws_upstream_select(const char *pool_name, const char *client_ip);
void arws_upstream_release(const char *pool_name, ArwsBackendNode *node, int status_code);

/* Consulta e Status */
ArwsUpstreamPool* arws_upstream_get_pool(const char *pool_name);
int arws_upstream_get_all_pools(ArwsUpstreamPool *out_pools, int max_pools);

#endif /* ARWS_UPSTREAM_H */
