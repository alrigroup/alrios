/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_session.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/rand.h>

#define ARWS_MAX_SESSIONS 1024

static int ct_compare(const void *a, const void *b, size_t len) {
    int diff = 0;
    const unsigned char *ca = (const unsigned char *)a;
    const unsigned char *cb = (const unsigned char *)b;
    for (size_t i = 0; i < len; i++)
        diff |= ca[i] ^ cb[i];
    return diff;
}

static int tokens_match(const char *a, const char *b) {
    if (!a || !b) return 0;
    size_t la = strlen(a), lb = strlen(b);
    if (la != lb) return 0;
    return ct_compare(a, b, la) == 0;
}

static ArwsSession sessions[ARWS_MAX_SESSIONS];
static int session_count = 0;
static void *session_mutex = NULL;

void arws_session_init(void) {
    session_mutex = ar_mutex_create();
    ar_mutex_lock(session_mutex);
    memset(sessions, 0, sizeof(sessions));
    session_count = 0;
    ar_mutex_unlock(session_mutex);
}

static void generate_random_token(char *buf, int len) {
    const char *chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    unsigned char raw[128];
    int token_len = len - 1;
    if (token_len > (int)sizeof(raw)) token_len = sizeof(raw);
    if (RAND_bytes(raw, token_len) != 1) {
        for (int i = 0; i < token_len; i++)
            raw[i] = (unsigned char)(ar_time_ms() & 0xFF);
    }
    for (int i = 0; i < token_len; i++)
        buf[i] = chars[raw[i] % 62];
    buf[token_len] = '\0';
}

void arws_session_generate_token(char *buf, int len) {
    generate_random_token(buf, len);
}

ArwsSession* arws_session_create(const char *user, const char *ip,
                                  int role, int ttl_seconds) {
    ar_mutex_lock(session_mutex);

    int idx = -1;
    for (int i = 0; i < ARWS_MAX_SESSIONS; i++) {
        if (!sessions[i].valid) { idx = i; break; }
    }
    if (idx == -1) {
        ar_mutex_unlock(session_mutex);
        return NULL;
    }

    char token[128];
    arws_session_generate_token(token, sizeof(token));
    strncpy(sessions[idx].token, token, sizeof(sessions[idx].token) - 1);
    strncpy(sessions[idx].user, user, sizeof(sessions[idx].user) - 1);
    strncpy(sessions[idx].ip, ip, sizeof(sessions[idx].ip) - 1);
    sessions[idx].role = role;
    sessions[idx].created_at = ar_time_ms() / 1000;
    sessions[idx].expires_at = sessions[idx].created_at + ttl_seconds;
    sessions[idx].valid = 1;
    session_count++;

    ar_mutex_unlock(session_mutex);
    return &sessions[idx];
}

ArwsSession* arws_session_verify(const char *token, const char *ip) {
    if (!token) return NULL;

    uint64_t now = ar_time_ms() / 1000;

    ar_mutex_lock(session_mutex);
    for (int i = 0; i < ARWS_MAX_SESSIONS; i++) {
        if (!sessions[i].valid) continue;
        if (!tokens_match(sessions[i].token, token)) continue;

        if (now >= sessions[i].expires_at) {
            sessions[i].valid = 0;
            session_count--;
            ar_mutex_unlock(session_mutex);
            return NULL;
        }

        if (ip && ip[0] && strcmp(sessions[i].ip, ip) != 0) {
            ar_mutex_unlock(session_mutex);
            return NULL;
        }

        ar_mutex_unlock(session_mutex);
        return &sessions[i];
    }
    ar_mutex_unlock(session_mutex);
    return NULL;
}

int arws_session_destroy(const char *token) {
    if (!token) return -1;

    ar_mutex_lock(session_mutex);
    for (int i = 0; i < ARWS_MAX_SESSIONS; i++) {
        if (sessions[i].valid && strcmp(sessions[i].token, token) == 0) {
            sessions[i].valid = 0;
            session_count--;
            ar_mutex_unlock(session_mutex);
            return 0;
        }
    }
    ar_mutex_unlock(session_mutex);
    return -1;
}

void arws_session_cleanup(void) {
    uint64_t now = ar_time_ms() / 1000;

    ar_mutex_lock(session_mutex);
    for (int i = 0; i < ARWS_MAX_SESSIONS; i++) {
        if (sessions[i].valid && now >= sessions[i].expires_at) {
            sessions[i].valid = 0;
            session_count--;
        }
    }
    ar_mutex_unlock(session_mutex);
}
