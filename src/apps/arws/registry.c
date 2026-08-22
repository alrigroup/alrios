/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#include "arws_gateway.h"
#include "ipc/ipc.h"
#include "ar_ipc.h"
#include "log.h"
#include "aros_hal.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static ArwsBackend backends[ARWS_MAX_BACKENDS];
static int backend_count = 0;
static void *registry_mutex = NULL;
static int next_backend_id = 1;

void arws_registry_init(void) {
    registry_mutex = ar_mutex_create();
    ar_mutex_lock(registry_mutex);
    memset(backends, 0, sizeof(backends));
    backend_count = 0;
    next_backend_id = 1;
    ar_mutex_unlock(registry_mutex);
}

int arws_backend_count(void) {
    ar_mutex_lock(registry_mutex);
    int cnt = backend_count;
    ar_mutex_unlock(registry_mutex);
    return cnt;
}

int arws_register_backend(const char *name, int fd) {
    if (!name || fd < 0) return -1;

    ar_mutex_lock(registry_mutex);
    for (int i = 0; i < backend_count; i++) {
        if (backends[i].registered && strcmp(backends[i].name, name) == 0) {
            backends[i].fd = fd;
            int id = backends[i].id;
            ar_mutex_unlock(registry_mutex);
            alri_print(CYN "[ARWS]" RST " Backend '%s' re-registered (id=%d, fd=%d)\n", name, id, fd);
            return id;
        }
    }

    if (backend_count >= ARWS_MAX_BACKENDS) {
        ar_mutex_unlock(registry_mutex);
        return -1;
    }

    int id = next_backend_id++;
    backends[backend_count].id = id;
    backends[backend_count].fd = fd;
    backends[backend_count].pid = 0;
    backends[backend_count].registered = 1;
    strncpy(backends[backend_count].name, name,
            sizeof(backends[backend_count].name) - 1);
    backends[backend_count].name[sizeof(backends[backend_count].name) - 1] = '\0';

    backend_count++;
    ar_mutex_unlock(registry_mutex);

    alri_print(CYN "[ARWS]" RST " Backend '%s' registered (id=%d, fd=%d)\n",
               name, id, fd);
    return id;
}

int arws_unregister_backend(int backend_id) {
    ar_mutex_lock(registry_mutex);
    for (int i = 0; i < backend_count; i++) {
        if (backends[i].id == backend_id) {
            backends[i] = backends[--backend_count];
            ar_mutex_unlock(registry_mutex);
            alri_print(CYN "[ARWS]" RST " Backend id=%d unregistered\n", backend_id);
            return 0;
        }
    }
    ar_mutex_unlock(registry_mutex);
    return -1;
}

void arws_close_all_backends(void) {
    ar_mutex_lock(registry_mutex);
    for (int i = 0; i < backend_count; i++) {
        if (backends[i].fd > 0) {
            ar_ipc_send_frame(backends[i].fd, IPC_SHUTDOWN, "shutdown", 8);
            ar_socket_close(backends[i].fd);
            backends[i].fd = -1;
        }
    }
    backend_count = 0;
    next_backend_id = 1;
    ar_mutex_unlock(registry_mutex);
}

ArwsBackend* arws_get_backend(int backend_id) {
    for (int i = 0; i < backend_count; i++) {
        if (backends[i].id == backend_id)
            return &backends[i];
    }
    return NULL;
}

int arws_find_backend_id_by_fd(int fd) {
    ar_mutex_lock(registry_mutex);
    for (int i = 0; i < backend_count; i++) {
        if (backends[i].fd == fd && backends[i].registered) {
            int id = backends[i].id;
            ar_mutex_unlock(registry_mutex);
            return id;
        }
    }
    ar_mutex_unlock(registry_mutex);
    return -1;
}

int arws_find_backend_by_name(const char *name) {
    ar_mutex_lock(registry_mutex);
    for (int i = 0; i < backend_count; i++) {
        if (strcmp(backends[i].name, name) == 0) {
            int fd = backends[i].fd;
            ar_mutex_unlock(registry_mutex);
            return fd;
        }
    }
    ar_mutex_unlock(registry_mutex);
    return -1;
}

int arws_get_backend_fd(int backend_id) {
    ar_mutex_lock(registry_mutex);
    for (int i = 0; i < backend_count; i++) {
        if (backends[i].id == backend_id) {
            int fd = backends[i].fd;
            ar_mutex_unlock(registry_mutex);
            return fd;
        }
    }
    ar_mutex_unlock(registry_mutex);
    return -1;
}

int arws_backend_send_request(int backend_id, const unsigned char *raw_req,
                              int raw_len) {
    int fd = arws_get_backend_fd(backend_id);
    if (fd < 0) return -1;
    return ar_ipc_send_raw(fd, raw_req, raw_len);
}

static int read_http_response(int fd, unsigned char *buf, int maxlen) {
    int total = 0;
    int header_end = -1;

    while (total < maxlen) {
        int n = ar_socket_recv(fd, buf + total, maxlen - total);
        if (n <= 0) break;
        total += n;

        if (header_end < 0) {
            for (int i = (total > 4 ? total - 4 : 0); i < total - 3; i++) {
                if (buf[i] == '\r' && buf[i+1] == '\n' &&
                    buf[i+2] == '\r' && buf[i+3] == '\n') {
                    header_end = i + 4;
                    break;
                }
            }
        }

        if (header_end >= 0) {
            int body_start = header_end;
            int body_so_far = total - body_start;

            const char *cl = strstr((const char *)buf, "Content-Length: ");
            if (!cl) cl = strstr((const char *)buf, "content-length: ");
            if (!cl) cl = strstr((const char *)buf, "Content-length: ");

            if (cl && cl < (const char *)buf + header_end) {
                int content_length = atoi(cl + 16);
                if (body_so_far >= content_length)
                    return total;
            } else {
                return total;
            }
        }
    }

    return total;
}

int arws_backend_read_response(int backend_id, unsigned char *buf, int maxlen) {
    int fd = arws_get_backend_fd(backend_id);
    if (fd < 0) return -1;
    return read_http_response(fd, buf, maxlen);
}
