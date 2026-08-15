/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_SESSION_H
#define ARWS_SESSION_H

#include <stdint.h>

typedef struct {
    char token[128];
    char user[64];
    char ip[64];
    int role;
    uint64_t created_at;
    uint64_t expires_at;
    int valid;
} ArwsSession;

void   arws_session_init(void);
ArwsSession* arws_session_create(const char *user, const char *ip, int role, int ttl_seconds);
ArwsSession* arws_session_verify(const char *token, const char *ip);
int    arws_session_destroy(const char *token);
void   arws_session_cleanup(void);
void arws_session_generate_token(char *buf, int len);

#endif
