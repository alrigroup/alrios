/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef ARWS_CONFIG_H
#define ARWS_CONFIG_H

#define MODE_PRODUCTION  "production"
#define MODE_TEST        "test"
#define MODE_MAINTENANCE "maintenance"

#define MAX_OVERRIDES       128
#define MAX_MAINTENANCE_IPS 32

typedef enum {
    ARWS_MODE_TEST       = 0,
    ARWS_MODE_PRODUCTION = 1
} ArwsMode;

ArwsMode     arws_config_get_mode(void);
int          arws_config_get_port(void);
void         arws_config_set_port(int p);
int          arws_config_get_operation_mode(void);
const char*  arws_config_get_mode_name(void);
const char*  arws_config_get_bind_address(void);

int arws_config_load(const char *path);
const char* arws_config_get_path(void);
int arws_config_reload_from_disk(void);
void arws_config_watchdog_stop(void);

const char* arws_config_get_effective_mode(const char *host, const char *path, const char *client_ip);
int  arws_config_is_maintenance_ip(const char *client_ip);
int  arws_config_add_override(const char *host, const char *path, const char *mode);
int  arws_config_remove_override(const char *host, const char *path);
int  arws_config_set_global(const char *mode);
const char* arws_config_get_global_mode(void);
int  arws_config_has_override(const char *host, const char *path);
int  arws_config_get_cache_ttl(void);
int  arws_config_set_cache_ttl(int seconds);
int  arws_config_is_no_cache(const char *host, const char *path);
int  arws_config_add_proxy_route(const char *host, const char *path, const char *target_url);
int  arws_config_has_proxy_route(const char *host, const char *path);
int  arws_config_add_stream_route(const char *host, const char *path, const char *target_url);
int  arws_config_has_stream_route(const char *host, const char *path);
int  arws_config_dump_routes(char *out, int size);

#endif
