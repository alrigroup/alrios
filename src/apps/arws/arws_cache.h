/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_CACHE_H
#define ARWS_CACHE_H

#define ARWS_CACHE_MAX_ENTRIES 256
#define ARWS_CACHE_DEFAULT_TTL 60

void arws_cache_init(void);
void arws_cache_set_ttl(int seconds);
int  arws_cache_get_ttl(void);
int  arws_cache_get(const char *key, unsigned char **out_data, int *out_len);
int  arws_cache_set(const char *key, const unsigned char *data, int len);
void arws_cache_clear(void);
void arws_cache_cleanup(void);
void arws_cache_make_key(char *out, int out_size, const char *method,
                         const char *host, const char *path);

#endif
