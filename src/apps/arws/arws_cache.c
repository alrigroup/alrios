/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_cache.h"
#include "arws_gateway.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CACHE_SHARDS 16

typedef struct {
    char key[512];
    unsigned char *data;
    int len;
    uint64_t stored_at;
    int ttl;
    int in_use;
} CacheEntry;

static CacheEntry cache[ARWS_CACHE_MAX_ENTRIES];
static void *shard_mutexes[CACHE_SHARDS];
static int cache_ttl = ARWS_CACHE_DEFAULT_TTL;

static unsigned int cache_hash(const char *key) {
    unsigned int hash = 5381;
    int c;
    while ((c = *key++)) hash = ((hash << 5) + hash) + c;
    return hash;
}

static unsigned int get_shard(const char *key) {
    return cache_hash(key) % CACHE_SHARDS;
}

static int find_slot(const char *key) {
    unsigned int h = cache_hash(key);
    unsigned int idx = h % ARWS_CACHE_MAX_ENTRIES;
    for (int i = 0; i < ARWS_CACHE_MAX_ENTRIES; i++) {
        unsigned int slot = (idx + i) % ARWS_CACHE_MAX_ENTRIES;
        if (!cache[slot].in_use) return slot;
        if (strcmp(cache[slot].key, key) == 0) return slot;
    }
    return -1;
}

static int evict_one(void) {
    uint64_t oldest_time = 0;
    int oldest_slot = -1;

    for (int i = 0; i < ARWS_CACHE_MAX_ENTRIES; i++) {
        if (!cache[i].in_use) continue;
        if (cache[i].ttl > 0 &&
            (ar_time_ms() - cache[i].stored_at) > (uint64_t)cache[i].ttl * 1000) {
            free(cache[i].data);
            memset(&cache[i], 0, sizeof(CacheEntry));
            return i;
        }
        if (oldest_slot < 0 || cache[i].stored_at < oldest_time) {
            oldest_time = cache[i].stored_at;
            oldest_slot = i;
        }
    }

    if (oldest_slot >= 0) {
        free(cache[oldest_slot].data);
        memset(&cache[oldest_slot], 0, sizeof(CacheEntry));
        return oldest_slot;
    }

    return -1;
}

void arws_cache_init(void) {
    for (int s = 0; s < CACHE_SHARDS; s++) {
        if (!shard_mutexes[s]) shard_mutexes[s] = ar_mutex_create();
        ar_mutex_lock(shard_mutexes[s]);
    }
    memset(cache, 0, sizeof(cache));
    for (int s = 0; s < CACHE_SHARDS; s++) {
        ar_mutex_unlock(shard_mutexes[s]);
    }
    alri_print(CYN "[ARWS]" RST " Cache initialized (shards=%d max=%d ttl=%ds)\n",
               CACHE_SHARDS, ARWS_CACHE_MAX_ENTRIES, cache_ttl);
}

void arws_cache_set_ttl(int seconds) {
    cache_ttl = seconds > 0 ? seconds : 0;
}

int arws_cache_get_ttl(void) {
    return cache_ttl;
}

int arws_cache_get(const char *key, unsigned char **out_data, int *out_len) {
    if (!key || !out_data || !out_len) return -1;

    unsigned int shard = get_shard(key);
    if (!shard_mutexes[shard]) shard_mutexes[shard] = ar_mutex_create();
    ar_mutex_lock(shard_mutexes[shard]);

    int slot = find_slot(key);
    if (slot < 0 || !cache[slot].in_use) {
        ar_mutex_unlock(shard_mutexes[shard]);
        return -1;
    }

    if (cache[slot].ttl > 0 &&
        (ar_time_ms() - cache[slot].stored_at) > (uint64_t)cache[slot].ttl * 1000) {
        free(cache[slot].data);
        memset(&cache[slot], 0, sizeof(CacheEntry));
        ar_mutex_unlock(shard_mutexes[shard]);
        return -1;
    }

    unsigned char *copy = (unsigned char *)ar_mem_alloc(cache[slot].len);
    if (!copy) {
        ar_mutex_unlock(shard_mutexes[shard]);
        return -1;
    }
    memcpy(copy, cache[slot].data, cache[slot].len);
    *out_data = copy;
    *out_len = cache[slot].len;
    ar_mutex_unlock(shard_mutexes[shard]);
    return 0;
}

int arws_cache_set(const char *key, const unsigned char *data, int len) {
    if (!key || !data || len <= 0) return -1;
    if (cache_ttl <= 0) return -1;

    unsigned int shard = get_shard(key);
    if (!shard_mutexes[shard]) shard_mutexes[shard] = ar_mutex_create();
    ar_mutex_lock(shard_mutexes[shard]);

    int slot = find_slot(key);
    if (slot < 0) {
        slot = evict_one();
        if (slot < 0) {
            ar_mutex_unlock(shard_mutexes[shard]);
            return -1;
        }
    }

    if (cache[slot].in_use)
        free(cache[slot].data);

    cache[slot].data = (unsigned char *)ar_mem_alloc(len);
    if (!cache[slot].data) {
        ar_mutex_unlock(shard_mutexes[shard]);
        return -1;
    }
    memcpy(cache[slot].data, data, len);
    strncpy(cache[slot].key, key, sizeof(cache[slot].key) - 1);
    cache[slot].key[sizeof(cache[slot].key) - 1] = '\0';
    cache[slot].len = len;
    cache[slot].stored_at = ar_time_ms();
    cache[slot].ttl = cache_ttl;
    cache[slot].in_use = 1;

    ar_mutex_unlock(shard_mutexes[shard]);
    return 0;
}

void arws_cache_clear(void) {
    for (int s = 0; s < CACHE_SHARDS; s++) {
        if (!shard_mutexes[s]) shard_mutexes[s] = ar_mutex_create();
        ar_mutex_lock(shard_mutexes[s]);
    }
    for (int i = 0; i < ARWS_CACHE_MAX_ENTRIES; i++) {
        if (cache[i].in_use) {
            free(cache[i].data);
        }
    }
    memset(cache, 0, sizeof(cache));
    for (int s = 0; s < CACHE_SHARDS; s++) {
        ar_mutex_unlock(shard_mutexes[s]);
    }
    alri_print(CYN "[ARWS]" RST " Cache cleared\n");
}

void arws_cache_cleanup(void) {
    uint64_t now = ar_time_ms();
    int removed = 0;

    for (int s = 0; s < CACHE_SHARDS; s++) {
        if (!shard_mutexes[s]) shard_mutexes[s] = ar_mutex_create();
        ar_mutex_lock(shard_mutexes[s]);
    }
    for (int i = 0; i < ARWS_CACHE_MAX_ENTRIES; i++) {
        if (!cache[i].in_use) continue;
        if (cache[i].ttl > 0 &&
            (now - cache[i].stored_at) > (uint64_t)cache[i].ttl * 1000) {
            free(cache[i].data);
            memset(&cache[i], 0, sizeof(CacheEntry));
            removed++;
        }
    }
    for (int s = 0; s < CACHE_SHARDS; s++) {
        ar_mutex_unlock(shard_mutexes[s]);
    }

    if (removed > 0)
        alri_print(CYN "[ARWS]" RST " Cache cleanup: removed %d expired entries\n", removed);
}

void arws_cache_make_key(char *out, int out_size, const char *method,
                         const char *host, const char *path) {
    snprintf(out, out_size, "%s|%s|%s",
             method ? method : "*",
             host ? host : "*",
             path ? path : "/");
}
