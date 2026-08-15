/*
 * Copyright (c) ALRIGROUP and its affiliates.
 *
 * This code is licensed under the ARGLR - ALRI GROUP LICENSE RESERVED
 * found in the LICENSE file in the root directory of this source tree
 * and at: https://github.com/alrigroup/licenses/tree/main
 */

#ifndef AROS_HAL_H
#define AROS_HAL_H

#include <stddef.h>
#include <stdint.h>

int ar_file_create(const char *directory, const char *filename, const char *content);
int ar_file_delete(const char *path);
int ar_file_exists(const char *path);

int ar_process_create(const char *path, char *const argv[]);
int ar_process_wait(int pid);
int ar_process_wait_nohang(int pid);
int ar_process_wait_status(int pid, int *exit_code);
int ar_process_kill(int pid);
int ar_process_self(void);

void *ar_process_group_create(void);
int   ar_process_group_add(void *group, int pid);
int   ar_process_group_destroy(void *group);

void *ar_module_load(const char *path);
void *ar_module_sym(void *handle, const char *name);
int   ar_module_unload(void *handle);

void *ar_thread_create(void *(*fn)(void *), void *arg);
int   ar_thread_detach(void *thread);
int   ar_thread_join(void *thread);

void *ar_mutex_create(void);
int   ar_mutex_lock(void *mutex);
int   ar_mutex_unlock(void *mutex);
void  ar_mutex_destroy(void *mutex);

void *ar_cond_create(void);
int   ar_cond_signal(void *cond);
int   ar_cond_wait(void *cond, void *mutex);
void  ar_cond_destroy(void *cond);

uint64_t ar_time_ms(void);
void     ar_sleep_ms(uint32_t ms);

int  ar_socket_create(int type);
int  ar_socket_reuseaddr(int fd, int enable);
int  ar_socket_bind(int fd, const char *addr, uint16_t port);
int  ar_socket_listen(int fd, int backlog);
int  ar_socket_accept(int fd);
int  ar_socket_connect(int fd, const char *addr, uint16_t port);
int  ar_socket_send(int fd, const void *data, size_t len);
int  ar_socket_recv(int fd, void *buf, size_t len);
int  ar_socket_set_nonblock(int fd);
int  ar_socket_set_recv_timeout(int fd, int timeout_ms);
void ar_socket_close(int fd);

int  ar_fs_mkdir(const char *path);
int  ar_fs_rmdir(const char *path);
int  ar_fs_exists(const char *path);

void *ar_mem_alloc(size_t size);
void  ar_mem_free(void *ptr);

void *ar_ssl_ctx_create(int is_server);
int   ar_ssl_ctx_use_certificate(void *ctx, const char *cert_path, const char *key_path);
void *ar_ssl_new(void *ctx, int fd);
int   ar_ssl_handshake(void *ssl);
int   ar_ssl_read(void *ssl, void *buf, int num);
int   ar_ssl_write(void *ssl, const void *buf, int num);
void  ar_ssl_free(void *ssl);
void  ar_ssl_ctx_free(void *ctx);

#define AR_EVENT_READ  1
#define AR_EVENT_WRITE 2
#define AR_EVENT_ERROR 4

typedef struct ArPoll ArPoll;

typedef struct {
    int fd;
    int events;
    void *user_data;
} ArPollEvent;

ArPoll *ar_poll_create(void);
int     ar_poll_add(ArPoll *p, int fd, int events, void *user_data);
int     ar_poll_mod(ArPoll *p, int fd, int events);
void    ar_poll_remove(ArPoll *p, int fd);
int     ar_poll_wait(ArPoll *p, ArPollEvent *events, int max_events, int timeout_ms);
void    ar_poll_destroy(ArPoll *p);

#endif
